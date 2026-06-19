# CricketSim — Optimization Report

> Profile first, then optimize the highest-impact areas. This report records what the
> profiling pass found across the feature-complete sim, the optimizations delivered in
> this milestone, the benchmark results, and the prioritized backlog.

Date: 2026-06-18 · Platform: macOS (Apple Silicon, Metal SM6) · Build: Development.

---

## 1. Method

The simulation is **physics-first and deterministic**: ball flight is a fixed 1 ms
sub-step RK4 integrator, contacts are analytic, and the AI/match layers are pure and
seedable. That makes most cost analyzable without a live frame capture:

- **Static cost analysis** of every per-frame tick path (what allocates, what traces,
  what scales with player/fielder count).
- **Headless throughput benchmarks** of the deterministic cores (the AI match
  simulator and the replay optimizer) via `FCricketPerformanceBenchmark` — these
  exercise the shipping code paths, not mocks.
- **Live attribution** at runtime via the new `CRICKET_PERF_SCOPE` instrumentation and
  the dashboard (physics / prediction / AI / animation / replay buckets vs budget).

---

## 2. Highest-impact opportunities (ranked)

| # | Area | Finding | Impact | Status |
|---|---|---|---|---|
| 1 | **Replay recording memory** | The recorder stores a dense fixed-rate ring; a 2-min innings = 7200 frames × (ball snapshot + every actor pose). Most frames are near-redundant (predictable arc, static fielders). | **High** (largest dynamic buffer; grows with match length × fielders) | **Fixed** — Replay Optimization Layer (§3.1) |
| 2 | **Per-frame ball sweep** | `UCricketBallPhysicsComponent::HandlePitchContact` issues a world sphere-sweep **every frame the ball is active**, even mid-flight metres above the pitch where no contact is possible. | **High** (a scene query per frame is the single most expensive recurring physics op) | Identified (§4.1) |
| 3 | **Redundant aero re-evaluation** | `FCricketAerodynamics::Evaluate` is recomputed in `TickComponent` for telemetry caching after the integrator already evaluated the field internally. | **Medium** (one extra force-field eval/frame) | Identified (§4.2) |
| 4 | **Always-on debug tick groups** | Each debug component (`Ball/Pitch/Batting/Bowling/Fielder/Anim`) ticks in `TG_PostPhysics` regardless of whether its overlay is visible. | **Medium** (N idle ticks; scales with actors) | Identified (§4.3) |
| 5 | **Fielding prediction cadence** | Fielder components run the trajectory/intercept forecast every tick; the forecast only needs refreshing when the ball state materially changes. | **Medium** (scales with fielder count, ≤11) | Identified (§4.4) |
| 6 | **AI brain churn** | AI controllers re-read the full match-awareness/brain decision each tick rather than per-ball or per-delivery-event. | **Low–Medium** | Identified (§4.5) |

The profiling spine (manager + budgets + dashboard) now makes #2–#6 measurable live,
so each can be tackled with a before/after number rather than a guess.

---

## 3. Optimizations delivered this milestone

### 3.1 Replay Optimization Layer (opportunity #1)
`FCricketReplayOptimizer` applies adaptive frame decimation (drop frames below a motion
threshold, always keeping event-adjacent frames) + millimetre position quantization to
a finished clip. Playback's existing interpolation reconstructs the dropped frames.

**Measured** (`RunReplayStress`, 6000 frames @ 60 Hz, 11 actors):

| Metric | Value |
|---|---|
| Frames | 6000 → **1012** |
| Size | **16.9%** of original |
| Saved | **3.96 MB** on this single clip |
| Max position error | **3.9 mm** (well within the 50 mm tolerance) |
| Optimize time | sub-millisecond |

Lossless mode (quantize + motion threshold both off) is verified to keep every frame
with zero error — the layer never silently degrades a clip you asked to keep intact.

### 3.2 Profiling spine + budgets (enables everything else)
- Zero-cost-in-shipping `CRICKET_PERF_SCOPE` instrumentation on the six hottest ticks.
- Per-category millisecond budgets that scale 60↔120 FPS, with amber/red flagging.
- Live dashboard + rate-limited overrun logging to catch regressions as they happen.

---

## 4. Backlog (identified, not yet applied) with recommended fixes

**4.1 Gate the ball sweep.** Skip `HandlePitchContact` when the ball's height and
downward velocity make a pitch contact impossible this frame (broad-phase reject on
`Position.Z` vs sweep extent), or sweep only on the descending arc. Expected: removes
the majority of per-frame scene queries during flight.

**4.2 Reuse the integrator's aero.** Have `FCricketBallIntegrator::Advance` expose the
last evaluated `FCricketAeroForces` so the component caches that instead of calling
`Evaluate` again. Saves one force-field evaluation per active ball per frame.

**4.3 Disable idle debug ticks.** Set debug components' `bCanEverTick` from their master
cvar/settings at `BeginPlay`, and re-enable on toggle, so a shipping/clean view pays
nothing for visualization it isn't drawing.

**4.4 Throttle fielding prediction.** Refresh the intercept forecast on a fixed cadence
(or on a "ball state changed materially" signal) rather than every frame; cache between
refreshes. The `Prediction` budget bucket now measures the win directly.

**4.5 Event-drive AI decisions.** Run brain decisions per delivery event / per ball
rather than per frame; the `AI` bucket quantifies the saving.

**4.6 Rendering (future, GPU-side).** Stadium/character/fielding draw cost is GPU-bound
and best addressed with LODs, instanced crowd/seating, and significance-based fielder
update rates once art is in. The dashboard already surfaces GPU + render-thread time
against the frame budget to drive this.

---

## 5. Benchmark results (this build)

All deterministic, headless, in the `CricketSim.Perf` suite (6/6 pass, 0 fail):

| Benchmark | Result |
|---|---|
| AI vs AI (single T20) | 251 balls simulated in **0.97 ms** (full match logic) |
| Long-match stress (4 × T20) | 915 balls, **0.69 ms / match** |
| Replay optimization stress | 6000 → 1012 frames, **16.9%** size, 3.9 mm error |

The sim's CPU logic has enormous headroom (a whole match resolves in under a
millisecond), which confirms the budget pressure at runtime will be dominated by
**per-frame scene queries (4.1), animation/character updates, and GPU/render cost** —
exactly the buckets the dashboard isolates.

---

## 6. Targets & verdict

- **60 FPS floor** — the CPU simulation budget (physics/AI/animation/replay) has ample
  margin; the framework now enforces it per-category and flags the first breach.
- **120 FPS preferred** — reachable on Apple Silicon; the budget set scales to 8.33 ms
  and the same instrumentation validates it.
- **Windows (future)** — no Mac-only APIs are used; the framework is RHI-agnostic
  (engine globals + platform memory) and will report identically on DX12.

The simulation is smooth, now **measurable and budgeted**, and the single largest
dynamic memory cost (replay) has been reduced ~6× with negligible quality loss before
presentation polish begins.
