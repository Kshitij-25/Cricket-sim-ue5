# CricketSim — Fielding System (MVP)

> Fielders REACT to the physics; they never control it. Every prediction is a
> forward integration of the real ball through the real model — no scripted
> landing points, no dice rolls.

This milestone adds a realistic fielding layer on top of the established physics
(ball flight, pitch, bat-ball collision, batting). Read `Docs/ARCHITECTURE.md`
first for the unit/axis conventions; this system inherits them and the
"outcomes emerge from physics" rule verbatim.

---

## 1. Technical design

### 1.1 The spine: a reusable, physics-true prediction core

The project already had `FCricketTrajectoryPredictor`, which integrates a *copy*
of the live ball forward through the **same** aerodynamic + pitch model the real
ball uses (so the prediction equals reality). The fielding system is built on a
thin reasoning layer over it, `FCricketFieldingPredictor`, with three pure
functions every fielder (and later AI) needs:

```
 live ball (state + integrator)
        │
        ▼
 FCricketFieldingPredictor::PredictBall ─▶ FCricketBallPrediction
   (real forward integration)               landing / apex / catch-window / path
        │                                        │
        ├── SolveIntercept(pred, fielderCaps) ─▶ FCricketInterceptResult
        │      earliest reachable point; catch vs ground; difficulty
        │
        └── SolveThrow(from, target, speed) ──▶ FCricketThrowSolution
               ballistic aim (stumps / keeper / fielder; run-out direct hit)
```

Because this layer is pure SI and free of `UWorld`/actors, it is unit-tested
headlessly and is the piece the brief earmarks for AI reuse.

### 1.2 The gameplay layer reacts

`UCricketFielderComponent` runs a state machine on a fielder pawn. Each tick it
asks the predictor for the ball's forecast and its own earliest intercept, moves
the pawn toward that point, and on contact catches/collects the ball and throws
it. A lightweight coordinator (`ACricketFieldingRig`) designates the best chaser
and supplies throw targets. The ball is only ever moved through the legitimate
physics API: `Freeze()` while held, `Release()` on the throw.

```
 UCricketFielderComponent (per fielder)
   Idle → Tracking → MovingToIntercept → (Catching | PickingUp) → Throwing
        → ReturningToPosition → Idle
        │ reads                              │ writes (physics API only)
        ▼                                    ▼
 FCricketFieldingPredictor      UCricketBallPhysicsComponent::Freeze / Release
```

### 1.3 What this layer must NOT do

No match rules, scoring, dismissily bookkeeping, commentary, crowds, replays, or
any captain/bowling/batting AI. Fielder *movement* (chase/intercept) is the
requested MVP feature, not strategy: there is no field-placement brain, just
"react to this ball." The coordinator picks the earliest-reachable chaser — a
mechanical choice, not tactics.

---

## 2. Class hierarchy

```
CricketPhysics (pure SI, headless-testable)
  FCricketFieldingPredictor ....... the Ball Prediction System (reused by AI later)
    · PredictBall    -> FCricketBallPrediction      (landing / apex / catch / path)
    · SolveIntercept -> FCricketInterceptResult      (catch vs ground vs boundary)
    · SolveThrow     -> FCricketThrowSolution         (ballistic aim / direct hit)
  (reuses) FCricketTrajectoryPredictor               (the real forward integrator)

CricketGameplay (binds the model to the world)
  UCricketFielderComponent ........ Fielding Controller + Fielder State Machine
                                    + Catch / Pickup / Throw systems
  UCricketFielderDebugComponent ... read-only visualization (cricket.Debug.Fielding)
  ACricketFielder ................. a single fielder pawn (component host)
  ACricketFieldingRig ............. coordinator + asset-free test environment
```

The six required architecture pieces map directly:

1. **Fielding Controller** → `UCricketFielderComponent`.
2. **Ball Prediction System** → `FCricketFieldingPredictor::PredictBall`.
3. **Catch System** → the `Catching` state + `SolveIntercept`'s catch classification.
4. **Pickup System** → the `PickingUp` state + ground-field classification.
5. **Throw System** → `SolveThrow` + the `Throwing` state's execution.
6. **Fielder State Machine** → the `ECricketFielderState` machine in the component.

---

## 3. Data models

All in `CricketPhysics/Public/CricketFieldingTypes.h` (SI; shared world axes).

| Type | Role |
|---|---|
| `FCricketBallPrediction` | Physics-true forecast: sampled `Path`, `LandingPointM` + `TimeToLandSec`, `ApexM`/`ApexHeightM`, `FirstBounceTimeSec` (the catch/ground split), plus `PositionAtTime`/`VelocityAtTime` helpers. |
| `FCricketInterceptQuery` | A fielder's **capabilities**: position, `MaxSpeedMS`, `ReactionTimeSec`, `ReachRadiusM`, catch/ground reach heights. |
| `FCricketInterceptResult` | The derived meeting point: `bCanIntercept`, `Kind` (Catch/GroundField/None), `Difficulty`, `PointM`, `TimeSec`, `SlackSec`, `RequiredSpeedMS`, `DistanceM`. |
| `FCricketThrowSolution` | Ballistic aim: `LaunchVelocityMS`, `FlightTimeSec`, `LaunchElevationDeg`, `bFeasible`. |
| `ECricketInterceptKind` / `ECricketCatchDifficulty` | Catch vs ground vs none; Regulation/Running/Diving/Impossible. |
| `ECricketFielderState` / `ECricketThrowTarget` | (gameplay) the seven states; stumps/keeper/nearest-fielder. |

---

## 4. State machine design

```
        ┌──────┐  ball live & I'm the chaser   ┌──────────┐
        │ Idle ├──────────────────────────────▶│ Tracking │
        └──▲───┘                                └────┬─────┘
           │ reached home                  intercept │ reachable
           │                                          ▼
 ┌─────────┴───────────┐                   ┌─────────────────────┐
 │ ReturningToPosition │◀──────────────────│  MovingToIntercept  │
 └─────────▲───────────┘   throw done      └───────┬──────┬──────┘
           │                                        │      │
           │            airborne & in reach │        │      │ grounded & in reach
           │                                ▼        ▼
        ┌──┴──────┐  secured  ┌──────────┐      ┌───────────┐  gathered
        │ Throwing │◀─────────│ Catching │      │ PickingUp ├──────────┐
        └──────────┘          └──────────┘      └───────────┘          │
             ▲                                                          │
             └──────────────────────────────────────────────────────────┘
```

- **Idle** — at home; becomes the chaser when the coordinator says so and the ball is live.
- **Tracking** — predict + intercept; if reachable → MovingToIntercept, else return.
- **MovingToIntercept** — re-predict each tick (adapts to bounce/drag), run to the
  intercept point; switch to Catching (ball airborne & within catch reach) or
  PickingUp (ball grounded & within pickup radius).
- **Catching / PickingUp** — `Freeze()` the ball and hold it at the hand; after the
  secure/gather time → Throwing.
- **Throwing** — choose a target, `SolveThrow`, `Release()` the ball; → Returning.
- **ReturningToPosition** — jog home → Idle.

Transitions are geometric: a chance is reachable, a catch is on, a ground ball is
in range — each follows from the prediction, never a random roll.

---

## 5. Prediction architecture

`SolveIntercept` walks the predicted path in time order and returns the **earliest**
sample the fielder can reach:

- A sample is a **catch** if it is airborne (before `FirstBounceTimeSec`) and within
  the fielder's catch-height band; a **ground field** if after the bounce and low
  enough to gather.
- Reachable if a top-speed run, after the reaction delay, gets within the reach
  radius: `horizDist ≤ MaxSpeed·(t − reaction) + reachRadius`.
- **Difficulty** falls out of the geometry: needing the reach radius ⇒ *Diving*;
  little time to spare ⇒ *Running*; comfortable ⇒ *Regulation*; otherwise the ball
  beats the fielder ⇒ *Impossible* (a boundary).

`SolveThrow` solves the projectile range equation for the launch angle that lands
a throw of a given speed on the target, picking the flatter arc for a run-out. The
analytic aim is exact under gravity; the actual thrown ball is then flown by the
full model, so drag is absorbed by the hit tolerance rather than scripted away.

---

## 6. Debug tooling

`UCricketFielderDebugComponent`, gated by `cricket.Debug.Fielding` (default on),
read-only. Visualizes every item the brief lists:

- **Predicted landing point** (yellow) + the full predicted ball path + apex.
- **Predicted catch / intercept point**, colour-coded (green catch / cyan ground / red none).
- **Interception path** — a line from the fielder to where it is running.
- **Throw path** — the thrown ball's flight (magenta).
- **Fielder decision state** — a colour-coded label above each fielder; the active
  chaser writes an on-screen readout (state, intercept kind/difficulty/time/slack).

The rig additionally draws the stumps, keeper, and a run-out / direct-hit status.

---

## 7. Testing strategy

Headless suite `CricketSim.Fielding`
(`CricketPhysics/Private/CricketFieldingTests.cpp`). Every test runs the **real**
predictor over a physically launched ball — no scripted landing points.

| Test | Proves |
|---|---|
| `GroundBall` | A flat shot stays low, bounces, and is **ground-fielded** by someone in its path; a distant fielder can't reach it. |
| `LoftedShot` | An aerial drive climbs and is taken as a **catch** by a fielder under where it lands. |
| `HighCatch` | A steep skyer goes very high with long hang time; the catch is reachable with positive slack. |
| `BoundarySave` | An in-range fielder saves it; an out-of-range fielder is beaten (a four). |
| `DirectHit` | The ballistic aim, flown under gravity, lands on the stumps to < 2 cm; an out-of-range target is reported infeasible (not a silent miss). |
| `ThrowReachesTarget` | The aimed throw, flown through the **full drag model**, still passes within 1 m of the target. |
| `Determinism` | The same launch predicts the same flight (no RNG). |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Fielding; Quit" -unattended -nullrhi -nosplash
# results in ~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log
```

(The pre-existing `CricketSim.Bat.EdgeImpact` failure is unrelated and predates
this milestone.)

---

## 8. Production-ready C++ — file map

| File | Module | Contents |
|---|---|---|
| `CricketFieldingTypes.h` | CricketPhysics/Public | Data models (§3). |
| `CricketFieldingPredictor.h/.cpp` | CricketPhysics | Predict / Intercept / Throw (§5). |
| `CricketFieldingTests.cpp` | CricketPhysics/Private | The `CricketSim.Fielding` suite (§7). |
| `CricketFielderComponent.h/.cpp` | CricketGameplay | Fielding Controller + state machine + catch/pickup/throw (§4). |
| `CricketFielderDebugComponent.h/.cpp` | CricketGameplay | Debug overlay (§6). |
| `CricketFielder.h/.cpp` | CricketGameplay | A fielder pawn (component host). |
| `CricketFieldingRig.h/.cpp` | CricketGameplay | Coordinator + asset-free test environment. |

### Trying it in the editor

Drop an `ACricketFieldingRig` into any level and press Play. Press **1–5** to play
a shot (ground / lofted / skyer / boundary / run-out); **L/R** to aim it; **T** to
toggle run-out mode (fielders throw at the stumps); **R** to reset. Watch the
chaser light up, run to the predicted intercept, take the catch or field it, and
throw — with the prediction, intercept, and throw paths all drawn live.

---

## 9. Boundaries & future work

- **In scope, done:** ball tracking (landing/intercept/catch prediction), ground
  fielding (chase/intercept/pickup/return), catching (stationary/moving/high/low),
  throwing (stumps/keeper/fielder), run-outs (collect/aim/hit), the state machine,
  debug, tests, and full physics integration.
- **Deliberately untouched:** the ball/pitch/collision/batting cores — fielding
  only *reads* their output and returns the ball via the physics API.
- **Future:** drop/fumble probability as a function of difficulty (currently a
  reachable chance is taken cleanly), dive animations, throw-on-the-run release
  timing, multi-fielder backing-up, and the run/dismissal bookkeeping that belongs
  to the match-rules phase. The prediction core is ready to be reused by AI batters
  and the captain's field-placement logic.
```
