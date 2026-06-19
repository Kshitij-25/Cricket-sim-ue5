# Presentation System

A broadcast-quality **presentation layer** that sits *on top of* the finished
simulation. It makes the game read and feel like a premium sports title — directing
cameras, rolling replays, swelling the crowd and painting score graphics — **without
ever touching a gameplay outcome**. Lives in the `CricketPresentation` runtime module
at the very top of the dependency graph, alongside `CricketUI` and `CricketAudio`:

```
CricketPresentation ─► CricketAudio / CricketUI
                    ─► CricketSim ─► CricketGameplay ─► CricketPhysics ─► Engine/Core
```

## The one hard rule

Presentation is a **consequence** of the simulation, never a cause. The data flow is
strictly one-way, exactly like the audio layer:

```
match/physics result ─► FCricketMatchSnapshot ─► FCricketEventClassifier ─► FCricketPresentationEvent
                     ─► broadcast / replay / crowd / score directors ─► camera + replay + atmosphere + graphics
```

Every read of the Match Engine happens through `FCricketMatchSnapshot` — an immutable
**copy** of the scoreboard. The only things the layer ever *commands* are
presentation-only: the camera director's **mode** and the replay component's
**playback**. Neither can change a scored result. Turn the whole layer off
(`bEnabled = false`, or never place it) and the simulation is bit-for-bit identical.

## Architecture (the six pieces)

| Brief piece | Implementation | Kind |
|---|---|---|
| **1. Presentation Manager** | `UCricketPresentationSubsystem` | World subsystem (thin wrapper) |
| **2. Broadcast Director** | `FCricketBroadcastDirector` | Pure core |
| **3. Replay Director** | `FCricketReplayDirector` | Pure core |
| **4. Event Presentation System** | `FCricketEventClassifier` + `FCricketPresentationEvent` | Pure core |
| **5. Crowd Presentation Controller** | `FCricketCrowdPresentationModel` | Pure core |
| **6. Match Flow Presentation Layer** | `FCricketMatchFlowModel` + `FCricketBroadcastSequence` | Pure core |
| *(score graphics)* | `FCricketScorePresentationModel` | Pure core |

The split is the same one the UI and audio layers use: the **decision cores are pure**
(no `UWorld`, no cameras, no RHI), so the entire "what to show, which camera, replay or
not, how loud, what does the graphic read" logic is unit-tested headlessly. The
`UCricketPresentationSubsystem` is just the world wrapper that discovers the live
systems, runs the cores, and drives the camera/replay.

### Presentation Manager — `UCricketPresentationSubsystem`

A `UTickableWorldSubsystem` (auto-creates in Game/PIE, no level wiring — same as the
audio manager). It:

- **discovers** the Match Engine (via `ACricketMatchRunner`), the Replay component, the
  Camera Director component, the Ball physics, and the Audio subsystem;
- **binds** the engine's `OnMatchStateChanged` and `OnBallApplied` delegates;
- **classifies** each ball into `FCricketPresentationEvent`s and runs a small
  presentation state machine: `Idle → LivePlay → EventBeat → Replay → BroadcastSequence`;
- **directs** the broadcast off those events — selects a camera, arms an automatic
  replay, swells the crowd arc, refreshes the score graphics;
- **sequences** the match-flow packages (intro / team / toss / over / innings / result);
- **surfaces** player-reaction intents (`OnReactionRequested`) for the animation/AI
  layers to consume — a passive output, not a command;
- draws a debug overlay under `cricket.Presentation.Debug 1`.

> **Over transitions** are driven from `HandleBallApplied` (via the score model's
> over-complete return), because the engine broadcasts `OnOverComplete` *before*
> `OnBallApplied` inside `ApplyDelivery` — so the fresh over summary only exists once
> the ball has been folded in.

### Broadcast cameras

The director only **picks an angle**; the live `UCricketCameraDirectorComponent` still
computes the pose from the subjects. Angles map onto the existing gameplay modes:

| Broadcast camera | Gameplay mode | Used for |
|---|---|---|
| Main Broadcast | `Spectator` | the side-on master, between/through deliveries |
| Bowling | `Bowling` | the bowler running in |
| Batting | `Batting` | milestone celebrations |
| Boundary | `Fielding` | following a struck ball to the rope |
| Stump | `BallFollow` | bowled/stumped dismissals |
| Replay | `Orbit` | orbiting replay framing |

Live selection has a small hysteresis (`MinHoldSeconds`) so the master shot doesn't
flicker; a boundary chase is treated as urgent and cuts immediately.

### Replay integration

`FCricketReplayDirector` turns a moment into an `FCricketReplayPlan` (whether to roll,
the slow-motion rate, the ordered angles, the hold per angle). The manager hands the
plan to the **existing** `UCricketReplayComponent`, which plays back the **recorded**
frames — it never re-simulates. Defaults:

- **Wicket** → always; 0.2–0.3× speed; stump-led (bowled/stumped) or master-led, 3 angles.
- **Six** → always; 0.3–0.4×; boundary-led, 2 angles.
- **Four** → only a *pressure* boundary (severity ≥ High); 0.5×.
- **Match result** → the fullest package; 0.25×; 3 angles.

The manager holds a short live **event beat** (`EventBeatSeconds`) before cutting to the
replay so the real ball finishes first, and never starts a replay while one is already
running (manual replays win).

### Crowd presentation

`FCricketCrowdPresentationModel` is the crowd **atmosphere arc** — deliberately distinct
from the audio layer's `FCricketCrowdController` (which does the instantaneous
reaction bump that gates one-shot cheers). This one models the slower broadcast
narrative: a **baseline tension** that rises through a tight death-overs chase and
relaxes when the game drifts out of reach, with event impulses layered on top. It
outputs an atmosphere level and a mood band (`Calm → Building → Loud → Electric →
Tense`) the director reads for *match-closing tension*.

### Score presentation

`FCricketScorePresentationModel` produces the pre-formatted broadcast lines:

- **Over summary** — `Over 12: 8 runs, 1 wkt — 96/3` (at the end of each over)
- **Partnership** — `Partnership 45 (32)` (resets on a wicket)
- **Chase line / RRR** — `Need 48 from 30 • RRR 9.60`
- **Milestone notifications** — `FIFTY • Kohli (34 balls)`, `FIVE-WICKET HAUL • …`

### Match flow

`FCricketMatchFlowModel` assembles each broadcast package as a list of captioned, timed
camera steps (`FCricketBroadcastSequence` with a runtime cursor the manager advances):
**match intro, team intro, toss, over transition, innings transition, match result**.

## Player reactions (passive output)

Wickets, boundaries, milestones and the result emit `FCricketReactionIntent`s via
`OnReactionRequested` (celebration / frustration / victory, with an intensity). The
presentation layer does **not** play the animation — it only says "this player would
react now", leaving the animation/AI layers to consume it. This keeps the layer passive
while still driving immersion.

## Debug tooling

`cricket.Presentation.Debug 1` draws the live state: current presentation state, active
broadcast camera, the active event (with a `[DEFINING]` flag), the crowd atmosphere +
mood, the partnership and last-over summary, the active caption, which sources are
bound, and a rolling log of recent moments (marking the ones that triggered a replay).

## Testing

Suite **`CricketSim.Presentation`** (9 tests), all headless/pure:

| Test | Covers |
|---|---|
| `Boundary` | four vs six classification, severity, captions, pressure escalation |
| `Wicket` | dismissal naming, falling batter/bowler, late-chase escalation |
| `Milestone` | fifty/century/five-for/team-hundred crossing detection |
| `Result` | full-ball priority ordering + the match-deciding defining flag |
| `Broadcast` | live-camera hysteresis, per-event angle, gameplay-mode mapping |
| `Replay` | wicket/six always replay, low-key four doesn't, result is fullest |
| `Crowd` | event lift, close-chase Tense baseline, decay to Calm |
| `Score` | over summary, partnership grow/reset, chase line, milestone text |
| `Flow` | intro/toss/innings/result sequences + cursor advance/complete |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Presentation; Quit" -unattended -nullrhi -nosplash
```

The `UCricketPresentationSubsystem` playback wrapper itself is exercised in PIE, not
unit-tested — the same split the UI and audio layers use.

## What it does *not* do

It never calls a mutator on the Match Engine, never feeds back into physics, fielding
or scoring, and never gates a ball. Camera and replay are the only outward effects, and
both are cosmetic. The simulation's integrity is preserved by construction.
