# CricketSim — Batting System

> Player input controls intent. Physics determines outcome. A well-timed shot is
> *earned* by bringing the sweet spot to the ball, not awarded by a script.

This milestone adds the batting **input layer** on top of the established
physics. It decides only **how the bat moves**; the existing
`FCricketBatCollision` still resolves every contact and the existing
`UCricketBallPhysicsComponent` still flies the ball. Read `Docs/ARCHITECTURE.md`
first — the unit/axis conventions and the "outcomes emerge from physics" rule
below are inherited from it verbatim.

---

## 1. Technical design

### 1.1 The core idea

The project already had two pure functions for the bat:

- `FCricketShotGenerator::GenerateBatState` — intent → a single bat state, and
- `FCricketBatCollision::Resolve` — a contact → the outcome.

But the generator *teleports* the bat onto the ball: it positions a sweet spot at
the ball and injects timing error as an explicit number. That is fine for unit
tests, but it is not a batting *experience* — nothing about it is earned.

This milestone adds the missing dimension: **time**. The bat is animated through a
real **backlift → downswing → follow-through** over ~0.4 s, and contact is found
**geometrically** when the moving bat crosses the live ball. Whether you middle it
depends on whether your downswing actually brought the sweet spot to where the
ball is, at the instant it is there. Mishits, edges, and the Early/Perfect/Late
verdict all **emerge from that geometry** — the swing layer injects nothing.

```
 Player intent (shot, footwork, aim, power, WHEN you press)
        │
        ▼
 FCricketSwingModel  ── BuildProfile ─▶ kinematic template (backlift/down/follow)
   (pure, SI, tested) ─ EvaluateBat ──▶ bat FCricketBatState at swing-time t
                       ─ DetectContact ▶ where the moving bat met the ball  ┐
        │                                                                   │
        ▼                                                                   │
 UCricketBattingComponent  (per tick: advance swing, sweep for contact) ◀───┘
        │  on contact:
        ▼
 UCricketBallPhysicsComponent::ApplyBatImpact  ──▶  FCricketBatCollision::Resolve
   (UNCHANGED existing physics — decides exit speed, spin, deflection, energy)
```

### 1.2 What this layer must NOT do

No match rules, fielding, AI, commentary, replays, crowds, or presentation. The
batting layer never reads or writes a "runs" or "out" value, and never chooses an
outcome. Its entire output is an `FCricketBatState` (where the bat is and how fast
it is moving) plus a contact point — exactly the inputs the collision solver
already consumes.

---

## 2. Data models

All in `CricketPhysics/Public/CricketBattingTypes.h` (SI; axes per the architecture
doc: `+X` down the pitch toward the striker, `+Y` off side for a RH bat, `+Z` up;
left-handers mirror in `Y`).

| Type | Role |
|---|---|
| `ECricketFootwork` | `Neutral` / `FrontFoot` / `BackFoot` — sets reach & the contact zone. |
| `ECricketSwingPhase` | `Idle` / `Backlift` / `Downswing` / `Contact` / `FollowThrough` / `Recovery`. |
| `ECricketTimingQuality` | `TooEarly` … `Perfect` … `TooLate` — a **read-out**. |
| `FCricketBattingInput` | The whole control surface: shot, footwork, handedness, `AimYawDeg`, `PowerScale`. Timing is **not** a field — it is *when* you trigger relative to the ball. |
| `FCricketSwingProfile` | Per-stroke kinematic template after footwork/handedness fold in: backlift/contact/follow offsets, face aim, arc, peak bat speed, phase durations. |
| `FCricketTimingResult` | Verdict: `Quality`, signed `TimingErrorSec` (>0 late, matching `FCricketShotIntent`), normalized score. |
| `FCricketContactSolution` | The bridge to physics: `BatAtContact`, world `ContactPointM`, closing speed, phase, timing. |

The existing `FCricketBatState`, `FCricketBatProfile`, `FCricketBatImpactReport`,
and `ECricketShotType` (the four MVP shots) are reused unchanged from
`CricketBatTypes.h`.

---

## 3. Class hierarchy

```
CricketPhysics (pure SI, headless-testable)
  FCricketSwingModel ............ Bat Motion + Timing Evaluation + contact detection
    · BuildProfile(shot, footwork, handed) -> FCricketSwingProfile
    · EvaluateBat(profile, input, stanceOrigin, t)  -> FCricketBatState  (+ phase, speed)
    · DetectContact(profile, input, stance, batProfile, ball0, ball1, t, dt, substeps)
    · ClassifyTiming(profile, contactSwingTime)     -> FCricketTimingResult
  (reuses) FCricketBatCollision::Resolve            -> outcome  (UNCHANGED)

CricketGameplay (binds the model to the world)
  UCricketBattingComponent ...... Batting Controller / Shot Intent / Animation layer
  UCricketBattingDebugComponent . read-only visualization (cricket.Debug.Batting)
  ACricketBattingRig ............ self-possessing striker pawn + bowling-component feeder
```

The six architecture pieces the brief asked for map cleanly:

1. **Batting Controller** → `UCricketBattingComponent`.
2. **Shot Intent System** → `FCricketBattingInput` + its setters on the component.
3. **Bat Motion System** → `FCricketSwingModel::BuildProfile` / `EvaluateBat`.
4. **Timing Evaluation System** → `DetectContact` + `ClassifyTiming`.
5. **Footwork System** → `ECricketFootwork` folded into the profile (reach, contact zone, balance).
6. **Animation Integration Layer** → the component drives an optional bat `USceneComponent` from the live `FCricketBatState` each tick (an AnimBP/mesh can follow it).

---

## 4. Input architecture (macOS, keyboard + mouse)

Polled from the possessing `PlayerController` each tick — deliberately asset-free
(no Enhanced Input mapping context required), identical in spirit to
`ACricketBowlingRig`. The control **scheme** is what a shipping build would port to
Input Actions.

| Input | Action |
|---|---|
| **Space / LMB** | Play the selected stroke — **this is the timing input**. |
| **1 / 2 / 3 / 4** | Shot: Defensive Block / Straight Drive / Cover Drive / Pull. |
| **Up / Down** | Footwork: front foot / back foot. |
| **N** | Neutral stance. |
| **Left / Right / Mouse-X** | Aim trim toward leg / off (`AimYawDeg`). |
| **Q / E** | Power − / +. |
| **F** | Feed a delivery (the feeder bowls a real ball at you). |
| **[ / ]** | Feed length: fuller / shorter. |
| **Tab** | Cycle feeder (quick / swing / off-spin / leg-spin). |
| **R** | Fresh ball. |

The five required input concerns are all covered: **shot selection** (1–4),
**shot direction** (aim trim/mouse), **footwork selection** (Up/Down/N), **timing
window** (when you press Space vs when the ball arrives), and **defensive actions**
(shot 1 / `Defend()`).

---

## 5. Batting architecture (how a stroke resolves)

1. **Trigger.** `TriggerSwing()` builds the active `FCricketSwingProfile` and starts
   the swing clock at 0 (the downswing). The backlift is assumed already set.
2. **Per tick (`TG_PostPhysics`, after the ball has flown).** Sample the bat with
   `EvaluateBat` for animation/debug, then `DetectContact` over the ball's swept
   segment `[prevPos, curPos]` this frame. The interval is **sub-stepped** so a fast
   downswing still resolves the crossing at a coarse frame rate.
3. **Contact.** When the ball reaches the bat face within the blade while closing,
   the component hands `BatAtContact` + the contact point to
   `UCricketBallPhysicsComponent::ApplyBatImpact` — i.e. the **existing** solver
   launches the ball. `OnShotPlayed` broadcasts the impact report + timing verdict.

**Bat movement modelled:** backlift (apex offset), downswing (eased sweet-spot path
to the contact zone with bat speed accelerating to a peak at contact), follow-
through (continuing arc, bat speed bleeding off), **bat face angle** (`FaceNormalAim`
rotated by `AimYawDeg`), **swing path** (the `LongAxis`/velocity recipe shared with
the shot generator), and **bat speed** (per-shot peak × `PowerScale`).

**Timing influences** — all emergent, none injected:
- *Contact point*: a late swing meets the ball before the sweet spot arrives →
  contact slides toward the toe; an early swing meets it past the sweet spot.
- *Bat angle at impact*: the face the ball meets is wherever the swing put it.
- *Exit trajectory*: a worse contact point → lower effective mass & restitution in
  the **unchanged** collision solver → slower, squirted exit.

**Footwork influences:**
- *Reach / contact location*: front foot pushes the contact zone down the pitch
  (−X) and a touch lower (the fuller ball); back foot sits deeper (+X) and higher
  (room for the short ball).
- *Shot quality / balance*: matching the foot to the length puts the sweet spot on
  the ball; the wrong foot displaces it — the punishment is geometric, not a rule.

---

## 6. Debug tooling

`UCricketBattingDebugComponent`, gated by `cricket.Debug.Batting` (default on),
read-only. It visualizes everything the brief asked for:

- **Bat path** — the sweet-spot trail through the swing, coloured by bat speed.
- **Bat speed** — arrow + on-screen km/h readout.
- **Contact timing** — `Early/Perfect/Late` + signed millisecond error, colour-coded.
- **Impact location** — region-coloured contact sphere + exit-velocity arrow.
- **Foot position** — front/back/neutral foot markers + stance origin; the loaded
  foot is highlighted.
- **Swing plane** — the bat face plane swept through the contact zone.

It complements the existing `UCricketBatDebugComponent` (which draws the resolved
*contact* on the ball); together they show intent → motion → contact → outcome.

---

## 7. Testing strategy

Headless suite `CricketSim.Batting`
(`CricketPhysics/Private/CricketBattingTests.cpp`). Each test runs the **real**
pipeline: a constant-velocity ball is stepped toward the striker exactly as the
component ticks, `DetectContact` finds the crossing, and the **actual**
`FCricketBatCollision::Resolve` produces the outcome — so the tests prove the whole
chain, not a mock.

| Test | Proves |
|---|---|
| `PerfectTiming` | Sweet spot meets the ball → Middle, fast, driven back. |
| `EarlyTiming` | Swing ahead → classified Early, off-centre, worse exit. |
| `LateTiming` | Swing behind → classified Late, off-centre, worse exit. |
| `FrontFootDrive` | Front foot middles a full ball; back foot to it is worse. |
| `BackFootShot` | Back foot pulls a short ball to leg; front foot to it is worse. |
| `DefensiveShot` | Contact made, soft exit, kept down — far softer than a drive. |
| `Determinism` | Same swing + ball → bit-identical exit & contact point. |
| `TimingSweep` | Sweeping the trigger early→late moves the timing error monotonically through zero. |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Batting; Quit" -unattended -nullrhi -nosplash
# results in ~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log
```

(The pre-existing `CricketSim.Bat.EdgeImpact` failure is unrelated bat-collision
tuning and predates this milestone.)

---

## 8. Production-ready C++ — file map

| File | Module | Contents |
|---|---|---|
| `CricketBattingTypes.h` | CricketPhysics/Public | Enums + structs (§2). |
| `CricketSwingModel.h/.cpp` | CricketPhysics | Bat Motion + Timing + contact detection (§3). |
| `CricketBattingTests.cpp` | CricketPhysics/Private | The `CricketSim.Batting` suite (§7). |
| `CricketBattingComponent.h/.cpp` | CricketGameplay | Batting Controller / Shot Intent / Animation layer (§5). |
| `CricketBattingDebugComponent.h/.cpp` | CricketGameplay | Debug overlay (§6). |
| `CricketBattingRig.h/.cpp` | CricketGameplay | Playable striker pawn + feeder (§4). |

### Trying it in the editor

Drop an `ACricketBattingRig` into any level and press Play. Press **F** to feed a
delivery, pick a shot (**1–4**) and footwork (**Up/Down**), then time **Space** to
the ball. The `cricket.Debug.Batting` overlay shows your bat path, speed, foot, the
swing plane, and — once you connect — the timing verdict and contact quality. The
same delivery, met a few milliseconds early or late or off the wrong foot, gives a
visibly different (and never scripted) result.

---

## 9. Boundaries & future work

- **In scope, done:** the four MVP strokes, footwork, timing, bat motion, debug,
  tests, full physics integration.
- **Deliberately untouched:** the ball-flight, pitch, bowling, and bat-collision
  cores — batting only *feeds* them.
- **Future:** rotating the bat through the downswing (non-zero `AngularVelocity`
  for blade-point speed variation), more strokes (cut/flick/sweep/loft) on the same
  profile machinery, stumps/bowled detection (a rules concern, out of scope here),
  and migrating the polled control scheme to Enhanced Input assets.
```
