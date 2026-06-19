# CricketSim — Optimization & Profiling Framework

> Keep the physics-first simulation smooth (60 FPS floor, 120 FPS preferred on Apple
> Silicon), measurable, and scalable — without ever changing what the simulation
> produces. Everything here observes; nothing feeds back into an outcome.

This is the milestone that makes the feature-complete sim *production-ready* on
performance. It adds a profiling spine, a budgeting system, a memory tracker, a
replay optimization layer, an on-screen dashboard, and an automated benchmark suite.

---

## 1. Where it sits in the module graph

Two new modules bracket the existing stack — one at the very bottom (so anything can
emit timing) and one at the very top (so it can observe and benchmark everything):

```
CricketPerformance   (TOP: manager subsystem, dashboard, replay optimizer, benchmarks, settings)
        │  depends on the whole sim stack — nothing depends on it, so no cycle
CricketUI / CricketAudio
CricketAI → CricketSim → CricketGameplay → CricketPhysics
        │                                        │
        └──────────────┬─────────────────────────┘
                       ▼  every sim module emits scopes into ↓
CricketPerfCore       (BOTTOM: profiler, scope timer + macro, rolling stat, budget, memory ledger)
        │
   Engine / Core / CoreUObject
```

Why split in two? The profiler must be includable by `CricketPhysics`,
`CricketGameplay`, and `CricketAI` to instrument their hot paths. If the manager
(which *depends* on those modules) also owned the profiler, that would be a
dependency cycle. `CricketPerfCore` depends on nothing in the project, so every
higher module can emit `CRICKET_PERF_SCOPE` freely.

| Module | Type / Phase | Responsibility | Depends on |
|---|---|---|---|
| **CricketPerfCore** | Runtime / PreDefault | Profiler singleton, RAII scope timer + macro, rolling-stat ring, simulation budget math, memory ledger, category enum. Pure, headless-testable. | Core, CoreUObject, Engine |
| **CricketPerformance** | Runtime / Default | The Performance Manager world subsystem + dashboard, the Replay Optimization Layer, the benchmark/stress harness, project settings. | + CricketPerfCore, CricketPhysics, CricketAI, CricketGameplay, CricketSim, DeveloperSettings, RHI, RenderCore |

---

## 2. The five architecture pieces

### 2.1 Performance Manager — `UCricketPerformanceSubsystem`
A `UTickableWorldSubsystem` (auto-creates in Game/PIE, no level wiring — same pattern
as the Audio Manager). Each frame it:
- samples the whole-frame measures (frame wall time, game/render-thread, GPU) from the
  engine globals (`GGameThreadTime`, `GRenderThreadTime`, `GGPUFrameTime`);
- drains the gameplay scope times the instrumented systems accumulated into
  `FCricketProfiler` (physics / prediction / AI / animation / replay);
- pushes each metric through a rolling window (min / avg / max / **p95** — spikes are
  what break frames, not averages);
- evaluates every category against the budget and logs overruns (rate-limited);
- scans live replay-clip memory and samples process physical usage;
- draws the dashboard.

`BuildReport()` returns a Blueprint-friendly `FCricketPerformanceReport` for the HUD
or tests.

### 2.2 Profiling Framework — `FCricketProfiler` + `CRICKET_PERF_SCOPE`
A dependency-free, game-thread accumulator. Wrap any hot section:

```cpp
void UCricketBallPhysicsComponent::TickComponent(...)
{
    CRICKET_PERF_SCOPE(Physics);   // RAII: times this scope, adds to the Physics bucket
    ...
}
```

The macro compiles to **nothing in Shipping** (`CRICKET_PERF_ENABLED == 0`). Currently
instrumented hot paths:

| Scope | Category | File |
|---|---|---|
| Ball integrator + contact | `Physics` | `CricketBallPhysicsComponent::TickComponent` |
| Fielding intercept/catch forecast | `Prediction` | `CricketFielderComponent::TickComponent` |
| Batter brain decision | `AI` | `CricketBatterAIController::TickComponent` |
| Fielding AI coordination | `AI` | `CricketFieldingAICoordinator::TickComponent` |
| Animation state machines | `Animation` | `CricketCharacterAnimComponent::TickComponent` |
| Replay frame capture | `Replay` | `CricketReplayComponent::CaptureFrame` |

Add more by including `CricketPerfProfiler.h` and dropping in a scope.

### 2.3 Simulation Budgeting System — `FCricketSimulationBudget`
A per-category millisecond budget derived from the frame-time target. Budgets are
authored as **fractions of the frame target** so the same split scales cleanly from
the 60 FPS floor (16.67 ms) to the 120 FPS goal (8.33 ms). Physics-first split:

| Category | Fraction of frame | @60 FPS | @120 FPS |
|---|---|---|---|
| Physics | 22% | 3.67 ms | 1.83 ms |
| Prediction | 10% | 1.67 ms | 0.83 ms |
| AI | 15% | 2.50 ms | 1.25 ms |
| Animation | 12% | 2.00 ms | 1.00 ms |
| Replay | 8% | 1.33 ms | 0.67 ms |
| Other | 10% | 1.67 ms | 0.83 ms |

`Evaluate(category, costMs)` returns Under / **Warning** (≥85%) / **Over** (>100%).

### 2.4 Memory Tracking System — `FCricketMemoryLedger`
A per-category byte ledger the sim's large, knowable buffers report into (notably live
replay clips). It does *not* hook the global allocator; it tracks what we can attribute
and reason about, and samples process physical usage (`FPlatformMemory::GetStats`) for
context. The manager rescans replay-clip memory twice a second.

### 2.5 Replay Optimization Layer — `FCricketReplayOptimizer`
Shrinks a finished `FCricketReplayClip` losslessly-where-possible / lossily-within-
tolerance:
1. **Adaptive sampling** — drop frames where nothing moved past a motion threshold,
   while always preserving the frames around recorded events (bounce / bat impact).
   Playback already interpolates, so dropped frames reconstruct within tolerance.
2. **Quantization** — snap positions to a millimetre grid (bounds error; makes a real
   serialized stream far more compressible).

Pure and deterministic, so its savings and worst-case error are unit-tested. Measured
on a 6000-frame clip: **→16.9% of original size, 3.9 mm max position error** (see the
Optimization Report).

---

## 3. The Debug Dashboard

Toggle with `cricket.Perf.Dashboard 1`. Shows, colour-coded by budget status
(green/amber/red):
- FPS + frame ms (avg & p95) vs the 60/120 targets;
- game-thread / render-thread / GPU times;
- physics / prediction / AI / animation / replay times vs budget (% used);
- replay tracked memory + process physical memory.

It is drawn via on-screen debug messages, guarded so it is inert headless.

---

## 4. Console variables

| CVar | Default | Meaning |
|---|---|---|
| `cricket.Perf.Enable` | 1 | Master switch for the profiler (scope timers + manager sampling). |
| `cricket.Perf.Dashboard` | -1 | On-screen dashboard. −1 = project settings, 0 = off, 1 = on. |
| `cricket.Perf.LogOverruns` | -1 | Log budget overruns. −1 = project settings. |

Project defaults live under **Project Settings ▸ Game ▸ Cricket Performance**
(`UCricketPerformanceSettings`): FPS targets, budget warn fraction, dashboard default,
rolling-window size, and the replay-optimizer knobs.

---

## 5. Automated benchmarks & stress tests

`FCricketPerformanceBenchmark` drives the **real** code paths (deterministic, headless):
- `RunAIvsAIMatch` — a full T20 via the AI match simulator;
- `RunLongMatchStress` — N back-to-back matches, aggregate throughput;
- `RunReplayStress` — a large dense clip through the optimizer, measuring savings + error.

These run in CI as part of the `CricketSim.Perf` suite (6 tests):
`RollingStat`, `Budget`, `Memory`, `Profiler`, `ReplayOptimizer`, `Benchmarks`.

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Perf; Quit" -unattended -nullrhi -nosplash
```

---

## 6. Design rules (kept consistent with the rest of the project)

- **Observation only.** No path here writes back into a gameplay outcome.
- **Zero shipping cost.** All instrumentation compiles out of Shipping builds.
- **Pure cores, headless tests.** Stat/budget/memory/optimizer math is UWorld-free and
  unit-tested, exactly like `CricketPhysics` and the Match Engine.
- **CVar + DeveloperSettings** layering and the auto-creating world-subsystem manager
  mirror the existing Audio/AI systems.
