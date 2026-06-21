# CricketSim — Vertical Slice Release Candidate

_Status as of 2026-06-18 hardening pass. Version `0.1.0-rc1`._

This is the index for the RC milestone. The objective was to **stabilize, harden,
polish, and package** the feature-complete simulation into a release-quality
vertical slice — not to add gameplay systems.

## Deliverables

| # | Deliverable | Status | Where |
|---|---|---|---|
| 1 | Release Candidate build | ✅ Code-complete + content authored + packaged | `KNOWN_ISSUES.md` (B1 resolved), `VERTICAL_SLICE_VALIDATION_REPORT.md` |
| 2 | QA checklist | ✅ | `QA_CHECKLIST.md` |
| 3 | Packaging pipeline | ✅ | `Scripts/`, `RELEASE_BUILD.md` |
| 4 | Stability report | ✅ | `STABILITY_REPORT.md` |
| 5 | Performance report | ✅ | `PERFORMANCE_REPORT.md` |
| 6 | Known issues report | ✅ | `KNOWN_ISSUES.md` |
| 7 | Technical documentation | ✅ | `RELEASE_BUILD.md`, `ARCHITECTURE.md`, `*_SYSTEM.md` |

## Headline state

- **122 / 122 automation tests pass.** The single long-standing failure
  (`Bat.EdgeImpact`) was root-caused and fixed (blade-curvature in the bat-ball
  model).
- **Editor (Development) and Game (Shipping) both compile and link clean.** The
  game target now packages all 10 runtime modules.
- **Debug overlays are off by default and compiled out of Shipping**; developer
  tools remain available in Development.
- **Telemetry in place**: crash breadcrumb hooks, structured error logging, and
  per-match analytics CSV via `UCricketDiagnosticsSubsystem`.

## Content authoring — resolved 2026-06-20

`/Game/Maps/L_Nets` and `/Game/Maps/L_Match` now exist (created via
`Scripts/setup_content.sh`, a repeatable Python editor-scripting pass), and
`Scripts/package_mac.sh Shipping` succeeds end-to-end on macOS — see
`Docs/VERTICAL_SLICE_VALIDATION_REPORT.md` for the full verification record,
including what still needs manual visual/interactive confirmation on real
hardware (no display was available in the environment this pass ran in).

## What changed this pass (code)

- `CricketBatCollision.cpp` / `CricketBatTypes.h` — blade-curvature edge physics.
- All `cricket.Debug.*` draw cvars default `0`; rig/runner HUDs gated under
  `#if UE_BUILD_SHIPPING`; pitch-debug master default off.
- `CricketDiagnosticsSubsystem.{h,cpp}` — new telemetry/diagnostics hub; wired into
  `ACricketMatchRunner`.
- `Source/CricketSim.Target.cs` — links all runtime modules for packaging.
- `Config/DefaultGame.ini` — version `0.1.0-rc1`, packaging settings + maps list.
- `Scripts/build.sh`, `run_tests.sh`, `package_mac.sh` — build/test/package pipeline.
- `Docs/` — this RC doc set.

## Sign-off gate

Run `QA_CHECKLIST.md` §0 + §6. When B1 is resolved, complete the 🔒 manual items
and §7 packaging, then tag.
