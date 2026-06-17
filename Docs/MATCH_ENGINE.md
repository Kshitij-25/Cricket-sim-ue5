# CricketSim — T20 Match Engine

> The rules layer above the physics sandbox. It consumes interpreted ball
> outcomes and applies cricket law — scoring, overs, wickets, innings, result.
> It NEVER alters a physics result; it only scores it.

This milestone turns the physics sandbox into a complete, playable T20 match
(India vs Australia). Read `Docs/ARCHITECTURE.md` first — this layer lives in the
`CricketSim` module, the top of the dependency graph (rules/GameMode/team data).

---

## 1. Technical design

### 1.1 The one hard rule: never alter physics

The engine sits behind a single, narrow seam: `FCricketBallResult` — the raw
physical facts of one delivery (was it a wide, did the bat strike it, did it hit
the stumps, was it caught, did it clear the rope, how many were run). The
simulation fills that in; `FCricketOutcomeInterpreter` classifies it into a
`FCricketDeliveryOutcome`; the engine applies the laws to that outcome. The engine
reads no physics and changes nothing about it.

```
 Ball/Bat/Bowl/Field physics ──▶ FCricketBallResult   (raw facts: struck? caught? six? runs run?)
                                        │
                 FCricketOutcomeInterpreter::Interpret  (the consume-physics seam)
                                        │
                                        ▼
                          FCricketDeliveryOutcome        (wide / 4 / caught / bye / run-out …)
                                        │
                       UCricketMatchEngine::ApplyDelivery (THE LAWS)
                                        │
        score · strike · over · wicket · innings · target · result
```

### 1.2 Why one engine, not seven services

The brief lists seven pieces (Match/Score/Innings/Over/Wicket/Statistics/State).
Applying a ball must be **atomic** — runs, strike, wicket, over-end and innings-end
all interact on a single delivery — so they live as clearly-delineated
responsibilities inside one deterministic `UCricketMatchEngine` rather than as
seven actors passing messages. The mapping is explicit (see §2).

### 1.3 What this layer must NOT do

No commentary, replays, crowds, presentation, or any AI (captain/bowling/batting).
The runner's ball generator is a deterministic stand-in for a full per-ball physics
rollout (which would need batting/running AI, out of scope); the interpreter +
engine path it feeds is the real one, ready to take results straight from physics.

---

## 2. Match architecture (the seven pieces)

| # | Piece | Where | Responsibility |
|---|---|---|---|
| 1 | **Match Engine** | `UCricketMatchEngine` | Orchestration + public API + `ApplyDelivery`. |
| 2 | **Score Engine** | `Score_Apply` | Runs, extras, boundaries → totals + batter/bowler stats. |
| 3 | **Innings Manager** | `Innings_Begin/CheckEnd/Close` | Lifecycle, end conditions, target. |
| 4 | **Over Manager** | `Over_Advance` + `SetBowler/CanBowl` | Ball/over counting, strike rotation, bowler legality. |
| 5 | **Wicket Manager** | `Wicket_Apply` | Dismissal, bowler credit, new batter, all-out. |
| 6 | **Statistics System** | `FCricketBatter/BowlerStats` in the cards | Live batting/bowling figures. |
| 7 | **Match State Machine** | `SetMatchState` + transitions | PreMatch → … → MatchComplete. |

All deterministic and free of `UWorld`, so the engine runs headlessly in tests and
is driven in-game by `ACricketMatchRunner`.

---

## 3. State machine design

```
 PreMatch ──StartMatch──▶ Toss ──PerformToss──▶ FirstInnings
                                                     │ innings ends (all out / overs / —)
                                                     ▼
                                              InningsBreak   (target = 1st-innings + 1)
                                                     │ StartSecondInnings
                                                     ▼
                                              SecondInnings
                                                     │ innings ends (all out / overs / TARGET PASSED)
                                                     ▼
                                              MatchComplete  (result computed)
```

Innings-end conditions (checked after every ball): **all out** (10 down), **overs
complete** (20×6 legal balls), or — second innings only — **target reached**
(chase ends instantly). The result: chasers pass it → win by wickets; finish level
on `target−1` → tie; fall short → bowling side wins by runs.

---

## 4. Data models

In `CricketSim/Public/CricketScoringTypes.h` (reusing `ECricketDismissal`,
`ECricketDeliveryLegality`, `FCricketMatchRules`, `FCricketInningsState` from
`CricketMatchTypes.h`).

| Type | Role |
|---|---|
| `FCricketDeliveryOutcome` | The interpreted ball the engine applies: legality, `RunsOffBat`(+`bBoundary`), `RanExtraRuns`+`ExtraType` (bye/leg-bye), `Dismissal`(+`bDismissedStriker`). Scorebook builders (`Four()`, `Wide()`, `Out(...)`). |
| `FCricketBallResult` | The raw physical facts (the consume-physics seam). |
| `FCricketBatterStats` | Runs, Balls, Fours, Sixes, out/dismissal/bowler, `StrikeRate()`. |
| `FCricketBowlerStats` | LegalBalls→overs, Maidens, RunsConceded, Wickets, `Economy()`. |
| `FCricketInningsScorecard` | Totals + batters + bowlers + live striker/non-striker/next-batter/bowler indices; `RunRate()`. |
| `FCricketSquad` | A playing XI (batting order = array order). |
| `FCricketMatchResult` | Decided/tie/winner/summary. |
| `ECricketMatchState` / `ECricketExtraType` | The six states; None/Bye/LegBye. |

**Run accounting** is defined once so the interpreter and engine agree exactly:
wide/no-ball add 1 penalty and are re-bowled; `RunsOffBat` is the striker's (0 on a
wide); `RanExtraRuns` are byes/leg-byes (or runs run on a wide); the bowler is
charged penalty + bat runs but **not** byes/leg-byes; a ball is "faced" only when
legal; strike rotates on odd **runs run** (not boundaries, not the penalty) and at
the end of each over.

---

## 5. Statistics architecture

Stats are not a separate pass — they are mutated in lockstep with scoring inside
`Score_Apply`/`Wicket_Apply`, so the scorecard is always exactly consistent with
the totals:

- **Batting**: runs, balls, fours, sixes per `FCricketBatterStats`; strike rate is
  derived (`100·runs/balls`). Dismissal + the bowler who took it are recorded.
- **Bowling**: legal balls (→ overs), maidens (an over with 0 runs charged to the
  bowler — byes don't count against him), runs conceded, wickets; economy derived
  (`6·runs/balls`).
- **Match**: score, run rate, required run rate, runs required, balls/overs
  remaining, target — all live getters on the engine.

---

## 6. Debug tooling

`ACricketMatchRunner` draws a live scoreboard HUD via on-screen debug text:
match **state**, both innings' **score**/wickets, the current **over** (o.b), the
**striker**\* and non-striker with their runs(balls), the **bowler**'s figures
(o-m-r-w + economy), **wickets**, **run rate**, and — chasing — target / runs
needed / balls left / **required run rate**, ending with the result. It is also the
playable driver (auto-play or ball-by-ball), so the same tool both runs and
visualizes a full match.

---

## 7. Testing strategy

Headless suite `CricketSim.Match` (`CricketSim/Private/CricketMatchTests.cpp`).
The engine is pure, so each test feeds scorebook outcomes and asserts the laws —
no physics, no randomness.

| Test | Proves |
|---|---|
| `FullOver` | Six legal balls complete an over; 1,4,0,6,2,1 = 14; the four is recorded. |
| `MaidenAndStrike` | A six-dot over is a maiden; strike rotates between overs and on a single. |
| `Extras` | Wide/no-ball add a penalty and re-bowl; byes are extras off neither bat nor bowler. |
| `Wickets` | Caught credits the bowler, run out does not; new batter in; all out ends the innings. |
| `InningsTransitions` | First innings closes → break → target set → chase begins with the other side batting. |
| `Chase` | Passing the target ends the match; won by wickets. |
| `Tie` | Finishing on `target−1` is a tie. |
| `DefendWin` | Falling short → bowling side wins by runs. |
| `Interpreter` | Raw ball facts classify correctly (six/caught/bowled/bye/run-out) and feed the engine. |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Match; Quit" -unattended -nullrhi -nosplash
# results in ~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log
```

(The pre-existing `CricketSim.Bat.EdgeImpact` failure is unrelated and predates this work.)

---

## 8. Production-ready C++ — file map

| File | Contents |
|---|---|
| `CricketScoringTypes.h` | Data models (§4). |
| `CricketMatchEngine.h/.cpp` | The engine + all seven responsibilities (§2). |
| `CricketOutcomeInterpreter.h/.cpp` | `FCricketBallResult` → `FCricketDeliveryOutcome` (the seam). |
| `CricketMatchTests.cpp` | The `CricketSim.Match` suite (§7). |
| `CricketMatchRunner.h/.cpp` | Playable driver + debug HUD (§6). |

### Trying it in the editor

Drop an `ACricketMatchRunner` into any level and press Play: an India–Australia T20
plays out in the HUD. **Space** bowls a ball, **Enter** toggles auto-play,
**Up/Down** change the speed, **R** restarts (same `Seed` ⇒ same match). The
shipping path wires the engine onto `ACricketGameMode` (which already holds
`FCricketMatchRules`); the runner is the self-contained test/debug environment, in
the style of the other rigs.

---

## 9. Boundaries & future work

- **In scope, done:** overs, deliveries, runs (dot→six), all extras (wide/no-ball/
  bye/leg-bye), all five dismissals, strike rotation, innings management, target,
  result, full stats, the six-state machine, debug HUD, and the tested rules core.
- **Deliberately untouched:** the physics/pitch/collision/batting/fielding cores —
  the engine only interprets their results.
- **Future:** free-hit after a no-ball, DLS, super-overs, the full per-ball physics
  rollout feeding `FCricketBallResult` (needs the batting/running AI of a later
  phase), and persisting the scorecard to `ACricketGameState` for replication.
```
