# CricketSim — QA Checklist (Vertical Slice RC)

How to use: run the **Automated** block first (one command, must be all-green).
Then work the **Manual** blocks. Manual play-in-level items are marked 🔒 if they
are currently blocked by Known Issue **B1** (no cookable map) — they become
runnable once a level is authored.

Legend: `[ ]` not run · `[P]` pass · `[F]` fail (file in `KNOWN_ISSUES.md`).

---

## 0. Automated regression gate (run every change)

```sh
Scripts/run_tests.sh            # full suite; non-zero exit on any failure
```

- [ ] Full suite: **122 / 122 pass**, exit 0.
- [ ] Editor compiles: `Scripts/build.sh CricketSimEditor Development`.
- [ ] Shipping compiles + links: `Scripts/build.sh CricketSim Shipping`.

Targeted suites (for faster iteration):
`Scripts/run_tests.sh CricketSim.Physics+CricketSim.Bat+CricketSim.Match` etc.

---

## 1. Match-completion checklist

- [ ] **Full T20 completes** (rules engine): `CricketSim.Match` suite green
      (scoring, extras, dismissals, strike rotation, over/innings/result).
- [ ] First innings closes on 20 overs **or** all-out, whichever first.
- [ ] Innings break → second innings sets the correct target.
- [ ] Chase ends correctly on target reached, all-out, or overs exhausted.
- [ ] Result string is correct for: win-by-runs, win-by-wickets, tie.
- [ ] 🔒 Full match played end-to-end in a level via `ACricketMatchRunner`
      (auto-play to completion, scoreboard + result shown).
- [ ] Match result is written to `Saved/Analytics/Matches.csv` (one row/match).

## 2. AI validation checklist

- [ ] `CricketSim.AI` suite green (difficulty model, awareness, brains, headless
      T20 sim: run/wicket rates, bowling patterns, shot distribution).
- [ ] `CricketSim.Balance` green; `Saved/Validation/*.md` + `Metrics.csv` written.
- [ ] Aggregate metrics graded against T20 benchmarks: Pass/Warn counts as
      expected (the two tracked Hard-tier Warn bands are acceptable — see
      `AI_EVALUATION.md`).
- [ ] AI never scripts an outcome — it produces intent only; results emerge from
      physics/rules (architectural invariant, see `AI_SYSTEM.md`).
- [ ] 🔒 AI-vs-AI match in a level plays to a believable score without stalls.
- [ ] 🔒 Human-vs-AI: AI bowls legal overs, sets fields, chases sensibly.

## 3. Physics validation checklist

- [ ] `CricketSim.Physics` green (drag crisis, Magnus carry/dip/drift, swing,
      reverse swing, wobble seam, spin decay, RK4 determinism).
- [ ] `CricketSim.Pitch` green (restitution, grip/skid, turn, seam, bounce var).
- [ ] `CricketSim.Bat` green incl. **EdgeImpact** (edge deflects + bleeds pace).
- [ ] `CricketSim.Bowling` / `CricketSim.Fielding` green.
- [ ] Determinism: same seed → identical match (re-run `run_tests.sh`, compare).
- [ ] No NaNs/inf in ball state over a long flight (covered by physics tests).
- [ ] 🔒 Visual spot-check in a level: a fast bowler's swing curves; spin turns
      off the pitch; a middled drive flies straight; an edge flies to slip/keeper.

## 4. Replay & camera checklist

- [ ] `CricketSim.Camera` green (framing/transitions, record/playback, slow-mo,
      swing/spin viz).
- [ ] 🔒 In a level: replay last delivery (V), scrub, pause, change speed, exit.
- [ ] 🔒 Replay capture does not hitch the live frame (see PERFORMANCE_REPORT).

## 5. UI / HUD / Audio / Presentation

- [ ] `CricketSim.UI` green (score/wicket/over/replay/physics-overlay view-models).
- [ ] `CricketSim.Audio` green (reactive cue selection, crowd controller, routing).
- [ ] `CricketSim.Presentation` green (event classifier, directors, crowd arc).
- [ ] 🔒 HUD shows live score/over/wickets in a level; widgets render (cvar
      `cricket.UI.Widgets 1`, default on).

## 6. Release-build hygiene

- [ ] No debug overlay renders by default (all `cricket.Debug.*` draw cvars = 0).
- [ ] Developer rig/runner help text absent in a Shipping build (compiled out).
- [ ] Developer tools still reachable in Development via cvars / project settings.
- [ ] `ProjectVersion` updated for the release.

## 7. Packaging (post-B1)

- [ ] `Scripts/package_mac.sh Shipping` cooks + stages + paks + archives.
- [ ] Packaged app launches into the startup map.
- [ ] App quits cleanly; `Saved/Analytics/Matches.csv` + logs written.
- [ ] Crash breadcrumb appears in the log on a forced fault (sanity-test the hook).

## 8. Save/load

- [ ] N/A for this slice (not implemented — see KI M1). Confirm it is cut from the
      release feature list, or schedule it.
