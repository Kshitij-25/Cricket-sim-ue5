# CricketSim — Development Roadmap

Phased plan toward the long-term goal: **the most realistic cricket simulation
ever made.** Physics realism first; presentation last. Each phase has concrete
exit criteria — do not advance until they are met.

Priorities (fixed): **1) physics realism · 2) clean architecture · 3) scalability
· 4) fast iteration.**

---

## Phase 0 — Foundation ✅ (this commit)

Engine architecture, module split, and the ball-physics core.

- [x] UE 5.7 project, four-module split (`CricketPhysics` → `CricketGameplay` →
      `CricketSim` → `CricketSimEditor`), macOS build green.
- [x] Ball physics core: aerodynamics (drag/Magnus/swing/wobble), RK4 integrator,
      pitch interaction (bounce/seam/turn). SI, deterministic.
- [x] Data-driven tuning hooks (`FCricketAeroCoefficients`, `UCricketBallProfileAsset`).
- [x] Gameplay bridge: `ACricketBall` + `UCricketBallPhysicsComponent`.
- [x] Docs: ARCHITECTURE, BALL_PHYSICS, ROADMAP.

**Exit:** project compiles & opens on macOS; a ball can be released in PIE and
flies/bounces under the model.

---

## Phase 1 — Ball physics validation & tuning  *(next)*

Make the model demonstrably real before building anything on top of it.

- [ ] **Headless physics tests** (`CricketPhysics` automation tests): energy
      sanity, determinism (same input → identical trajectory), Magnus sign per
      spin axis, swing direction per regime.
- [ ] **Trajectory harness**: a dev actor/console command that bowls a delivery
      from authored inputs and logs/plots the path (lateral & vertical deviation
      vs distance).
- [ ] **Reference calibration**: tune coefficients so the validation checklist in
      `BALL_PHYSICS.md` §8 reproduces real magnitudes (e.g. ~0.5–0.8 m of swing
      over 20 m; spinner drift/dip in the right ballpark).
- [ ] **Ball profiles**: author `UCricketBallProfileAsset` presets (new
      Kookaburra, 40-over reversing, dewy white ball).

**Exit:** all checklist deliveries reproduce from inputs only; trajectories
match reference within agreed tolerance; tests pass in CI on macOS.

---

## Phase 2 — Bowling

Turn human intent into physically-grounded release state.

- [x] **Pitch model**: a first-class pitch simulation — `FCricketSurfacePatch`
      (hardness/roughness/moisture/grass/friction/restitution/wear/unevenness),
      `FCricketPitchMaterialLibrary` (Hard/Dry/Green types), the pitch physics
      module `FCricketPitchInteraction` + bounce/spin/seam solvers,
      `UCricketPitchProfileAsset` (length zones + wear), `UCricketPitchDebugComponent`,
      and the `CricketSim.Pitch` comparative test suite. Deterioration / footmarks /
      day progression are designed-for (data + hooks) but not yet active.
      See `Docs/PITCH_SYSTEM.md`.
- [ ] **Bowler release**: map run-up/action + aim + seam preset + pace into
      `Release()` inputs; seedable human scatter applied to inputs only.
- [ ] **Bowling control scheme** (EnhancedInput): line, length, pace, seam
      orientation, cross-seam/wobble, stock vs variation.
- [ ] **Ball ageing**: shine/roughness/seam wear over overs → enables reverse.

**Exit:** a player can bowl repeatable, distinct deliveries (outswinger, off-
break, wobble-seam yorker) that behave correctly off the surface.

---

## Phase 3 — Batting & bat–ball collision

The other emergent-physics centrepiece.

- [x] **Bat collision model**: analytic impulse on contact (sweet-spot / edge /
      toe response, restitution by impact location, bat speed & angle → exit
      velocity and spin). Resolved by our model, contact found via Chaos query.
      `FCricketBatCollision` + the `CricketSim.Bat` suite.
- [x] **Batting input**: footwork + shot type + timing window → bat swing plane &
      speed. Timing/placement bias *inputs*; outcome stays emergent. The bat is
      animated through backlift→downswing→follow-through over time and contact is
      detected geometrically (`FCricketSwingModel`, `UCricketBattingComponent`,
      `ACricketBattingRig`, debug overlay, `CricketSim.Batting` suite).
      See `Docs/BATTING_SYSTEM.md`.
- [x] **Edges, mistimes, aerial vs grounded** all fall out of collision geometry —
      a late/early swing or wrong-foot stroke displaces the contact off the sweet
      spot; nothing is injected.
- [ ] **Stumps & wicket** collision; bowled/hit-wicket detection. *(Deferred: a
      rules concern, out of scope for the batting-mechanics milestone.)*

**Exit:** identical deliveries produce different, physically-plausible shots from
different bat presentations; edges and mistimes emerge, not scripted. ✅ (strokes,
footwork & timing) — stumps/dismissal detection deferred to the rules phase.

---

## Phase 4 — Basic fielding

- [x] Fielder pawns with simple intercept/chase AI and catch/field/throw —
      `ACricketFielder` + `UCricketFielderComponent` (the Idle→Tracking→
      MovingToIntercept→Catching/PickingUp→Throwing→Returning state machine).
- [x] Catching as a physics interception (reach, reaction, difficulty), not a dice
      roll — `FCricketFieldingPredictor::SolveIntercept` over the real predicted path.
- [x] Throw + run-out resolution; boundary detection — `SolveThrow` ballistic aim,
      stumps/keeper/fielder targets, direct-hit detection, out-of-range = boundary.
- [x] Reusable Ball Prediction System (`FCricketFieldingPredictor`) for AI reuse,
      with the `CricketSim.Fielding` test suite and `cricket.Debug.Fielding` overlay.
      See `Docs/FIELDING_SYSTEM.md`.

**Exit:** a struck ball is fielded, caught, or goes to the boundary correctly;
run-outs resolvable. ✅ (MVP: chase/intercept/catch/pickup/throw/direct-hit all
emerge from the real prediction. Drop/fumble probability & run bookkeeping deferred
to the rules phase.)

---

## Phase 5 — Match rules (T20) & state machine

- [x] Ball-by-ball state machine — `UCricketMatchEngine` (PreMatch→Toss→
      FirstInnings→InningsBreak→SecondInnings→MatchComplete); `ApplyDelivery`
      drives delivery → outcome → bookkeeping → next ball/over/innings atomically.
- [x] Scoring, extras (wides/no-balls/byes/leg-byes), all five dismissals
      (`ECricketDismissal`), strike rotation, bowler-over limits (`FCricketMatchRules`).
      Full batting/bowling/match statistics; consume-physics seam
      (`FCricketBallResult` → `FCricketOutcomeInterpreter`) so the engine never
      alters a physics result. `CricketSim.Match` test suite. See `Docs/MATCH_ENGINE.md`.
- [x] India & Australia squads + a playable driver/HUD (`ACricketMatchRunner`)
      that runs a full match end-to-end. Target & result (win-by-runs / by-wickets /
      tie) computed.
- [ ] `ACricketGameState` replicating the scorecard; wire the engine onto
      `ACricketGameMode` for the shipping path. *(Deferred: replication/persistence.)*
- [ ] Powerplay field-restriction enforcement; one stadium/pitch asset. *(Deferred.)*

**Exit:** a full 20-over innings (and chase) plays out end-to-end with correct
rules, scores, and a result — headless or with placeholder visuals. ✅ (engine +
HUD runner; replication/stadium deferred.)

---

## Animation layer (cross-cutting, foundation in place)

The animation *architecture* is built physics-first: a code-side controller +
state machines + notify-timeline that an Anim Blueprint sits on, never an authority
over outcomes.

- [x] Four state machines (locomotion / bowling / batting / fielding) +
      `FCricketActionMontage`/`FCricketMontagePlayer` notify engine; locomotion
      classifier. `FCricketAnimationModel`, `CricketSim.Anim` test suite.
- [x] `UCricketCharacterAnimComponent` (the AnimController the AnimBP reads): derives
      locomotion from movement, FOLLOWS the batting/fielding sims, and TIMES the
      bowling release via the BallRelease notify. Gameplay notify hooks
      (release/impact/catch/throw). `UCricketAnimDebugComponent` (`cricket.Debug.Anim`).
- [x] Live Input→Animation→Physics integration in `ACricketBowlingRig` (run-up →
      BallRelease → `BowlNow`). See `Docs/ANIMATION_SYSTEM.md`.
- [ ] Authored skeleton/AnimBP/animation assets + IK/root-motion clips. *(Asset
      content, not code; the documented AnimBP structure is ready to build on.)*

---

## Camera & Replay layer (cross-cutting, foundation in place)

Cameras frame the sim and replays play back its recorded results — neither ever
alters physics.

- [x] Pure framing model + transitions + swing/spin measurement
      (`FCricketCameraModel`); eight camera modes (Batting/Bowling/Fielding/
      Spectator + Free/Orbit/BallFollow/PhysicsInspection).
- [x] Replay data models + playback cursor (`CricketReplayTypes`): capped-ring
      recording of ball/player/anim state + sparse events; slow-mo/pause/step/seek;
      interpolated sampling. `CricketSim.Camera` test suite.
- [x] `UCricketCameraDirectorComponent` (Camera Manager) + `UCricketReplayComponent`
      (Recorder + Playback + physics-viz overlays, `cricket.Debug.Replay`), wired
      live into `ACricketFieldingRig`. See `Docs/CAMERA_REPLAY_SYSTEM.md`.
- [ ] On-disk replay serialization + cinematic/broadcast camera packages. *(Deferred.)*

---

## Stadium & Environment layer (cross-cutting, foundation in place)

The venue as a simulation environment (geometry + rules), not a visual asset.

- [x] Pure ground geometry + boundary rules + field positions
      (`FCricketStadiumModel`): elliptical boundary, four/six classification from the
      real ball path, catch-at-boundary validation, parametric named field positions
      that scale with ground size. `CricketSim.Stadium` test suite.
- [x] `ACricketStadium` (Stadium Manager + Match Environment Controller): builds the
      ground from its transform, detects boundaries off the live ball, exposes field
      positions, and pushes the venue atmosphere (wind/humidity) onto the ball aero —
      a real effect on flight. Day/night + floodlights; debug viz (boundary, ring,
      pitch, positions, sight screens, umpires, pavilion, landing heatmap, wagon
      wheel), `cricket.Debug.Stadium`. See `Docs/STADIUM_SYSTEM.md`.
- [ ] Captain-AI field-setting (the placement model is built for it); weather sim
      beyond the wired aero. *(Architected, deferred.)*

This supersedes the "one stadium/pitch" item deferred in Phase 5.

---

## Player control layer (cross-cutting, foundation in place)

A Cricket-07-inspired, keyboard-first control scheme on Enhanced Input — controls
generate intent only; physics stays the source of truth.

- [x] Pure intent model (`FCricketInputModel`): the C07 foot×direction batting grid
      (7 strokes), bowling/running/fielding resolution, and the input-context rule.
      `CricketSim.Input` test suite.
- [x] Enhanced Input layer built in C++ (`UCricketPlayerInputComponent`): Input
      Actions + five stacked Mapping Contexts (Match/Batting/Bowling/Fielding/Replay),
      the Input State Manager, and the six per-domain controllers routing to the
      existing components. Gamepad future-proofed (extra MapKey lines).
      `UCricketInputDebugComponent` (`cricket.Debug.Input`), `ACricketPlayerPawn`
      showcase. See `Docs/INPUT_SYSTEM.md`.
- [ ] Gamepad mappings; two-player / AI-partner running comms. *(Architected, deferred.)*

---

## Phase 6 — Hardening, scalability, Windows

- [ ] Windows build green (DX12); verify physics parity macOS↔Windows.
- [ ] Performance pass (the model is cheap, but validate at match scale).
- [ ] Save/replay of deliveries (state snapshots) for testing & analysis.
- [ ] Tuning/validation editor tooling in `CricketSimEditor`.

**Exit:** identical results cross-platform; stable 60 fps match; deliveries
replayable bit-for-bit.

---

## Explicitly out of scope (per the brief)

Open-world, career mode, tournaments, multiplayer, microtransactions, live
service. Also deferred until *after* the simulation is real: UI/HUD, presentation,
commentary, crowds, broadcast cameras, cosmetics.

## Cross-cutting principles

- **Outcomes emerge from physics.** Ratings & input bias the *inputs* to the
  model (release pace, spin, bat speed/angle), never the result.
- **Determinism is sacred.** All randomness is explicit and seedable, applied at
  the input layer, never inside `CricketPhysics`.
- **Tune as data.** Coefficients live in assets; calibration never requires a recompile.
- **Test the core headlessly.** The physics module carries automation tests and
  must stay free of world/actor dependencies.
