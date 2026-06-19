# CricketSim — Performance Report (Vertical Slice RC)

_Generated 2026-06-18. Companion to `PERFORMANCE_SYSTEM.md` and `OPTIMIZATION_REPORT.md`._

## What is verified

The performance & profiling framework (modules `CricketPerfCore` +
`CricketPerformance`) is implemented and its headless benchmark/regression suite
(`CricketSim.Perf`, 6 tests) **passes** as part of the full 122-test run. That
covers:

- **Rolling-stat math** (frame-time ring buffer, percentiles) — used by the live
  dashboard.
- **Simulation budgeting** (`FCricketSimulationBudget`) — the cooperative
  time-slice that caps per-frame AI/forecast work.
- **Memory ledger** (`FCricketMemoryLedger`) — category-tagged allocation tracking.
- **Profiler scope accumulation** (`CRICKET_PERF_SCOPE`) — compiled out in Shipping
  via `CRICKET_PERF_ENABLED`.
- **Replay optimizer** (`FCricketReplayOptimizer`) — keyframe/compression model.
- **AI-vs-AI / long-match / replay benchmark harness** (`FCricketPerformanceBenchmark`).

Hot ticks are instrumented: ball physics, fielder forecast, batter AI + fielding
coordinator, character anim, replay capture.

## Live runtime tooling (developer)

- `cricket.Perf.Enable 1` (default on; lightweight, compiles out in Shipping).
- `cricket.Perf.Dashboard 1` — on-screen FPS / frame-time / budget / memory HUD.
- `cricket.Perf.LogOverruns 1` — logs frames that blow the simulation budget.

These are available in Development/Test builds via `UCricketPerformanceSettings`.

## What is NOT yet measured (and why)

On-device **wall-clock FPS, frame-time percentiles, and memory high-water on
Apple Silicon** require running a *windowed* build with a populated level. That is
gated by Known Issue **B1** (no cookable map). Until a level exists, performance is
validated by:
- the headless benchmark harness (algorithmic cost + budget adherence), and
- the determinism/throughput of the headless AI-vs-AI match simulator
  (hundreds of full T20s run per `CricketSim.Balance` invocation in seconds).

## Recommended measurement plan (once B1 is resolved)

1. Package `Development` (`Scripts/package_mac.sh Development`) so the dashboard
   and `stat unit` / `stat fps` are available.
2. Capture `stat unit`, `stat game`, `stat gpu` and the `cricket.Perf.Dashboard`
   readout for: (a) live human-vs-AI over, (b) AI-vs-AI auto-play at max speed,
   (c) replay playback, (d) a full 40-over match end-to-end.
3. Targets for the slice on M-series: **60 FPS** sustained in normal play, no
   single frame > 33 ms during a delivery, flat memory across a full match (no
   per-ball growth — watch the memory ledger), stable replay capture (no hitching
   on record).
4. File any regressions against the budget categories the dashboard names.

## Optimization posture

The architecture is already structured for performance: a single fixed-substep
RK4 ball integrator (not per-actor Chaos), a cooperative simulation budget, and a
strict module graph that keeps hot physics free of UI/AI dependencies. No
performance hot-spots were introduced this pass; the EdgeCurvature change is a few
extra vector ops inside an already-rare bat-contact event.
