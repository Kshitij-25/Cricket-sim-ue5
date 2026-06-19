# CricketSim — Known Issues (Vertical Slice Release Candidate)

_Last updated: 2026-06-18. Audit performed during the RC hardening pass._

This is the honest blocker/issue list for the vertical slice. Issues are ranked
by severity. "Blocker" = prevents a shippable, runnable public build. "Major" =
materially hurts the experience but does not stop a build. "Minor" = polish.

---

## BLOCKER

### B1 — No cookable content / startup map
**Status: open. Owner: content/level design (cannot be resolved in code alone).**

`Content/` contains **zero assets** (no `.umap`, no `.uasset`). `DefaultEngine.ini`
sets `GameDefaultMap=/Game/Maps/L_Nets`, which does not exist. Consequences:

- `Scripts/package_mac.sh` (RunUAT `BuildCookRun`) will **fail at cook** — there is
  no startup map to cook, and no levels containing the gameplay actors
  (`ACricketMatchRunner`, `ACricketBattingRig`, `ACricketBowlingRig`,
  `ACricketFieldingRig`, `ACricketBall`, stadium, pawns).
- The project is currently validated entirely at the **headless / simulation
  tier** (122 automation tests + the AI-vs-AI batch validation), which is robust,
  but a *playable* packaged slice needs at least one authored level.

**What unblocks it (in-editor, ~half a day):**
1. Create `/Game/Maps/L_Nets` (a nets practice level) and/or `/Game/Maps/L_Match`.
2. Place the gameplay actors (or a `GameMode` that spawns them) and a
   `PlayerStart`. The C++ already auto-possesses (`ACricketMatchRunner`,
   `ACricketPlayerPawn`), so a near-empty level with the actor placed is enough.
3. Add the map to **Project Settings ▸ Packaging ▸ List of maps to include**.
4. Re-run `Scripts/package_mac.sh Shipping`.

Code, modules, and the Shipping link step are all verified working (see
`STABILITY_REPORT.md`) — content authoring is the only remaining gate.

---

## MAJOR

### M1 — Save/load is not implemented
There is no `USaveGame` / serialization layer. Matches are deterministic from a
seed (`ACricketMatchRunner::Seed` + `Hash01`), so "resume" is not currently a
feature. If save/load is in scope for the slice it must be authored; otherwise it
should be cut from the feature list for this milestone (recommended).

### M2 — In-engine playable flow is unverified
Because of B1, the following could only be validated at the simulation tier, not
by playing a packaged build:
- Full T20 completion **in a level** (the rules engine itself is test-covered).
- Human-vs-AI in a real level (input + AI cores are test-covered in isolation).
- Replay capture/playback **in a level** (the replay core is test-covered).
Once B1 is resolved these move to the QA checklist's manual-play section.

---

## MINOR

### m1 — Debug components still tick in Shipping (zero visible output)
The `UCricket*DebugComponent`s are created as default subobjects on gameplay
actors. After the RC pass their overlays default **off** (cvars `0`) and the
rig/runner on-screen help is compiled out under `#if UE_BUILD_SHIPPING`, and UE's
`DrawDebug*` are no-ops in Shipping — so **nothing renders**. The components still
ticka (negligibly) in Shipping. A future cleanup can wrap the
`CreateDefaultSubobject<...Debug...>` calls in `#if !UE_BUILD_SHIPPING` to drop
them entirely. Tracked, not blocking.

### m2 — `ProjectVersion` is `0.1.0`
Bumped to `0.1.0-rc1` in `DefaultGame.ini` for the RC. Update on each release.

### m3 — Two AI balance bands at "Hard" tier sit in Warn, not Pass
`Death run-rate` and `chase-success%` at the Hard difficulty tier are in-band at
the Simulation tier but Warn at Hard (see `VALIDATION_REPORT.md`/`AI_EVALUATION.md`).
This is tracked, intentional, and documented — not a regression.

---

## RESOLVED during the RC hardening pass
- ✅ `CricketSim.Bat.EdgeImpact` automation test **fixed** (was the one long-standing
  failure). Root cause: the bat-ball model used a single flat face normal, so an
  off-centre hit changed only restitution/mass (pace) but never the impulse
  *direction* — no sideways squirt, no pace bleed. Added blade-curvature
  (`FCricketBatProfile::EdgeCurvature`): off-centre contacts now get an oblique
  contact normal. **All 122 tests pass.**
- ✅ Debug overlays no longer default on (all `cricket.Debug.*` draw cvars → `0`).
- ✅ Developer harness HUD/help text compiled out of Shipping.
- ✅ Game target now links all 10 runtime modules; **Shipping compiles clean**.
- ✅ Telemetry: `UCricketDiagnosticsSubsystem` (crash breadcrumb hooks + error log
  + match analytics CSV) added.
