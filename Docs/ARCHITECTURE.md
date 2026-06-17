# CricketSim — Architecture

> Physics-first cricket simulation. The cricket ball is the protagonist; every
> shot outcome emerges from aerodynamics and contact, never from scripted
> trajectories.

This document records the engine-level architecture decisions and the rationale
behind them. It is the first thing to read before touching the code.

---

## 1. Engine & platform

| Decision | Choice | Why |
|---|---|---|
| Engine | **Unreal Engine 5.7.4** (installed binary build) | Mandated. Chaos physics, cross-platform, mature C++ tooling. |
| Primary language | **C++20** | Mandated C++-first. Set in `*.Target.cs` via `CppStandard`. |
| Dev platform | **macOS (Apple Silicon, Metal SM6)** | Mandated. All code builds & runs on Mac throughout dev. |
| Ship target | **Windows (DX12)** later | No Mac-only APIs in gameplay/physics; RHI configured per-platform in `DefaultEngine.ini`. |
| Blueprints | **Thin layer only** | Data assets, level scripting, designer tuning. No simulation logic in BP. |

### Portability rule
Gameplay and physics code uses only engine-portable APIs. Anything platform-specific
(RHI, input backends, file paths) is confined to config and never leaks into the
`CricketPhysics`/`CricketGameplay` modules. macOS and Windows must produce
**identical physics** for the same inputs (see §4, Determinism).

---

## 2. The central decision: custom aerodynamic integrator, *not* Chaos rigid-body for ball flight

This is the most important architectural choice in the project.

**Chaos (UE's physics engine) does NOT simulate the ball in flight.** Instead:

- **Ball flight** is integrated by our own **`CricketPhysics`** module — a fixed
  sub-step RK4 integrator over an explicit aerodynamic force field (drag, Magnus,
  swing, seam). See `Docs/BALL_PHYSICS.md`.
- **Contacts** (ball↔pitch bounce, and later bat↔ball) are detected using the
  world's collision system (sweeps/queries against Chaos geometry) but **resolved
  by our own analytic models** (`CricketPitchInteraction`), so we control
  restitution, friction grip/skid, spin coupling and seam deviation exactly. The
  pitch is itself a first-class simulation system (data-driven surface material,
  bounce/spin/seam solvers, debug tooling) — see `Docs/PITCH_SYSTEM.md`.

### Why not let Chaos do it?
1. **Aerodynamics.** A general rigid-body solver has no concept of Magnus force,
   seam-dependent swing, or a Reynolds-driven drag crisis. These ARE cricket. We
   need a bespoke force model regardless.
2. **Determinism & tunability.** Swing/seam/spin must be reproducible and
   designer-tunable to match real-world reference. A custom integrator with a
   fixed timestep gives bit-stable, frame-rate-independent flight (§4).
3. **Authoring control.** Coefficients map directly to wind-tunnel literature, so
   tuning is grounded in measurable physics rather than solver hacks.

### What Chaos still gives us
- Broad-phase/narrow-phase collision geometry for the pitch, stumps, bat, fielders.
- Sweep & overlap queries to find contact points and normals.
- Ragdoll/secondary physics for cosmetic elements (much later).

```
        ┌─────────────────────────────────────────────────────┐
        │                  CricketPhysics (SI)                 │
        │   FCricketBallState ── RK4 ──> aerodynamic field     │
        │        ▲                              │              │
        │        │ contact resolution           ▼              │
        │   CricketPitchInteraction  <── contact point/normal  │
        └────────┼─────────────────────────────┼──────────────┘
                 │ (metres)            (cm) ▲   │ world queries
        ┌────────┼─────────────────────────┼───┼──────────────┐
        │     CricketGameplay: UCricketBallPhysicsComponent    │
        │     (owns state, ticks integrator, sweeps world,     │
        │      mirrors SI->cm onto the actor transform)        │
        └──────────────────────────────────────────────────────┘
                 Chaos world (geometry, sweeps) ── UE cm space
```

---

## 3. Module layout

Strict one-directional dependency graph (lower depends on nothing above it):

```
CricketSimEditor   (editor-only: tuning, validation, asset tooling)
        │
CricketSim         (primary game module: T20 rules, GameMode/State, team data)
        │
CricketGameplay    (actors/components: ball actor, bowling/batting/fielding, input)
        │
CricketPhysics     (pure SI physics: aerodynamics, integrator, pitch interaction)
        │
   Engine / Core / CoreUObject / PhysicsCore / Chaos
```

| Module | Type | Responsibility | May depend on |
|---|---|---|---|
| **CricketPhysics** | Runtime (PreDefault) | Deterministic SI ball model. No actors, no input, no rendering. Unit-testable in isolation. | Core, CoreUObject, Engine, PhysicsCore, Chaos |
| **CricketGameplay** | Runtime | Binds physics to the world: `ACricketBall`, `UCricketBallPhysicsComponent`, bowling/batting pawns, basic fielding, bat-ball collision. | + CricketPhysics, EnhancedInput |
| **CricketSim** | Runtime (primary) | Match flow & T20 rules, `ACricketGameMode`, GameState, team/player data. | + CricketGameplay |
| **CricketSimEditor** | Editor | Data validation, physics tuning utilities, custom detail panels. Never shipped. | + everything, UnrealEd/Slate |

**Why split physics out of gameplay?** It keeps the model free of `UWorld`,
ticking, and actor lifecycle so it can be (a) unit-tested headlessly, (b) reused
by replay/analysis tools, and (c) reasoned about as pure functions of state.

---

## 4. Determinism strategy

Realism reviews require reproducible deliveries ("bowl that exact ball again").

1. **Fixed integration sub-step** (`FCricketBallIntegrator::DefaultSubstep = 1 ms`).
   Wall-clock delta is sliced into fixed steps with a carried remainder. The
   precise guarantees (both covered by automation tests):
   - **Bit-identical** results for an identical delta-stream — the basis for
     replay/repro: record the release + delta sequence and you reproduce the
     exact flight.
   - **Frame-rate independence to within one sub-step** across *different*
     slicings (e.g. 60 vs 100 fps). It is not bit-identical across slicings
     because the leftover partial step differs by up to one 1 ms sub-step;
     committed sim state only ever advances in whole sub-steps.
2. **Double precision** throughout the core (`FVector` is `double` in UE5).
3. **No hidden RNG in the model.** Bounce "variation" is a *deterministic hash* of
   the landing position (`UCricketBallPhysicsComponent::DeterministicVariance`),
   combined with an authored pitch unevenness field — same spot, same behaviour.
4. **State is plain data** (`FCricketBallState`): position, velocity, spin, seam
   normal, time. Snapshot it to reproduce or rewind any delivery.

Stochastic *human* variation (a bowler's release scatter) is injected at the
gameplay layer as explicit, seedable noise on the release inputs — never inside
the physics core.

---

## 5. Units & coordinate conventions

- **Core is SI**: metres, kg, seconds, radians. Every coefficient is comparable
  to published aerodynamics literature.
- **UE world is centimetres.** Conversion happens at exactly one boundary —
  `UCricketBallPhysicsComponent` — via `CricketPhysics::MetersToWorld` /
  `WorldToMeters`. The model never sees centimetres.
- **Axes** are shared with UE world space (no rotation needed at the boundary):
  `+X` down the pitch, `+Y` off side (UE right), `+Z` up. Cross products use UE's
  left-handed convention; the Magnus sign is derived and unit-tested against it.

---

## 6. Data-driven tuning

Physical *constants* (ball mass, radius) live in code (`CricketPhysicsConstants.h`).
*Tunable coefficients* and surface/environment conditions are data:

- `FCricketAeroCoefficients` — drag/Magnus/swing/wobble knobs.
- `UCricketBallProfileAsset` — a "ball personality" preset (new Kookaburra, old
  reversing SG, dewy white ball), discoverable via the Asset Manager.
- `UCricketTeamDataAsset` — India & Australia rosters; ratings bias *inputs*
  (release pace, spin rpm), never outcomes.

This lets designers tune realism without recompiling and keeps the format (T20),
teams, and ball behaviour as content.

---

## 7. Build & run (macOS)

```sh
# Build the editor target
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    CricketSimEditor Mac Development -project="$PWD/CricketSim.uproject"

# Generate Xcode project files (optional, for IDE)
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" \
    -project="$PWD/CricketSim.uproject" -game

# Open in the editor
open "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app" \
    --args "$PWD/CricketSim.uproject"
```

See `Docs/ROADMAP.md` for the phased delivery plan and `Docs/BALL_PHYSICS.md` for
the physics model.
