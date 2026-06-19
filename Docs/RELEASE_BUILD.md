# CricketSim — Build, Test, Package & Telemetry (Release Candidate)

The operational manual for the vertical slice. For the deep architecture see
`ARCHITECTURE.md`; for per-system design see the other `Docs/*_SYSTEM.md`.

---

## Prerequisites

- macOS on Apple Silicon, Unreal Engine **5.7** installed.
- Default engine path: `/Users/Shared/Epic Games/UE_5.7`. Override with the
  `UE_ROOT` environment variable for all scripts below.

## Scripts (in `Scripts/`)

| Script | Purpose |
|---|---|
| `build.sh [Target] [Config]` | Compile a target. `Target`=`CricketSimEditor`(default)\|`CricketSim`. `Config`=`Development`(default)\|`Shipping`\|`Test`\|`DebugGame`. Compile only — no cook. |
| `run_tests.sh [Suite]` | Headless automation tests. `Suite` defaults to `CricketSim` (all). Parses UE's log, exits non-zero on failure (CI-friendly). |
| `package_mac.sh [Config] [OutDir]` | Cook + stage + pak + archive a standalone macOS build via RunUAT `BuildCookRun`. `Config`=`Shipping`(default). **Requires content — see KNOWN_ISSUES B1.** |

### Day-to-day
```sh
Scripts/build.sh                 # editor, Development
Scripts/run_tests.sh             # full suite (expect 122/122)
```

### Release codepath validation
```sh
Scripts/build.sh CricketSim Shipping     # proves the shipping link + #if guards
```

### Packaging (after a startup map exists)
```sh
Scripts/package_mac.sh Shipping          # → Build/Mac/
```

## Build configurations

| Config | Use | Debug overlays | Perf scopes |
|---|---|---|---|
| Development | dev + manual QA + on-device profiling | available (cvar-gated, default off) | on |
| Test | automated perf/regression | available | on |
| **Shipping** | **release / testers** | **compiled out** | compiled out |

The Game target (`Source/CricketSim.Target.cs`) links all 10 runtime modules so
they cook into the packaged build.

---

## Telemetry & diagnostics

`UCricketDiagnosticsSubsystem` (`CricketSim` module, a `UGameInstanceSubsystem`)
auto-creates in every Game/PIE world — no level wiring. It does three things and
**never** affects gameplay:

1. **Crash reporting hook** — binds `FCoreDelegates::OnHandleSystemError` and
   flushes a breadcrumb (build version, config, last match context) to the log the
   moment the process faults. Complements UE's CrashReportClient (whose configs
   already live under `Saved/Config/CrashReportClient/`).
2. **Error logging** — `RecordError(Context, Detail)` funnels gameplay faults
   through the `LogCricketDiag` log category in one greppable format.
3. **Match analytics** — `RecordMatchStart(...)` / `RecordMatchResult(...)` append
   a row per match to `Saved/Analytics/Matches.csv`
   (`Timestamp,Context,Decided,Tie,Winner,HomeScore,HomeWickets,AwayScore,AwayWickets,Summary`).
   `ACricketMatchRunner` calls these automatically; any other match driver can too.

Offline/aggregate analytics (run-rate/economy/benchmark grading over a population
of matches) live separately in `CricketMatchAnalytics` and write to
`Saved/Validation/` (see `BALANCING_FRAMEWORK.md`).

Grep the log for diagnostics:
```sh
grep "LogCricketDiag" "$HOME/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log"
```

---

## Module map (runtime dependency graph, bottom → top)

```
CricketPerfCore  (instrumentation primitives; depends only on Core/Engine)
   └─ CricketPhysics      (SI aerodynamics, integrator, pitch, bat collision)
        └─ CricketGameplay (actors/components: ball, rigs, fielder, anim, input)
             └─ CricketSim  (T20 rules, match engine, runner, GAME MODULE,
             │               diagnostics subsystem)
             ├─ CricketAI          (brains, difficulty, headless match sim)
             ├─ CricketUI          (HUD data-binding; CommonUI/UMG)
             ├─ CricketAudio       (reactive cue selection)
             ├─ CricketPerformance (perf manager, benchmark, settings)
             └─ CricketPresentation(broadcast directors, crowd arc)
CricketSimEditor  (Editor-only)
```

Invariants worth protecting: physics is pure SI and deterministic; UI/Audio/AI
never feed back into physics outcomes; the AI produces *intent*, not results.

---

## Release checklist pointer

Before tagging a build, run `Docs/QA_CHECKLIST.md` §0 (automated gate) and §6
(release hygiene). The full deliverable status is in `Docs/RELEASE_CANDIDATE.md`.
