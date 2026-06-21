# CricketSim — Known Issues (Vertical Slice Release Candidate)

_Last updated: 2026-06-20. B1 resolved this pass — see `VERTICAL_SLICE_VALIDATION_REPORT.md`._

This is the honest blocker/issue list for the vertical slice. Issues are ranked
by severity. "Blocker" = prevents a shippable, runnable public build. "Major" =
materially hurts the experience but does not stop a build. "Minor" = polish.

---

## RESOLVED

### B1 — No cookable content / startup map — ✅ RESOLVED 2026-06-20

`Content/Maps/L_Nets.umap` and `Content/Maps/L_Match.umap` now exist, along with
the recommended `/Game/Data/Balls` and `/Game/Data/Teams` data assets. Created
programmatically via `Scripts/setup_content.py` (Unreal Python editor scripting)
+ `Scripts/setup_content.sh` (headless driver) — re-runnable, not a one-off manual
editor session. `Scripts/package_mac.sh Shipping` now succeeds end-to-end:
`BUILD SUCCESSFUL`, artifact at `Build/Mac/CricketSim-Mac-Shipping.app`, no
cook errors. Full detail: `Docs/VERTICAL_SLICE_VALIDATION_REPORT.md`.

---

## MAJOR

### M1 — Save/load is not implemented
There is no `USaveGame` / serialization layer. Matches are deterministic from a
seed (`ACricketMatchRunner::Seed` + `Hash01`), so "resume" is not currently a
feature. If save/load is in scope for the slice it must be authored; otherwise it
should be cut from the feature list for this milestone (recommended).

### M2 — In-engine playable flow — partially verified, visual layer still open
B1 is resolved and the packaged app launches into both `L_Nets` and `L_Match`
without crashing (process liveness + clean exit confirmed). What remains
unverified specifically:
- Visual confirmation (HUD panels, scoreboard, ball, camera framing) — the
  environment this pass ran in has no functional display (`screencapture`
  fails), so nobody has *looked* at the running game yet.
- Human-vs-AI interactive feel in a real level (input + AI cores are
  test-covered in isolation, but not played by a human).
- Replay capture/playback **visually** (the replay core is test-covered headlessly).

Action: run `Docs/GAMEPLAY_FOOTAGE_CHECKLIST.md` on real hardware with a display.
Full T20 completion and AI-vs-AI are no longer blocked — see `B1` (resolved) and
`Docs/VERTICAL_SLICE_VALIDATION_REPORT.md` §2–3.

### M3 — Match analytics CSV does not appear in the packaged macOS build
`Saved/Analytics/Matches.csv` was not found anywhere (project tree or the app's
sandbox container) after a full `L_Match` run in the packaged Shipping app. Likely
cause: the packaged app runs under macOS App Sandbox
(`Build/Mac/Resources/Sandbox.*.entitlements`), and
`FFileHelper::SaveStringToFile`'s write silently no-ops outside the sandbox
container — already flagged as a risk in `CONTENT_AUTHORING_CHECKLIST.md` §9 and
confirmed to occur in this pass. Never crashes; just produces no file. Fix
options: write inside `FPaths::ProjectSavedDir()` resolved *within* the sandbox
container, or add a file-access entitlement if persistent telemetry outside the
sandbox is required.

### M4 — Project pinned to UE 5.7; this pass validated against UE 5.8
UE 5.7 was not available to install in this environment; UE 5.8 was used instead.
Two small compatibility fixes were required (`bOverrideBuildEnvironment = true`
in both `Target.cs` files; `RHIGetGPUFrameCycles()` replacing the removed
`GGPUFrameTime` extern in `CricketPerformanceSubsystem.cpp`) — see
`Docs/VERTICAL_SLICE_VALIDATION_REPORT.md` §1 for detail. Both are inert on a
correctly matched 5.7 install. **Recommendation:** re-run the full test +
package + manual-play pass on an actual UE 5.7 install before treating this as
the final sign-off, since 5.7 itself was never exercised this pass.

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

## RESOLVED during the content-authoring / vertical-slice pass (2026-06-20)
- ✅ **B1** — `L_Nets` + `L_Match` levels and recommended data assets created
  (see RESOLVED section above and `VERTICAL_SLICE_VALIDATION_REPORT.md`).
- ✅ Packaging verified end-to-end on macOS: `Scripts/package_mac.sh Shipping`
  produces a working, launchable `.app` with no cook errors.
- ✅ UE 5.8 compatibility fixes (see M4) — 122/122 tests still pass, no
  behavioral drift in AI-vs-AI statistical metrics vs. the pre-existing baseline.

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
