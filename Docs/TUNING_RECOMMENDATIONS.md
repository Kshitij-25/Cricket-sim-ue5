# CricketSim — Tuning Recommendations

> The change list produced by the validation pass: what was tuned (with rationale and
> result), and the open items the data still flags. Every recommendation is backed by a
> measured metric in `VALIDATION_REPORT.md` / `Saved/Validation/Metrics.csv`.

---

## Part A — Changes applied (baseline → calibrated)

All preserve the neutral-identity guarantee (`Balance.NeutralIsIdentity`) and
determinism (`AI.Determinism`), and keep the existing AI tests green.

### A1. Contest run/boundary distribution — *scoring carried by boundaries, not free singles*
**Symptom:** boundary % 5.8 (real ~16), dot % 25 (real ~38), six % 0.9 — a low-tempo
single-milking game; the optimal AI exploited near-free singles.
**Change (`CricketContestModel::Resolve`):** cut the auto-single rate on the *rotate*
intent and added real dots; raised boundary conversion and added a six channel to the
*attack* intent; lifted the *boundary-hit* six share.
**Result:** boundary % → 13.7, dot % → 34.9, six % → 3.4, RR 6.8 → 7.7. ✅

### A2. Innings shape — *stop the powerplay eating the death*
**Symptom:** powerplay RR 10.6 (too hot) but death RR 6.8 (too cold) — the innings
*decelerated*; the top order fell early and the tail batted the death.
**Change (`FCricketTeamStrategy::PowerplayApproach` default `Accelerate`→`Consolidate`;
`CricketTacticalEvaluator` first-innings death lean +0.25):** calmer powerplay preserves
wickets for the death.
**Result:** powerplay 10.6 → 9.1, death 6.8 → 8.1; chase stopped collapsing in the PP. ✅

### A3. Chase model — *mirror the first-innings phase shape*
**Symptom:** chase success 12 % — chasers sat at a flat, over-conservative tempo and
fell ~20 short.
**Change (`CricketTacticalEvaluator`):** chase aggression = the phase Base shape +
run-rate gap modulation, instead of a flat `0.5 + gap`.
**Result:** chase success 12 % → 35 % (Hard), **52 % (Simulation)**. ✅

### A4. Pressure → mistakes — *don't auto-collapse the chaser*
**Change (`CricketBatterBrain`):** pressure-driven lapse multiplier 1.5 → 0.8 (a chasing
side carries high pressure throughout; 2.5× lapses guaranteed collapse).
**Result:** fewer chase collapses; supports A3. ✅

### A5. Wicket attrition trim
**Change:** attack/boundary-hit base wicket probabilities trimmed (~12 %) so more set
batters survive to the death.
**Result:** wickets/innings 6.8 → 6.1 (still in band); modest death/chase help. ✅

---

## Part B — Open items (tracked, data-flagged)

### B1. Death run rate at Hard tier — 8.1 vs believable floor 8.2 *(tracked)*
**Root cause:** wicket attrition front-loads — ~6 wkts fall before over 16, so the
**tail bats the death**. Already in-band at Simulation (9.47).
**Recommendation:** (a) shift wicket timing later (reduce mid-innings, not death, wicket
probability), and/or (b) give recognised lower-order hitters a higher boundary
conversion at the death so the tail can still swing. Re-measure death RR per over-band.

### B2. Chase success at Hard tier — 35 % vs floor 36 *(tracked)*
**Root cause:** chasing is **wicket-limited** — a steeper gap response was tested and
*lowered* chase success (more aggression → more wickets → collapse). In-band at
Simulation (52 %).
**Recommendation:** improve chase *wicket preservation*, not aggression — e.g. a
"wickets-in-hand vs balls-left resource" gate that defends more when a collapse is in
progress, accelerating only when resources are safe (a Duckworth-Lewis-style resource
read). This is brain work, not a contest dial.

### B3. "Optimal" tier is risk-averse for T20 *(tracked)*
**Symptom:** Simulation RR 7.23 < Easy RR 8.33 — the best tier under-scores because
low-variance accumulation beats boundary-hitting on the current expected value.
**Recommendation:** raise the batting value function's **scoring weight** at high
situational awareness (or lower the implicit wicket-loss penalty) so the optimal line is
"use all 120 balls and 10 wickets." Validate that Sim RR rises above the weaker tiers
while wickets stay ≤ them.

### B4. Boundary / dot / six in WARN, not PASS *(minor)*
Boundary 13.7 (band 14–19.5), dot 34.9 (35–42), six 3.4 (4–7.5) sit just below the
on-target core. **Recommendation:** a small `BoundaryScale` ≈ 1.08 nudge (or contest
six-share +) lifts all three into the core without disturbing the believable bands —
candidate for the shipped "Calibrated" preset.

### B5. Dismissal mix *(minor)*
Run-outs ~18 % of dismissals (a touch high), stumpings <1 % (a touch low).
**Recommendation:** lower the running run-out probability slightly; raise the spin
stumping chance for advanced-footwork dismissals.

### B6. Physics magnitude calibration *(see `PHYSICS_VALIDATION.md`)*
Physics is validated for direction/sign/symmetry but not absolute magnitude.
**Recommendation:** add `CricketSim.Physics.Magnitudes` asserting swing/seam/spin/bounce
against the published reference bands, reusing `FCricketBenchmarkRange`.

---

## Part C — Shipping the calibrated preset

The neutral default is the identity reference. For a future **"Calibrated" shipped
preset**, fold B4's small `BoundaryScale` nudge into `FCricketBalanceConfig` and ship it
as the default match balance, keeping `Neutral()` as the regression baseline. Gate it
behind the same `CricketSim.Balance.ValidationReport` run so the headline grade only
improves.
