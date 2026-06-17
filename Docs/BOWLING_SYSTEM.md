# CricketSim — Bowling System

> Phase 2 of the roadmap. Turns human/AI **intent** into physically-grounded
> **release conditions** that feed the existing deterministic ball-physics core.
> It is a *generator of physical delivery conditions*, **not** a scripted
> animation system: swing, dip, drift, seam movement, reverse and turn all emerge
> from `CricketPhysics` exactly as they do for any other ball state.

---

## 1. Technical design

The bowling system sits entirely *above* the physics core and writes into it
through one boundary: `UCricketBallPhysicsComponent::ReleaseEx(...)`. It never
touches the aerodynamic or pitch model.

```
 Intent (line/length/pace/swing/spin/movement)
        │            ┌──────────────────────────── CricketPhysics (pure, SI) ───────────────────────────┐
        ▼            │                                                                                    │
 UCricketBowlingComponent ──► FCricketDeliveryGenerator ──► FCricketReleaseParameters                     │
   (controller, cm↔m)            (intent → physics,            (the 8 release params + model inputs)       │
        │                         aim solve via predictor)             │                                  │
        │                                                              ▼                                  │
        └────────────────────────────────────────────► UCricketBallPhysicsComponent::ReleaseEx ──► RK4 ──┤
                                                          (sets FCricketBallState; surface/coeffs/env)     │
                                                                                                          └─►
```

Key decisions:

* **Single physical contract.** A delivery is fully described by
  `FCricketReleaseParameters` (the eight release parameters the brief lists, plus
  the exact quantities the integrator consumes: angular velocity, seam stability,
  per-delivery coefficients). The controller maps these onto the ball; nothing
  else is needed.
* **Length is solved, not guessed.** The chosen length band is converted to a
  release **elevation** by bisecting against the *real* `FCricketTrajectoryPredictor`
  until the ball pitches in the band. The prediction uses the same integrator the
  live ball will fly through, so the planned pitch point equals the actual one.
* **Movement emerges.** Out/in/reverse swing, wobble, off/leg turn are produced by
  authoring the **seam orientation, spin axis/rate, seam stability, surface and the
  per-delivery swing-transition speed** — never by adding a sideways nudge to the
  result. Reverse swing in particular is *emergent*: the reverse delivery presents
  an away-seam on a scuffed ball at pace, and the model's regime flip tails it back
  in. (See §4.)
* **Determinism preserved.** The generator is a pure function of
  `(intent, action, context)`. Human inaccuracy is seeded scatter applied to the
  *inputs only* — exactly the project's standing rule.

---

## 2. Class hierarchy

```
CricketPhysics (Runtime, pure SI — no UWorld/actors, headlessly testable)
  CricketBowlingTypes.h        enums + FCricketBowlingIntent / FCricketReleaseParameters
                               / FCricketDeliveryContext / FCricketDeliveryDiagnostics
                               / FCricketBowlingAction / FCricketDeliveryPreset
                               + namespace CricketField (pitch geometry)
  FCricketDeliveryGenerator    intent + action + context → release parameters (+ aim solver)
  UCricketBowlingActionAsset   UPrimaryDataAsset wrapping FCricketBowlingAction + presets
                               + static built-in bowlers (quick / swing / off / leg)

CricketGameplay (Runtime, actors/components — binds physics to the world)
  UCricketBallPhysicsComponent +ReleaseEx(...)  (existing class, one added method)
  UCricketBowlingComponent     the Bowling Controller (intent, ageing, BowlNow, OnDelivery)
  UCricketBowlingDebugComponent every required debug overlay + the pitch map
  ACricketBowlingRig           APawn training environment (spawns ball, owns the
                               above, polls the keyboard+mouse control scheme, camera)
```

`FCricketDeliveryGenerator` is the bowling analogue of the existing
`FCricketShotGenerator`; `UCricketBowlingComponent` mirrors how
`UCricketBallPhysicsComponent::PlayShot` calls a pure generator then resolves it.

---

## 3. Data models

### The eight release parameters (`FCricketReleaseParameters`, all SI)

| # | Field | Meaning |
|---|---|---|
| 1 | `ReleaseSpeedMS` / `ReleaseVelocityMS` | release velocity (magnitude + world vector) |
| 2 | `ReleasePositionM` | release position (m, world) |
| 3 | `ReleaseElevationDeg` / `ReleaseAzimuthDeg` | release angle (elevation + azimuth) |
| 4 | `SeamNormal` | seam orientation (unit seam-plane normal) |
| 5 | `SpinAxis` | spin axis (unit) |
| 6 | `SpinRateRPM` | spin rate |
| 7 | `WristPosition` | wrist/hand position (enum) |
| 8 | `BallCondition` | shine / roughness / seam (`FCricketBallSurface`) |

Plus the derived model inputs: `AngularVelocityRadS`, `SeamStability`,
per-delivery `Coefficients` (wobble rate/amplitude, swing-transition speed), and
an `Archetype` hint.

### Intent & action

* `FCricketBowlingIntent` — the five MVP control axes (`Line`, `Length`, `Pace01`,
  `SwingAmount`, `SpinAmount`) plus `Movement` and fine line/length trims.
* `FCricketBowlingAction` — the bowler's physical envelope (release height/width,
  arm slot, pace range, max RPM, stock backspin, held-seam stability) and the
  named presets they offer.
* `FCricketDeliveryContext` — world conditions (release point, striker stumps,
  environment, ball condition, seed, scatter).
* `FCricketDeliveryDiagnostics` — physics-derived prediction (pitch point, length,
  line, free-flight swing, regime, aim convergence) for debug & tests.

### Coordinate & unit conventions (shared with the physics core)

`+X` down the pitch toward the striker, `+Y` the off side for a right-hander,
`+Z` up; SI metres/seconds/radians. The only cm↔m conversion is in the controller.
Field geometry lives in `namespace CricketField` (pitch length 20.12 m, release-to-
stumps 17.7 m, stump height 0.711 m, etc.) — kept out of the pure aero model.

---

## 4. Bowling architecture — how each delivery is produced

All directions below are for a right-arm bowler to a right-handed striker.

| Delivery | Mechanism authored | Emergent result |
|---|---|---|
| **Yorker / Good length / Short / Bouncer** | length band → release elevation solved against the predictor | pitches in band; bouncer rears above the stumps at the striker |
| **Outswinger** | shiny ball, seam canted toward `+Y`, swing-transition raised to 42 m/s so the regime stays conventional at pace | side force toward `+Y` (away from the bat) |
| **Inswinger** | seam canted toward `−Y`, transition 42 m/s | side force toward `−Y` (into the bat) |
| **Reverse swing** | rough ball (roughness ≈ 0.9, dull shine), seam presented like an *away* seam, transition left low (28 m/s) | speed + roughness tip into the **reverse regime**; the side force flips and the ball tails **in** — emergent, same seam as a conventional out-swinger |
| **Wobble seam** | scrambled seam: low `SeamStability` (~0.15) + `WobbleSeamRate/Amplitude` coefficients | seam precesses in flight → late, inconsistent movement |
| **Off break** | spin axis ≈ `(+X, +Y, +Z)`, finger-spin RPM, seam canted to aid the turn | grips and turns `−Y` (into the bat); dips (topspin) and drifts to off |
| **Leg break** | spin axis ≈ `(−X, +Y, −Z)`, wrist-spin RPM | grips and turns `+Y` (away); dips and drifts to leg |

These match the physics core's unit-tested sign conventions
(`OutswingSeamNormal` is `+Y`-biased; spin about `+X` turns `−Y`; backspin lifts;
topspin dips; side-spin `+Z` drifts `+Y`).

### The aim solver

`Length → elevation` is a bounded bisection (≈24 iterations) on a monotone
relationship (more elevation ⇒ fuller). Each probe integrates the *full* authored
state (so backspin carry / topspin dip is accounted for) to its first bounce and
measures the length from the striker. `Line → azimuth` is direct geometry (aim the
straight release line through the chosen channel at the stumps); swing/spin then
deviate the ball off that line — the deviation is reported as `FreeFlightSwingM`.

---

## 5. Input design (macOS keyboard + mouse, MVP)

Implemented on `ACricketBowlingRig` by polling the possessing `PlayerController`
each tick — deliberately asset-free so it works the instant the pawn is placed in
a level. (A shipping build would migrate the same scheme to Enhanced Input assets.)

| Input | Action | Maps to |
|---|---|---|
| `Space` / `LMB` | bowl | `BowlNow()` |
| `↑` / `↓` | length fuller / shorter | `StepLength(∓1)` |
| `←` / `→` | line toward leg / off | `StepLine(∓1)` |
| mouse move | fine aim (X = line, Y = length) | `SetLineFineM` / `SetLengthFineM` |
| wheel / `[` `]` | pace − / + | `AdjustPace(±)` |
| `Q` / `E` | swing amount − / + | `AdjustSwing(±)` |
| `Z` / `C` | spin amount − / + | `AdjustSpin(±)` |
| `M` | cycle movement archetype | `SetMovement` |
| `Tab` | cycle bowler (quick/swing/off/leg) | `SetAction` |
| `1`–`8` | select the bowler's preset deliveries | `SelectPreset` |
| `-` | scuff the ball (enables reverse) | `AgeBall(+0.1)` |
| `R` | fresh ball | `ResetBall()` |

The five required axes — **line, length, pace, swing amount, spin amount** — are
each directly controllable, plus movement/preset/ball-condition selection.

---

## 6. Debug tools (`UCricketBowlingDebugComponent`, CVar-gated)

`cricket.Bowl.Debug.Enable` (master) + `.PitchMap` / `.Swing` / `.Readout`.

* **Release point** — sphere at the release position.
* **Seam orientation** — the seam-plane ring + normal at release.
* **Spin axis + RPM** — double-ended axis line with an rpm label.
* **Swing prediction** — a straight chord release→pitch vs the curved predicted
  path through the real model; the gap is the swing/dip, annotated in metres.
* **Actual trajectory** — the live flight trail.
* **Bounce location** — predicted pitch point and the latest actual landing.
* **Pitch map** — length-zone bands, the stumps, and every ball's landing colour-
  coded by length.
* **Readout** — movement/length/line, pace, elevation/azimuth, wrist, spin rpm,
  seam stability, ball condition, regime, and the predicted length/line/swing.

---

## 7. Testing strategy

Headless automation tests in `CricketPhysics` (`CricketSim.Bowling.*`), run with:

```sh
"$ENGINE/Binaries/Mac/UnrealEditor-Cmd" "$PWD/CricketSim.uproject" \
    -ExecCmds="Automation RunTests CricketSim.Bowling; Quit" -unattended -nullrhi
```

Each test generates a delivery and asserts its **emergent** flight through the
shared model:

| Test | Asserts |
|---|---|
| `Determinism` | identical inputs (+ seed) reproduce identical release params |
| `Yorker` | pitches < 2.5 m from the stumps; aim converged |
| `GoodLength` | pitches in [4, 8] m |
| `Bouncer` | pitches short **and** clears stump height at the striker (and is higher there than a good-length ball) |
| `Outswing` / `Inswing` | free-flight swing `+Y` / `−Y`, conventional regime |
| `ReverseSwing` | reverse regime, tails `−Y`, and is **opposite** a conventional away-seam at the same pace |
| `OffSpin` / `LegSpin` | turn `−Y` / `+Y` across a bounce on a gripping pitch |
| `WobbleSeam` | low release stability, precession enabled, seam drifts far more in flight than a held seam |

Because the generator is pure and deterministic, these run with no world or
actors and guard the physical guarantees the rest of the game will rely on.

---

## 8. Integration notes

* `UCricketBallPhysicsComponent` gained one method, `ReleaseEx(...)`, identical to
  `Release(...)` but also seeding seam stability; `Release(...)` now delegates to
  it with stability 1.0. Fully backward compatible.
* The controller writes the delivery's `Coefficients`, `Surface`, `Environment`
  and `PitchSurface` onto the ball immediately before `ReleaseEx`, so the live
  flight uses exactly the conditions the generator solved against.
* **Pitch plane consistency.** The live ball resolves bounces by sweeping real
  `WorldStatic` geometry, while the aim solver and the predicted overlays resolve
  against an analytic plane. The controller derives that plane's height from the
  striker's ground level (`GetGroundPlaneZM`) and feeds it into both the generator
  (`FCricketDeliveryContext::GroundPlaneZM`) and the debug prediction, so the
  planned pitch point matches the actual one regardless of the rig's world Z.
* The `ACricketBowlingRig` carries its own thin `WorldStatic` collision slab down
  the pitch (top face at the rig's ground level), so the live ball bounces and the
  actual landing marks register even in an otherwise empty level — it genuinely
  works the instant the pawn is dropped in. It self-possesses Player 0 and spawns
  its own ball. Or add a `UCricketBowlingComponent` to any actor that references an
  `ACricketBall` and call `BowlNow()` from code (providing your own pitch floor).
```
