# CricketSim — Human Playtest Plans

> Structured scripts for the things automation can't measure: *feel*. The statistical
> framework proves the sim is believable on paper; these plans prove it is **enjoyable
> and responsive** in the hands. Each plan has a setup, a procedure, the signals to
> watch, and a 1–5 rating rubric so sessions are comparable across testers and builds.

**How to run:** one facilitator, one tester per session. Tester thinks aloud.
Facilitator records ratings + verbatim quotes against the rubric, never coaches.
Capture build hash + balance preset. Cross-reference any "feel" complaint against
`Saved/Validation/Metrics.csv` to see if it has a statistical signature.

Rating scale: **1** broken · **2** frustrating · **3** acceptable · **4** good ·
**5** excellent. Target: median ≥ 4 on every dimension before sign-off.

---

## Plan 1 — Batting feel

**Setup:** Free-hit net / single-over loop, neutral pitch, Medium bowling AI; then
repeat on FlatTrack and GreenSeamer.

**Procedure**
1. Face 2 overs defending only — judge whether timing a block feels controllable.
2. Face 2 overs rotating strike — can you place singles intentionally?
3. Face 2 overs attacking — does middling a drive feel earned and distinct from a
   mistime/edge?
4. Face a short ball, a yorker, a wide outside off — does each *read* differently in
   time to respond?

**Watch:** timing-window forgiveness, clarity of middled vs edged, shot-to-region
mapping, whether power feels rewarded without being automatic.

**Rate:** Timing clarity · Shot variety · Risk/reward readability · Overall control.

**Tuning hooks if it fails:** `BatTimingWindow` (forgiveness), `BoundaryScale`
(reward), contest `CleanContact` (mistime distinction).

---

## Plan 2 — Bowling feel

**Setup:** Bowl a full over to a Medium batter AI on each of neutral / GreenSeamer /
Turner.

**Procedure**
1. Bowl 3 stock good-length balls — is line/length input precise and repeatable?
2. Bowl an outswinger then an inswinger — is the movement visible and controllable?
3. Bowl a bouncer and a yorker — do length extremes land where aimed?
4. As a spinner: bowl an off-break and a wrong'un — is turn legible and the variation
   distinct?

**Watch:** input-to-release latency, how much the surface visibly changes movement,
whether setting a batter up over an over feels possible.

**Rate:** Aim precision · Movement legibility · Variation distinctness · Surface feedback.

**Tuning hooks:** `SwingStrength`, `SpinStrength`, `BounceVariation`; bowler
`ExecutionScatter` via difficulty.

---

## Plan 3 — Fielding feel

**Setup:** Trigger catches (edge to slip, skier to deep), ground stops, and a run-out
chase on neutral pitch.

**Procedure**
1. Take a regulation catch and a hard chance — is the catch window fair?
2. Field a drive in the ring and attempt a run-out — does the throw/run-out timing feel
   agential, not scripted?
3. Misfield deliberately — are consequences (overthrows, extra runs) legible?

**Watch:** anticipation vs reaction, whether the predictor's intercept point looks
right, throw ballistics readability.

**Rate:** Catch fairness · Intercept believability · Run-out agency · Feedback clarity.

**Tuning hooks:** fielding predictor parameters (separate from balance config);
cross-check run-out frequency in `Metrics.csv` (currently a touch high at ~18 % of
dismissals).

---

## Plan 4 — Camera usability

**Setup:** Play a full over each in: batting cam, bowling cam, broadcast/replay.

**Procedure**
1. Bat an over — can you track the ball from release to contact without losing it?
2. Bowl an over — does the camera frame line/length usefully?
3. Watch an auto-replay of a boundary and a wicket — is the cut timely and the angle
   informative?
4. Trigger a swing/spin physics-viz replay — is the movement readable?

**Watch:** ball trackability, framing during running, transition jarring, replay cut
timing.

**Rate:** Ball trackability · Framing · Transition smoothness · Replay usefulness.

---

## Plan 5 — Control responsiveness

**Setup:** Cricket-07 control scheme, neutral pitch. Run the input-latency probe
(`cricket.Input.Debug 1`).

**Procedure**
1. Rapid shot-direction changes — does intent map 1:1 to footwork/shot?
2. Bowling run-up + release timing — is the commit point clear?
3. Running between wickets: call, turn, dive — are the windows fair?
4. Context switches (bat → field on dismissal) — does input context follow the game?

**Watch:** dropped/misread inputs, perceived latency, key-combo discoverability,
context-switch correctness.

**Rate:** Input fidelity · Perceived latency · Timing fairness · Context handling.

---

## Session scorecard (template)

| Dimension | Build | Preset | Rating (1–5) | Top issue | Quote |
|---|---|---|---|---|---|
| Batting feel | | | | | |
| Bowling feel | | | | | |
| Fielding feel | | | | | |
| Camera usability | | | | | |
| Control responsiveness | | | | | |

**Sign-off:** median ≥ 4 across dimensions over ≥ 5 testers, with no dimension's median
below 3, on the shipped balance preset.
