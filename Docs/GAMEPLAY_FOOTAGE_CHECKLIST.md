# CricketSim — Gameplay Footage & Manual Testing Checklist

Use this to capture the footage/evidence for the vertical-slice sign-off and to
drive the 10 Human vs AI sessions required by the validation pass. Run against
the **packaged Shipping build** (`Build/Mac/CricketSim-Mac-Shipping.app`), not PIE,
for the final capture — PIE is fine for iterating.

---

## 0. Setup

- [ ] `Scripts/setup_content.sh` ran clean (creates `L_Nets` + `L_Match` + data assets).
- [ ] `Scripts/package_mac.sh Shipping` exits 0.
- [ ] Packaged app launches and loads into `L_Nets` (the configured `GameDefaultMap`).

---

## 1. L_Nets — Human batting + bowling (interactive)

Capture 1–2 minutes of continuous footage per item; note any anomaly immediately
(timestamp + description) rather than waiting until the end.

- [ ] **Footwork** — D (front foot), W (back foot), S (defensive) each visibly change stance.
- [ ] **Shot direction** — ↑ straight, → off, ← leg, ↓ fine, E cover, Q midwicket all distinguishable.
- [ ] **Shot execution** — Space / LMB plays the shot; timing window affects outcome (edge / clean hit / miss).
- [ ] **Lofted shot** — hold LeftShift + Space; ball should carry further / higher than a grounded shot of the same direction.
- [ ] **Auto-feed** — deliveries arrive every ~3.5 s without manual intervention (`FeedInterval`).
- [ ] **Camera** — C cycles camera, B = ball-follow, F = free cam; no camera clipping through ground.
- [ ] **Replay** — V replays the last delivery; P pauses, [ / ] change speed, , / . step frame-by-frame.
- [ ] **HUD panels present**: Batting, Bowling, Ball-physics. **Scoreboard correctly empty** (no `MatchRunner` in this level — by design, not a bug).
- [ ] **No black viewport** (lighting present), no missing-mesh warnings in the on-screen log.

---

## 2. L_Match — AI vs AI full T20 (auto-play)

- [ ] Match starts automatically (`bAutoPlay=true`), toss happens, first ball is bowled within a few seconds.
- [ ] **Scoreboard panel populates**: score, overs, current batters, current bowler, run rate update ball-by-ball.
- [ ] Innings break occurs at the correct over count (20 overs or all-out).
- [ ] Second innings starts with the correct chase target displayed.
- [ ] Match concludes with a clear result (win margin or tie) — capture the final scoreboard frame.
- [ ] **No crash, freeze, or stall** across the full ~20-over innings × 2.
- [ ] Press Enter to toggle auto-play off/on; ↑/↓ change auto-play speed (Development build only — compiled out of Shipping; verify via Development package or PIE).

---

## 3. AI vs AI — 10 match batch (statistical capture)

Run via `Scripts/validate_vertical_slice.sh` (Phase 2/3) or manually replay
`L_Match` 10 times with different seeds (`Seed` property on `CricketMatchRunner`).

For each of the 10 matches record:

| # | Seed | 1st Inn Score | 2nd Inn Score | Wickets (avg) | Run Rate | Boundaries (4s+6s) | Result | Crash? |
|---|---|---|---|---|---|---|---|---|
| 1 | 12345 | | | | | | | |
| 2 | 12346 | | | | | | | |
| 3 | 12347 | | | | | | | |
| 4 | 12348 | | | | | | | |
| 5 | 12349 | | | | | | | |
| 6 | 12350 | | | | | | | |
| 7 | 12351 | | | | | | | |
| 8 | 12352 | | | | | | | |
| 9 | 12353 | | | | | | | |
| 10 | 12354 | | | | | | | |

**Averages** (fill after all 10): Score: __ · Wickets: __ · Run rate: __ · Boundaries: __

> Note: the automated `CricketSim.Balance.ValidationReport` test already runs 80
> matches at the simulation tier and is the statistically robust source (see
> `Docs/VALIDATION_REPORT.md`). This 10-match table is the lighter, level-based
> spot-check requested for the vertical-slice deliverable specifically.

---

## 4. Human vs AI — 10 session matrix

Use `L_Nets` (human batting/bowling vs the auto-feeder / AI brains). Record per session:

| # | Mode | Duration | Deliveries faced/bowled | Runs scored / conceded | Crash? | Notes |
|---|---|---|---|---|---|---|
| 1 | Human batting | | | | | |
| 2 | Human batting | | | | | |
| 3 | Human batting | | | | | |
| 4 | Human batting | | | | | |
| 5 | Human batting | | | | | |
| 6 | Human bowling | | | | | |
| 7 | Human bowling | | | | | |
| 8 | Human bowling | | | | | |
| 9 | Human bowling | | | | | |
| 10 | Human bowling | | | | | |

> "Human bowling" in this slice means controlling the bowler-side timing inputs
> via `CricketPlayerInputComponent`'s bowling layer (if bound) or via
> `CricketBowlingRig` if placed instead of `CricketPlayerPawn` — confirm which
> rig is in the level before the session and note it.

---

## 5. Replay system

- [ ] Record a delivery, trigger replay (V), confirm camera angle differs from live play.
- [ ] Step frame-by-frame (`,` / `.`) — ball position changes smoothly, no teleporting.
- [ ] Change playback speed (`[` / `]`) — speed visibly changes without desync.
- [ ] Replay of a boundary-scoring shot shows the ball crossing the rope correctly.

---

## 6. Packaging & launch evidence

- [ ] Screenshot/log of `Scripts/package_mac.sh Shipping` ending in `BUILD SUCCESSFUL`.
- [ ] Screenshot of the produced `.app` under `Build/Mac/`.
- [ ] Screenshot/video of the packaged app launching standalone (double-click, not via editor) on macOS.
- [ ] Output log excerpt showing no "Could not find object for asset" errors during cook.

---

## 7. Crash & stability log

Record any crash immediately — do not wait until the session ends.

| Timestamp | Level | Action that triggered it | Crash log path | Repro? |
|---|---|---|---|---|
| | | | | |

If zero crashes across all sessions above, state that explicitly in the
validation report rather than leaving the table conspicuously blank.
