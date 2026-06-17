# CricketSim — Player Control System (Cricket 07-inspired, Enhanced Input)

> Controls generate player INTENT only. The physics remains the source of truth.
> A well-played shot is earned through timing and footwork meeting the ball — the
> control scheme just expresses what the player is trying to do.

Read `Docs/ARCHITECTURE.md` first. This layer sits on top of the existing batting,
bowling, fielding, camera and replay systems and feeds them intent.

---

## 1. Input architecture

A pure **intent model** (`FCricketInputModel`) maps held control state to intent in
exactly the Cricket 07 idiom, and a thin **Enhanced Input** layer
(`UCricketPlayerInputComponent`) provides the keys, the mapping contexts, and the
state manager that routes actions through that model to the gameplay components.
Because the mapping is pure, every key-combo → intent rule is headless-testable and
independent of the Enhanced Input wiring.

```
 keys ─▶ Enhanced Input (IA + IMC, in C++) ─▶ UCricketPlayerInputComponent
                                                     │ (controllers)
                                       FCricketInputModel::Resolve* (pure intent)
                                                     │
                  Batting / Bowling / Camera / Replay / ... components
                                                     │
                                          PHYSICS decides the outcome
```

**Why Enhanced Input in C++?** The whole project is asset-free (debug-drawn, no
.uassets). Enhanced Input supports building `UInputAction`s and `UInputMappingContext`s
in code (`MapKey`), which is exactly what this does — genuine Enhanced Input, no asset
authoring. **Gamepad support is future-proofed**: add gamepad `MapKey` lines to the
same contexts; no logic changes (the model reads abstract intent, not keys).

The brief's nine architecture items:

| # | Item | Where |
|---|---|---|
| 1 | Mapping Contexts | `IMC_Match/Batting/Bowling/Fielding/Replay` (built in `BuildInput`). |
| 2 | Input Actions | the `IA_*` actions (built in `BuildInput`). |
| 3 | Input State Manager | `SetContext` + the Enhanced Input subsystem layer swap. |
| 4–9 | Batting / Bowling / Running / Fielding / Camera / Replay controllers | the handler regions in `UCricketPlayerInputComponent`, each calling the pure model. |

---

## 2. Enhanced Input design (the input layers)

Layers are **stacked mapping contexts**: a shared **Match** base layer is always
active (running, cameras, replay-enter), and exactly one **role** layer sits on top
(Batting / Bowling / Fielding), or the **Replay** layer when reviewing. Switching
role = swap the top context. Keys can repeat across role layers (D is front-foot when
batting, a delivery when bowling) precisely because only one role layer is active —
that is the point of input layers.

```
 [ Replay ]      (when reviewing)        priority 2
 [ Batting | Bowling | Fielding ]        priority 1   (one active)
 [ Match (running, cameras, replay V) ]  priority 0   (always)
```

`FCricketInputModel::ResolveContext(replay, batting, bowling, fielding)` is the pure
rule for which layer should be active (replay overrides; else the role).

---

## 3. Key mapping design (Cricket 07-inspired)

**Batting** (the headline scheme):

| Input | Meaning |
|---|---|
| **D** (hold) | Front foot |
| **W** (hold) | Back foot |
| **S** | Defensive |
| **Shift** (hold) | Lofted modifier |
| **Up / Right / Left / Down / E / Q** | Direction: Straight / Off / Leg / Fine Leg / Cover / Midwicket |
| **Space / LMB** | Play the shot (the timing input) |

The **foot × direction** grid yields the seven MVP strokes — Defensive, Straight
Drive, Cover Drive, Pull, Cut (back-foot off), Flick (front-foot leg), Lofted Drive
(Shift + drive). These map onto the four physics base swings + footwork + aim + power;
the loft and placement still **emerge from the contact**, never a scripted launch.

**Bowling**: D/S/W = stock / variation / aggressive; arrows = line/length; Q/E = swing/spin
modifiers; Space = bowl. **Running**: D take, A send back, W dive. **Fielding**:
Space catch, E throw, W dive, R relay (fielders keep using their own prediction).
**Camera**: C cycle, B ball-follow, F free. **Replay**: V toggle, P pause, [ ] speed,
, . frame-step.

---

## 4. Class hierarchy

```
CricketPhysics (pure, testable)
  FCricketInputModel ........ Resolve{BattingShot, Delivery, RunCall, FieldAction, Context}
                              + ToBattingInput (intent -> existing FCricketBattingInput)
CricketGameplay
  UCricketPlayerInputComponent .. Enhanced Input setup + state manager + the 6 controllers
  UCricketInputDebugComponent ... intent/context visualization (cricket.Debug.Input)
  ACricketPlayerPawn ............ the playable showcase (a batter facing auto-fed deliveries)
```

---

## 5. Data models

In `CricketInputTypes.h`.

| Type | Role |
|---|---|
| `ECricketInputContext` | The input layers (Match/Batting/Bowling/Fielding/Replay). |
| `ECricketShotDirection` | The six steer directions. |
| `ECricketC07Shot` | The seven MVP strokes. |
| `FCricketBattingControlState` | Held keys at the shot (front/back/defend/loft + direction). |
| `FCricketBattingShotIntent` | The resolved stroke + footwork + loft + direction + aim/power. |
| `FCricketBowlingControlState` / `FCricketBowlingControlIntent` | Delivery choice + line/length + swing/spin, and the resolved deltas. |
| `ECricketRunCall` / `ECricketFieldAction` | Running calls; fielding actions. |

These are intents — none of them name an outcome.

---

## 6. Debug tooling

`UCricketInputDebugComponent` (gated by `cricket.Debug.Input`) shows the **active
input context**, the live **batting control state** (held foot/defend/loft + direction),
the resolved **shot intent** after a play, the **bowling intent**, and the last
**running** / **fielding** action — plus a controls cheat-sheet.

---

## 7. Testing strategy

Headless suite `CricketSim.Input` (`CricketPhysics/Private/CricketInputTests.cpp`).
The intent model is pure, so the whole control scheme is tested without input hardware.

| Test | Proves |
|---|---|
| `BattingCombinations` | The C07 grid: S→defensive, D+off→cover drive (off aim), W+leg→pull, W+off→cut, D+leg→flick, Shift+drive→lofted; LH mirrors. |
| `BowlingCombinations` | Delivery choice sets pace; line/length steps; swing/spin modifiers. |
| `RunningActions` | D take / A send back / W dive, with dive priority. |
| `FieldingActions` | Catch/dive/throw/relay/move priority. |
| `ContextSwitching` | The active layer follows the role; replay overrides. |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Input; Quit" -unattended -nullrhi -nosplash
# results in ~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log
```

(The pre-existing `CricketSim.Bat.EdgeImpact` failure is unrelated and predates this work.)

---

## 8. Production-ready C++ — file map

| File | Module | Contents |
|---|---|---|
| `CricketInputTypes.h` | CricketPhysics | Data models (§5). |
| `CricketInputModel.{h,cpp}` | CricketPhysics | The pure intent mapping (§1,§3). |
| `CricketInputTests.cpp` | CricketPhysics/Private | The `CricketSim.Input` suite (§7). |
| `CricketPlayerInputComponent.{h,cpp}` | CricketGameplay | Enhanced Input + state manager + controllers (§1,§2). |
| `CricketInputDebugComponent.{h,cpp}` | CricketGameplay | Intent/context overlay (§6). |
| `CricketPlayerPawn.{h,cpp}` | CricketGameplay | The playable showcase (§4). |

### Trying it in the editor

Drop an `ACricketPlayerPawn` into a level and press Play. Deliveries are auto-fed at
the striker; play Cricket-07 strokes — hold **D** (front) or **W** (back), pick a
direction (**arrows / Q / E**), optionally hold **Shift** to loft, and press **Space**
on the ball to play it. **C/B/F** change cameras; **V** replays the delivery (then
**P** pause, **[ ]** speed, **, .** step). The `cricket.Debug.Input` overlay shows the
active context and the resolved intent — and whether you middle it is still decided by
the swing meeting the ball, not the key you pressed.

---

## 9. Boundaries & future work

- **In scope, done:** the full Cricket-07 batting scheme, bowling/running/fielding/
  camera/replay controls, the five input layers + state manager, Enhanced Input built
  in C++, the pure intent model, debug, tests, and a playable pawn.
- **Future-proofed:** gamepad (add `MapKey` lines), two-player / AI-partner running
  (the run call is an explicit intent ready to be communicated to a partner), and
  player-driven fielding overrides (the action is recorded; the fielder still predicts).
- **Deliberately not done:** UI/HUD, and anything that lets input determine an
  outcome — input ends at intent.
```
