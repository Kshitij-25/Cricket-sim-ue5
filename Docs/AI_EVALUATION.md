# CricketSim — AI Evaluation Report

> How the cricket-intelligence layer behaves under match conditions, measured across
> difficulty tiers and phases. Live numbers regenerate into
> `Saved/Validation/AIEvaluation.md` via `CricketSim.Balance.AIEvaluation`.

The AI is a **driver of the shared systems**, not a parallel sim: brains emit the same
intent a human gives (shot + footwork, line + length, bowling change) and outcomes
emerge from the physics/contest + laws. Difficulty tunes **decision quality**, never
reaction speed or omniscience. This report grades that behaviour.

---

## 1. Difficulty tiers (40 matches each, neutral balance)

| Tier | 1st-inn | RR | Wkts | Bound% | Dot% | DeathRR | ChaseWin% |
|---|---:|---:|---:|---:|---:|---:|---:|
| Easy | 165 | 8.33 | 7.0 | 18.5 | 43.4 | 5.73 | 30 |
| Medium | 169 | 8.15 | 6.5 | 16.5 | 39.5 | 6.94 | 28 |
| Hard | 154 | 7.67 | 6.2 | 13.9 | 35.6 | 7.76 | 33 |
| Simulation | 143 | 7.23 | 5.7 | 8.2 | 26.0 | **9.47** | **52** |

### Finding A — tiers express *risk appetite*, and stronger = more controlled
A stronger tier **preserves wickets** (Sim 5.7 vs Easy 7.0) and chases far better
(Sim 52 % vs Easy 30 %), because it reads the bowling and avoids the rush of blood.
This is the intended decision-quality signal and the `AIEvaluation` test asserts it
(stronger AI loses fewer wickets).

### Finding B — the "optimal" tier is currently risk-averse for T20 *(tracked)*
The Simulation tier scores *slower* (RR 7.23) than Easy (8.33): it has learned that in
the current contest, low-variance accumulation beats boundary-hitting on expected
value. In real T20 the best side should also score the most. **Recommendation:** lift
the batting value function's scoring weight at high situational awareness so the
optimal line is "use all 120 balls & 10 wickets," not "survive." Logged as an
`AI FINDING` warning by the test. See `TUNING_RECOMMENDATIONS.md` §AI value function.

---

## 2. Phase behaviour (powerplay → death)

After the innings-shape fix, the tempo is correctly ordered at the stronger tiers:

| Tier | Powerplay RR | Death RR | Shape |
|---|---:|---:|---|
| Easy | 11.5 | 5.7 | inverted (collapses early) |
| Hard | 9.4 | 7.8 | flattening |
| Simulation | ~9 | **9.5** | correct (accelerates) |

The weaker tiers blaze the powerplay, lose the top order, and leave the tail to bat the
death — itself a realistic *consequence* of poor decision-making, but it suppresses the
death rate. The Simulation tier holds wickets and accelerates correctly. This is the
clearest illustration that the AI's quality, not a scripted curve, drives the innings
shape.

---

## 3. Bowling & captaincy (from the shipped AI tests)

Validated by `CricketSim.AI.*` (all pass):

- **Bowling decisions** (`BowlerDeath`, `BowlerMatchup`): more yorkers at the death
  than in the powerplay; an aware bowler targets the batter's weakness more than a
  blind one. Good length is the stock ball (27 % of deliveries), with a believable
  variation spread (full 24 %, back-of-length 20 %, bouncer 16 %, yorker 14 %).
- **Field placement** (`CaptainLegal`): the captain always sets a legal field — never
  more than the allowed riders outside the circle in the powerplay — and rotates
  bowlers within the over-cap / no-consecutive-overs laws.
- **Batter intent** (`BatterAggression`, `BatterLeave`): aggression scales with the
  required rate (a steep chase forces the aerial route); a disciplined batter rarely
  chases the wide one.

---

## 4. Run-chase behaviour *(tracked)*

Chasing is **wicket-limited** here: the chase now mirrors the first-innings phase shape
modulated by the run-rate gap (fixing the old flat, over-conservative tempo), lifting
chase success from 12 % to 35 % at Hard and **52 % at Simulation**. The residual gap at
Hard is the same wicket-attrition issue as the death overs — pushing harder when behind
loses more wickets, so the lever is wicket preservation, not more aggression
(empirically confirmed: a steeper gap response *lowered* chase success). See
`TUNING_RECOMMENDATIONS.md` §Chase.

---

## 5. Summary scorecard

| Dimension | Verdict |
|---|---|
| Shot selection | ✅ Believable intent mix; scales with situation |
| Bowling decisions | ✅ Phase- & matchup-appropriate |
| Field placement | ✅ Always legal, plan-consistent |
| Powerplay behaviour | ✅ Correct after shape fix (stronger tiers) |
| Run-chase behaviour | 🟡 Realistic at Sim (52 %); Hard tier collapses a little |
| Difficulty scaling | ✅ Quality → wicket preservation; 🟡 scoring-weight tracked |
