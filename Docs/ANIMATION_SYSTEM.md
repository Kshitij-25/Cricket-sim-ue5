# CricketSim — Animation System

> Input → Animation → Physics → Result. The animation layer **visualizes** what the
> simulation is doing and **times** when one physics event happens (the ball
> release). It never decides an outcome. The physics systems remain the source of
> truth.

Read `Docs/ARCHITECTURE.md` first. This layer sits *on top of* the existing
gameplay components (`UCricketBattingComponent`, `UCricketFielderComponent`,
`UCricketBowlingComponent`) and the physics core — it reads them, it does not
replace or override them.

---

## 1. Animation architecture

The project is deliberately **asset-free** (the rigs are debug-drawn; no skeletal
meshes or Anim Blueprints are authored in C++). So the production-ready,
*testable* deliverable is the **C++ controller + state-machine + notify-timeline
layer that a real Anim Blueprint sits on**, plus the documented AnimBP structure
(§3). This is the same pattern the rest of the project uses: a pure, headless core
with a thin gameplay/presentation layer over it.

Two responsibilities, matching the philosophy:

1. **Derive** believable visual state from the sim — locomotion from the pawn's
   movement, and the bowling/batting/fielding action states by **following** the
   existing gameplay components.
2. **Time** exactly one physics event — the bowling run-up's **BallRelease** notify
   tells the bowling system *when* to release. The flight that follows is pure
   physics. Nothing else in animation gates an outcome.

```
 Input ──▶ UCricketCharacterAnimComponent ──(BallRelease notify)──▶ Bowling::BowlNow()
              │  derives                                                 │
              │  · locomotion  (from pawn velocity)                      ▼
              │  · bowling state (its own run-up montage)         delivery generator
              │  · batting state (FOLLOWS UCricketBattingComponent)      │
              │  · fielding state (FOLLOWS UCricketFielderComponent)     ▼
              ▼                                                    BALL PHYSICS (truth)
   motion data the Anim Blueprint reads (gait, states, release pose, bat data)
```

Why "follow", not "drive"? The batting and fielding systems already advance their
own timelines and resolve contact via physics (the swing meets the ball
geometrically; the fielder catches/throws in its state machine). Animation reads
their state and presents it, so a mistimed shot or a dropped chance is never
papered over by a canned animation.

---

## 2. State machine design

Four state machines (`CricketAnimationTypes.h`), each a `UENUM`:

| Machine | States |
|---|---|
| **Locomotion** | Idle · Walk · Jog · Sprint · Turn · Stop |
| **Bowling** | Idle · RunUp · DeliveryStride · Release · FollowThrough · Recover |
| **Batting** | Guard · Backlift · Downswing · Impact · FollowThrough · Recover |
| **Fielding** | Idle · Run · GroundStop · Pickup · Catch · Throw · ReturnToPosition |

- **Locomotion** is classified from planar speed (+ turn rate and along-track
  acceleration for Turn/Stop) by `FCricketAnimationModel::ClassifyLocomotion`.
- **Bowling** runs as a timed montage (`MakeBowlingMontage`) — the only machine the
  animation *owns* end-to-end, because the run-up is new timing the sim didn't have.
- **Batting** maps 1:1 from the swing model's `ECricketSwingPhase` (`MapBattingPhase`).
- **Fielding** maps from `ECricketFielderState`.

### The notify-timeline engine

Every action is a `FCricketActionMontage`: a list of timed phases carrying
`FCricketAnimNotifyDef` events. `FCricketMontagePlayer` advances it and emits each
notify **exactly once** as its time is crossed. This one deterministic engine
powers all action timing — release, bat impact, catch, throw, pickup, footfalls —
and is exactly what the timing tests exercise.

---

## 3. Animation Blueprint structure (the layer a designer builds)

The C++ controller exposes everything an Anim Blueprint needs, so the AnimBP is a
thin presentation layer:

```
ABP_CricketCharacter (UAnimInstance)
  • reads UCricketCharacterAnimComponent each frame:
      GetLocomotionState() / GetLocomotion().GaitBlend   -> Locomotion state machine + blendspace
      GetBowlingState()                                  -> Bowling layer (additive on upper body)
      GetBattingState()  + GetBatSpeedMS()/FaceNormal    -> Batting layer
      GetFieldingState()                                 -> Fielding layer
      GetReleaseOffsetM()/GetWristAxis()/ReleaseTime     -> release pose (IK target / hand)
  • Layered blend per bone (Character Animation Layers):
      Base   = locomotion (full body)
      Upper  = action layer (bowling arm / bat swing / catch-throw), blended by action state
  • AnimNotify_BallRelease / _BatImpact / _CatchAttempt / _ThrowRelease are placed in
    the montages and forwarded to gameplay via OnAnimNotify (already wired in C++).
```

**Root motion strategy:** locomotion uses **in-place** animation with capsule-driven
movement (the gameplay components move the pawn kinematically; animation must not
fight them) — so animation is *driven by* velocity, never the reverse. The bowling
run-up and fielding dives use **baked root motion only for cosmetic body travel
that does not affect the ball**; the release point and the ball's launch come from
the bowling system, not the root-motion curve. This keeps physics authoritative.

**Motion data architecture:** all pose-driving data is plain SI structs
(`FCricketLocomotionSample`, `FCricketBowlingActionTimeline`, the montage types) —
data, not logic — so gaits and action timings are tunable without recompiling and
are shared by every character.

---

## 4. Gameplay integration plan

| Hook | Source | Effect |
|---|---|---|
| **BallRelease** notify | bowling run-up montage | `ACricketBowlingRig` binds it → `UCricketBowlingComponent::BowlNow()`. Input starts the run-up; the notify releases the ball. |
| **BatImpact** notify | `OnShotPlayed` (batting) | Emitted coincident with the geometric bat-ball contact the batting system resolved. |
| **CatchAttempt / PickupContact** | `OnStateChanged` (fielder) | Emitted as the fielder enters Catching / PickingUp. |
| **ThrowRelease** notify | `OnThrew` (fielder) | Emitted when the fielder releases its throw. |

The integration is live in `ACricketBowlingRig`: pressing **Space** now starts the
run-up animation, and the BallRelease notify fires the delivery — a working
Input → Animation → Physics loop. The batting/fielding rigs already carry the
components needed; the anim controller auto-finds the batting/fielder components on
its owner and follows them.

---

## 5. Debug tooling

`UCricketAnimDebugComponent`, gated by `cricket.Debug.Anim` (default on), read-only.
Visualizes everything the brief lists:

- **Current animation state** — locomotion / bowling / batting / fielding readouts.
- **State transitions** — the live dominant-state label above the character.
- **Notify timing** — a rolling list of recent notifies with their fire times; the
  newest flashes.
- **Bat path** — the swing trail (when a batter).
- **Release timing** — the bowling action time vs the scheduled release, + wrist axis.
- **Movement speed** — the locomotion speed and gait blend.

---

## 6. Testing strategy

Headless suite `CricketSim.Anim` (`CricketPhysics/Private/CricketAnimationTests.cpp`).
The state machines and the notify engine are pure, so the timing is tested without
any skeletal mesh — the same model the AnimBP runs.

| Test | Proves |
|---|---|
| `Locomotion` | Speed (+ turn/decel) selects Idle/Walk/Jog/Sprint/Turn/Stop; gait blend rises with speed. |
| `BowlingReleaseTiming` | BallRelease fires once, at the action's release time. |
| `BatCollisionTiming` | BatImpact fires once, at the end of the downswing (the contact instant). |
| `CatchAndThrowTiming` | CatchAttempt / ThrowRelease / PickupContact fire on cue. |
| `StateTransitions` | The bowling action visits RunUp→Stride→Release→FollowThrough→Recover in order and the player completes. |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Anim; Quit" -unattended -nullrhi -nosplash
# results in ~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log
```

(The pre-existing `CricketSim.Bat.EdgeImpact` failure is unrelated and predates this work.)

---

## 7. Production-ready C++ — file map

| File | Module | Contents |
|---|---|---|
| `CricketAnimationTypes.h` | CricketPhysics | The four state machines + the montage/notify engine + locomotion & bowling data. |
| `CricketAnimationModel.h/.cpp` | CricketPhysics | Locomotion classifier + action montage builders + batting-phase map. |
| `CricketAnimationTests.cpp` | CricketPhysics/Private | The `CricketSim.Anim` suite (§6). |
| `CricketCharacterAnimComponent.h/.cpp` | CricketGameplay | The AnimController the AnimBP reads (§1). |
| `CricketAnimDebugComponent.h/.cpp` | CricketGameplay | Debug overlay (§5). |
| `CricketBowlingRig.{h,cpp}` (edited) | CricketGameplay | Input → run-up → BallRelease → `BowlNow` integration (§4). |

### Trying it in the editor

Drop an `ACricketBowlingRig` into a level and press Play. Press **Space**: the
bowler runs up, and at the release point the BallRelease notify fires the delivery
— the `cricket.Debug.Anim` overlay shows the bowling state advancing, the release
timing/wrist axis, and the notify flashing exactly when the ball leaves the hand.
The batting and fielding rigs show their action states and the bat path; all of it
follows the sim, never the other way round.

---

## 8. Boundaries & future work

- **In scope, done:** the four state machines, the notify-timeline engine, the
  AnimController, locomotion derivation, the bowling run-up that times the release,
  batting/fielding state following, all the gameplay notify hooks, debug, tests, and
  a live Input→Animation→Physics integration.
- **Deliberately not done:** authored skeletal-mesh assets, Anim Blueprints, and
  animation sequences (asset content, not code), and any presentation polish — the
  AnimBP structure is documented (§3) for a designer to build on this controller.
- **Future:** retarget-ready skeleton + motion captures behind these states, IK for
  the release hand and bat grip, dive/slide root-motion clips, and blendspace tuning
  — none of which change that physics stays the source of truth.
```
