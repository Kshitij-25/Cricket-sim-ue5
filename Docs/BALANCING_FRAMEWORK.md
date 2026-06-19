# CricketSim — Balancing & Validation Framework

> The production tooling for tuning the simulation to believable cricket and proving
> it stays there. No new gameplay features — this layer **measures, grades and tunes**
> the systems that already exist.

This document covers the four C++ pieces that make up the framework, the six tuning
dials, the real-world benchmark set, and the workflow for a balancing pass. The
generated statistical results live in [`VALIDATION_REPORT.md`](VALIDATION_REPORT.md),
the AI behaviour analysis in [`AI_EVALUATION.md`](AI_EVALUATION.md), the physics
checks in [`PHYSICS_VALIDATION.md`](PHYSICS_VALIDATION.md), the human test scripts in
[`PLAYTEST_PLANS.md`](PLAYTEST_PLANS.md) and the change list in
[`TUNING_RECOMMENDATIONS.md`](TUNING_RECOMMENDATIONS.md).

---

## 1. Where it lives

Everything is in the existing **`CricketAI`** runtime module (pure, headless,
deterministic — no `UWorld`), built on the already-shipped headless match simulator.

| File | Role |
|---|---|
| `Public/CricketBalanceConfig.h` | **Balancing Framework** — `FCricketBalanceConfig`: the six tuning dials + named presets (incl. pitch conditions). |
| `Public/CricketMatchAnalytics.h` · `Private/CricketMatchAnalytics.cpp` | **Analytics Architecture** — `FCricketT20Benchmarks`, `FCricketAggregateMetrics`, verdict grading, Markdown/CSV reports. |
| `Public/CricketSimulationBatch.h` · `Private/CricketSimulationBatch.cpp` | **Match Simulation Framework** — `FCricketSimulationBatch`: run hundreds of matches across conditions / difficulties and aggregate. |
| `Private/CricketBalancingTests.cpp` | **Validation gate** — the `CricketSim.Balance.*` automation tests that run the batches, grade them, assert believability and write the reports. |

These plug into the unchanged contest path:

```
FCricketSimulationBatch  →  FCricketAIMatchSimulator::Simulate(…, Balance)
        ↓ (N matches)                    ↓ per ball
FCricketAggregateMetrics  ←  telemetry   FCricketBatterBrain / BowlerBrain / CaptainBrain
        ↓ grade                          FCricketContestModel::Resolve(…, Balance)  ← dials applied here
FCricketT20Benchmarks → PASS / WARN / FAIL → Markdown + CSV report
```

---

## 2. The six tuning dials (`FCricketBalanceConfig`)

Each dial is a multiplier (neutral = `1.0`) or a bias (neutral = `0.0`). A
**default-constructed config is the identity transform** and reproduces the shipped
behaviour *bit-for-bit* — guaranteed by the `CricketSim.Balance.NeutralIsIdentity`
test. Designers move a dial, re-run the batch, read the metric deltas.

| Brief requirement | Dial | Effect | Applied in |
|---|---|---|---|
| **Swing strength** | `SwingStrength` | Scales the wicket threat lateral seam/swing movement carries (pace). | contest: movement → wicket |
| **Spin strength** | `SpinStrength` | Scales spin's bite — beating the bat, the stumping down the track. | contest: turn → wicket, stumping |
| **Bounce variation** | `BounceVariation` | How much a lively/uneven surface punishes the short ball & mistimes. | contest: short-ball wicket |
| **Bat timing windows** | `BatTimingWindow` | Widens/tightens the clean-contact window (a wider window middles more). | contest: `CleanContact` |
| **AI aggression** | `BatterAggressionBias`, `BowlerAggressionBias` | Biases every batter's intent-to-score / bowler's attack tendency. | simulator: effective profile |
| **AI risk-taking** | `BatterRiskTaking` | Scales how much accepted batting risk feeds the wicket chance. | contest: risk → wicket |

Plus two **master scoring dials** for headline tuning without distorting the matchup
behaviour: `BoundaryScale` (run-rate) and `GlobalWicketScale` (wicket-rate). The
`CricketSim.Balance.DialsAreMonotonic` test proves each moves the metrics the right
way (e.g. `BoundaryScale +40%` → run rate up & boundary % up; `GlobalWicketScale +60%`
→ wickets up).

### Presets / pitch conditions

`FCricketBalanceConfig` ships named presets that double as the **pitch-condition
axis** the validation batch sweeps (measured numbers in `VALIDATION_REPORT.md`):

| Preset | Character | Measured (Hard AI, 50 matches) |
|---|---|---|
| `Neutral()` | Identity / true pitch | 157 @ 7.81, 6.2 wkts, 13.9% bnd |
| `FlatTrack()` | A road — bat dominates | **169 @ 8.40, 4.9 wkts, 16.6% bnd, 44% chases** |
| `GreenSeamer()` | Lateral movement + bounce | 138 @ 6.99, 7.8 wkts, 11.4% bnd |
| `Turner()` | Dry rank-turner, spin bites | 138 @ 6.94, 7.5 wkts, 10.8% bnd |
| `WornPitch()` | Day-2 variable bounce + reverse | 138 @ 7.11, 7.1 wkts, 11.5% bnd |

---

## 3. Analytics Architecture

`FCricketAggregateMetrics::FromMatches()` folds a population of `FCricketAIMatchTelemetry`
into the full metric suite the brief lists:

- **Scoring:** avg 1st/2nd-innings total (+σ), run rate (+σ), team strike rate.
- **Wickets:** wickets/innings, bowling average, dismissal-mode mix.
- **Frequencies:** boundary %, dot-ball %, six %, economy rate.
- **Phase:** powerplay & death run rates (new phase-split telemetry).
- **Outcome:** chase success %, tie %.
- **Distributions:** batter-intent mix (shot distribution), bowling-length mix
  (pitch map), dismissal mix — rendered as ASCII bar charts in the report.

`FCricketT20Benchmarks::Default()` is the single source of truth for the real-world
target bands (modern men's T20I / top franchise cricket). Each metric gets a
`[PassLo, PassHi]` on-target core and a wider `[WarnLo, WarnHi]` believable envelope;
`Classify()` returns **Pass / Warn / Fail**. Reports render to Markdown (graded table
+ distribution charts) and CSV (one row per cell, for spreadsheets/plots).

---

## 4. Match Simulation Framework

`FCricketSimulationBatch` is the experiment harness:

- `StandardTeam(code, difficulty)` — a balanced, believable XI (promoted from the AI
  tests so tooling and tests share one team).
- `RunCell(label, A, B, rules, balance, N, seedBase)` — N deterministic matches under
  one config; alternates who bats first by match parity to debias 1st/2nd-innings
  stats.
- `SweepConditions(difficulty, conditions, N)` — the pitch-condition sweep.
- `SweepDifficulties(N)` — Easy / Medium / Hard / Simulation, neutral balance.

Determinism: every match is seeded from `(seedBase, matchIndex)`, so an entire
report is reproducible. The `CricketSim.AI.Determinism` test guards bit-stability.

---

## 5. Running it

```sh
# Build
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
  CricketSimEditor Mac Development -project="$PWD/CricketSim.uproject"

# Run the validation gate (writes Saved/Validation/*.md + Metrics.csv)
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Balance; Quit" -unattended -nullrhi -nosplash
```

Results: `~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log`
(grep `Test Completed. Result=`). Reports: `<Project>/Saved/Validation/`.

The four gate tests:

| Test | Asserts |
|---|---|
| `Balance.NeutralIsIdentity` | Neutral config == default behaviour, bit-for-bit (regression guard). |
| `Balance.DialsAreMonotonic` | Every dial moves the metrics the expected direction; presets rank correctly. |
| `Balance.ValidationReport` | Headline scoring metrics are believable vs benchmarks; writes the report; tracks the two known gaps. |
| `Balance.AIEvaluation` | Difficulty tiers behave; writes the AI evaluation report. |

---

## 6. A balancing pass (workflow)

1. **Measure** — run `CricketSim.Balance`; read `Saved/Validation/ValidationReport.md`.
2. **Diagnose** — find the FAIL/WARN rows; use the distribution charts + condition /
   difficulty sweeps to locate the cause (e.g. inverted phase shape, low boundary %).
3. **Tune** — move a `FCricketBalanceConfig` dial, *or* (for a permanent shift) the
   shipped contest/brain constant the dial scales. Never widen a benchmark to pass.
4. **Re-measure & compare** — the CSV diff shows the metric deltas; confirm no
   collateral regression in the believable bands or the determinism test.
5. **Record** — note the change and its rationale in `TUNING_RECOMMENDATIONS.md`.
