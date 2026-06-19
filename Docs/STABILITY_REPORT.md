# CricketSim — Stability Report (Vertical Slice RC)

_Generated 2026-06-18 during the Release-Candidate hardening pass._

## Summary

| Area | Status |
|---|---|
| Editor build (CricketSimEditor, Mac, Development) | ✅ Succeeds |
| Game build (CricketSim, Mac, **Shipping**) — compile + link | ✅ Succeeds |
| Automation tests (`Automation RunTests CricketSim`) | ✅ **122 / 122 pass** |
| Known crashes | None reproduced in headless runs |
| Known gameplay blockers (code) | None; content gap is the only blocker (KI B1) |
| Major physics bugs | None open (EdgeImpact fixed this pass) |

## Build verification

Both targets were compiled clean during this pass:

```
Scripts/build.sh CricketSimEditor Development   # → Result: Succeeded
Scripts/build.sh CricketSim       Shipping      # → Result: Succeeded
```

The Shipping compile is significant: it exercises every `#if UE_BUILD_SHIPPING`
path added this pass (developer-overlay compile-out) and links all **10 runtime
modules** named in `Source/CricketSim.Target.cs`. Previously the game target named
only 3 modules; the top-of-graph modules (UI/Audio/AI/Performance/Presentation)
are now linked into the packaged executable.

## Test results

Full suite run headless (`-nullrhi -unattended`). 122 tests across:
`Physics, Bowling, Bat, Pitch, Batting, Fielding, Match, Anim, Camera, Stadium,
Input, UI, Audio, AI, Perf, Presentation, Balance`.

Result: **122 Success / 0 Fail.**

Results are parsed from `~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log`
(`Result={Success}` / `Result={Fail}`), not stdout. `Scripts/run_tests.sh` does
this and returns a non-zero exit on any failure.

## Stability work done this pass

1. **Fixed the one long-standing test failure — `CricketSim.Bat.EdgeImpact`.**
   The bat-ball collision modelled the blade as a single flat plane, so an
   off-centre contact changed only restitution and effective mass (it softened the
   hit) but never changed the impulse *direction*. Real edges squirt sideways and
   bleed pace because the blade surface curves away toward the edge/toe. Added
   `FCricketBatProfile::EdgeCurvature`; the resolver now tilts the effective
   contact normal toward the edge proportional to the contact offset. Verified no
   regressions: all Bat/Batting tests and the full suite still pass.

2. **Crash/error diagnostics hooks.** `UCricketDiagnosticsSubsystem`
   (`UGameInstanceSubsystem`, auto-creates in every Game/PIE world) binds
   `FCoreDelegates::OnHandleSystemError` to flush a breadcrumb (build, config, last
   match context) at the instant of an unhandled fault — complementing UE's
   CrashReportClient. See `Docs/RELEASE_BUILD.md` ▸ Telemetry.

3. **Debug-overlay hardening** (see `Docs/KNOWN_ISSUES.md` ▸ Resolved). No overlay
   renders by default; developer harness HUDs compile out of Shipping.

## Risk notes

- The only thing preventing a *runnable* packaged build is content (KI **B1**),
  not code. Everything code-side that can be verified headlessly is green.
- Determinism is a load-bearing property (physics + match flow are seed-driven and
  have explicit `Determinism` tests). Keep new gameplay code free of
  `FMath::Rand`/wall-clock reads; route randomness through the existing seeded
  hashing as the runner/sim already do.
