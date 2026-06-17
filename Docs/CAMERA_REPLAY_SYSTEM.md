# CricketSim — Camera & Replay System

> Cameras frame the simulation; replays play back its recorded results. Neither
> ever moves, re-computes, or scripts anything in the physics. A replay shows
> exactly what the simulation did.

Read `Docs/ARCHITECTURE.md` first. This layer sits on top of the physics, fielding,
animation and match systems and only reads them.

---

## 1. Camera architecture

A pure **framing model** (`FCricketCameraModel`) computes a camera pose for any
mode from the positions it is filming (the ball, the two pitch ends, the active
fielder) plus per-mode tuning. The gameplay **Camera Manager**
(`UCricketCameraDirectorComponent`) calls it each frame, smoothly **blends** on
mode changes, and applies the result to a `UCameraComponent`. Because the math is
pure, the framing and the transitions are headless-testable.

```
 live subjects (ball / stumps / fielder)  ─▶ FCricketCameraModel::ComputePose(mode)
                                                      │  (per mode)
        UCricketCameraDirectorComponent ── Blend ─────┤  (transitions)
                                                      ▼
                                            UCameraComponent (pose + FOV)
```

### Camera modes

| Mode | Framing | Serves |
|---|---|---|
| **Batting** | Behind the batter, elevated, looking down the pitch; tracks the live ball. Distance & height adjustable. | Clear ball tracking, line/length. |
| **Bowling** | Behind the bowler, lower, looking at the striker. | Release / seam / trajectory readability. |
| **Fielding** | Trails the ball from above-behind, framing the active fielder too. | Ball tracking, catch awareness, throw visibility. |
| **Spectator** | Side-on broadcast, high, square of the pitch. | The classic watch-the-match view. |
| **Free** | Explicit location + yaw/pitch. | Debug fly-cam. |
| **Orbit** | Spherical around a pivot (ball / impact). | Inspect a moment from any angle. |
| **Ball Follow** | Tight chase just behind the ball's travel. | Debug ball-follow. |
| **Physics Inspection** | Close, side-on, tight FOV. | Read swing/spin and the seam. |

---

## 2. Replay architecture

`UCricketReplayComponent` is the Replay Manager: **Recorder**, **Playback System**,
and the **physics visualization** it enables, over the pure data model in
`CricketReplayTypes.h`.

```
 RECORD   ball + actors + anim state sampled @ N Hz ──▶ FCricketReplayClip (ring)
          + sparse events (release/bounce/impact/catch/throw)
 PLAY     FCricketReplayPlayer cursor (rate/pause/step) ──▶ SampleAtTime ──▶ drive
          the (frozen) ball + actors; nothing is re-simulated
 VISUALIZE  path · bounce points · impact · measured swing/spin deviation
```

- **Recording pipeline** (§4): a fixed-rate, capped ring of compact frames so a
  long innings stays bounded, plus a sparse event list. Each delivery is its own
  clip (auto-started when the ball goes live).
- **Playback pipeline** (§5): the pure cursor advances the clip at a variable rate,
  the component samples an interpolated frame and writes it onto the actors; the
  ball physics is **frozen** so playback can never diverge from what was recorded.

---

## 3. Data models

In `CricketReplayTypes.h` and `CricketCameraTypes.h` (ball SI metres; transforms UE cm).

| Type | Role |
|---|---|
| `FCricketBallSnapshot` | Ball pos/vel/spin/seam + in-flight, per frame. |
| `FCricketActorSnapshot` | A player's location, rotation, anim-state id. |
| `FCricketReplayFrame` | One instant: ball + all actors. |
| `FCricketReplayEvent` | A timeline marker (type + time + location). |
| `FCricketReplayClip` | The recording: capped-ring frames + events; `SampleAtTime` interpolates. |
| `FCricketReplayPlayer` | The playback cursor: rate (slow-mo), pause, frame-step, seek. |
| `FCricketCameraPose` / `Config` / `Subjects` | Camera pose, per-mode tuning, the framed positions. |
| `ECricketCameraMode` / `ECricketReplayEventType` | The eight modes; the seven event kinds. |

**Storage efficiency:** fixed sample rate (default 60 Hz), a per-clip frame cap
(`MaxFrames`, oldest evicted), and sparse events — so memory is bounded regardless
of innings length. (Float quantization of snapshots is a drop-in further saving.)

---

## 4. Recording pipeline

Each frame while a delivery is live, the recorder captures:

- **Ball state** — from `UCricketBallPhysicsComponent::GetState()` (the real SI state).
- **Player state** — each registered actor's transform.
- **Animation state** — the actor's `UCricketCharacterAnimComponent` locomotion (or
  fielder) state id.
- **Match-relevant events** — release & bounce (from the ball's `OnBounce`), bat
  impact (from `OnBatImpact` + `GetLastBatContactCm`), catch & throw (marked by the
  rig from the fielder's `OnStateChanged`/`OnThrew`).

It records **results**, never inputs: the bounce point is where the ball actually
bounced, the impact is where the bat actually met it.

---

## 5. Playback pipeline

`StartReplay()` starts the cursor on the recorded clip. Each frame:

1. `FCricketReplayPlayer::Advance(realDt)` moves the cursor by `realDt × Rate`
   (Rate < 1 = slow motion; paused = hold; clamped to the clip).
2. `Clip.SampleAtTime(cursor)` returns an interpolated frame (lerp positions, slerp
   rotations).
3. The component freezes the ball physics and writes the sampled ball + actor
   transforms — so the replay is a faithful re-display, never a re-simulation.

Controls: pause/resume, frame-step (±1, auto-pauses), rate up/down (slow-mo),
seek, plus the replay cameras (Free / Orbit / Ball-Follow / Physics-Inspection).

---

## 6. Debug tooling

- **Physics-visualization overlays** (`cricket.Debug.Replay`): the recorded **ball
  path**, **bounce points**, **impact location**, catch markers, and the measured
  **swing** (in-flight lateral movement) and **spin/seam** (off-pitch deviation),
  computed from the actual path — the realism made visible, not a label.
- **Debug cameras**: Free, Ball-Follow, Orbit, and Physics-Inspection modes are
  first-class camera modes, selectable any time.
- **On-screen state**: the camera mode, and during replay the progress %, rate, and
  pause state.

---

## 7. Testing strategy

Headless suite `CricketSim.Camera` (`CricketPhysics/Private/CricketCameraTests.cpp`).
The framing, transitions, recording, playback and visualization are pure, so they
are tested without a viewport.

| Test | Proves |
|---|---|
| `Modes` | Each mode frames correctly (behind batter looking down the pitch; behind bowler; side-on; tight inspection FOV). |
| `Transitions` | A blend equals From at 0, To at 1, and is between midway. |
| `ReplayRecording` | The clip is a capped ring (oldest evicted) + sparse events; path/event accessors. |
| `ReplayPlayback` | `SampleAtTime` interpolates between frames and clamps the ends. |
| `SlowMotion` | The cursor obeys rate (½ speed), pause holds it, frame-step moves one frame, seek jumps. |
| `PhysicsVisualization` | Measured swing/spin deviation matches the actual lateral movement in a known path; a straight ball reads ~0. |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Camera; Quit" -unattended -nullrhi -nosplash
# results in ~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log
```

(The pre-existing `CricketSim.Bat.EdgeImpact` failure is unrelated and predates this work.)

---

## 8. Production-ready C++ — file map

| File | Module | Contents |
|---|---|---|
| `CricketReplayTypes.{h,cpp}` | CricketPhysics | Replay data models + cursor (§3,§5). |
| `CricketCameraTypes.h` | CricketPhysics | Camera data models (§3). |
| `CricketCameraModel.{h,cpp}` | CricketPhysics | Framing + transitions + swing/spin measurement (§1,§6). |
| `CricketCameraTests.cpp` | CricketPhysics/Private | The `CricketSim.Camera` suite (§7). |
| `CricketCameraDirectorComponent.{h,cpp}` | CricketGameplay | The Camera Manager (§1). |
| `CricketReplayComponent.{h,cpp}` | CricketGameplay | Recorder + Playback + overlays (§2,§4,§5,§6). |
| `CricketFieldingRig.{h,cpp}` (edited) | CricketGameplay | Live integration: cameras + record/replay + event marking. |

### Trying it in the editor

Drop an `ACricketFieldingRig` into a level and press Play. Launch a delivery
(**1–5**), then press **C** to cycle the gameplay cameras (Batting / Bowling /
Fielding / Spectator); **[ ]** adjusts the chase distance. Press **V** to replay the
delivery you just saw: **C** now cycles the replay cameras (Ball-Follow / Orbit /
Physics-Inspection / Spectator), **P** pauses, **Left/Right** frame-step,
**Up/Down** change the speed (slow motion), and the `cricket.Debug.Replay` overlay
draws the ball path, the bounce, the impact, and the measured swing/spin — exactly
what the physics produced.

---

## 9. Boundaries & future work

- **In scope, done:** four gameplay + four debug cameras with blended transitions;
  recording of ball/player/anim/event data; playback with slow-mo/pause/step/seek;
  physics-visualization overlays; a live integration; tests.
- **Deliberately not done:** broadcast presentation packages, cinematic camera
  rails, and on-disk replay serialization (the clip is a plain struct, trivially
  serializable when a save system arrives) — and anything that would let a camera or
  replay alter the simulation.
- **Future:** snapshot quantization for even smaller clips, multi-clip highlight
  reels, and Sequencer export — none of which change that physics stays the truth.
```
