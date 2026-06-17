# Ball Physics System — Technical Design

This is the milestone design for the **ball physics system**: the deterministic,
data-driven, Chaos-integrated foundation that every future gameplay system
(bowling, batting, fielding, rules) will build on. It covers the seven
deliverables: technical design, class hierarchy, data model, physics
architecture, implementation plan, production code, and testing strategy.

For the *physics equations* see [`BALL_PHYSICS.md`](BALL_PHYSICS.md); for the
*engine-level rationale* (custom integrator vs Chaos, modules, determinism) see
[`ARCHITECTURE.md`](ARCHITECTURE.md). This document is the system blueprint that
ties them together.

---

## 1. Scope

In scope: ball state, aerodynamics, integration, pitch interaction, pitch/ball
data assets, configuration, the update pipeline, prediction, and debug tooling.

Explicitly **out of scope** for this milestone (deferred): UI, menus, match
rules, batting/bowling/fielding mechanics & controls, crowds, commentary,
presentation. The ball simulation and its developer tools only.

---

## 2. Class hierarchy

```
CricketPhysics (Runtime, PreDefault) — pure SI, no UWorld
├── Data (USTRUCT)
│   ├── FCricketBallState          position, velocity, acceleration, angular
│   │                              velocity, spin axis, seam normal, seam
│   │                              stability, time
│   ├── FCricketBallSurface        ball condition: shine asymmetry, roughness,
│   │                              seam proudness
│   ├── FCricketEnvironment        temp, humidity, pressure, wind -> air density
│   ├── FCricketAeroCoefficients   tunable drag/Magnus/swing/wobble knobs
│   ├── FCricketSurfacePatch       local pitch: hardness, friction, moisture, unevenness
│   ├── FCricketImpact             per-bounce contact inputs
│   ├── FCricketTrajectorySample / Prediction / PredictionParams
│   └── FCricketBounceReport       bounce diagnostics
│
├── Solvers (stateless C++ classes)
│   ├── FCricketAerodynamics       Evaluate(state, surface, env, coeffs) -> forces
│   ├── FCricketBallIntegrator     RK4 fixed-substep flight integration
│   ├── FCricketPitchInteraction   impulse bounce: restitution, grip/skid, turn, seam
│   └── FCricketTrajectoryPredictor integrate-ahead through the same model
│
├── Assets (UPrimaryDataAsset)
│   ├── UCricketBallProfileAsset   a "ball personality" (coeffs + initial condition)
│   └── UCricketPitchProfileAsset  a pitch personality (zoned surface + wear)
│
└── Config
    └── UCricketPhysicsSettings    UDeveloperSettings: global defaults + debug toggles

CricketGameplay (Runtime) — binds the core to the world (Chaos)
├── UCricketBallPhysicsComponent   owns state + integrator; ticks flight; sweeps
│                                  the Chaos world for contacts; mirrors SI->cm
├── UCricketBallDebugComponent     read-only visualization overlay (+ CVars)
└── ACricketBall                   thin actor: mesh + physics + debug components
```

Key property: **solvers are stateless and engine-light**; all mutable state is
plain `USTRUCT` data owned by the component. This is what makes the system
unit-testable, replayable, and reusable by prediction/AI.

---

## 3. Data model

The complete ball state required by the brief, all in `FCricketBallState` (SI):

| Quantity | Field | Notes |
|---|---|---|
| Position | `Position` | m, world axes |
| Velocity | `Velocity` | m/s |
| Acceleration | `Acceleration` | m/s², committed by integrator (telemetry) |
| Angular velocity | `AngularVelocity` | rad/s; magnitude = spin rate |
| Spin axis | `AngularVelocity.GetSafeNormal()` | direction of the above |
| Seam orientation | `SeamNormal` | unit normal of the seam plane; precesses |
| Seam stability | `SeamStability` | [0,1]; 0 = wobble seam, 1 = held seam |
| Ball condition | `FCricketBallSurface` | shine asymmetry, roughness, seam proudness |
| Time | `TimeSinceRelease` | s; drives wobble phase |

**Why seam stability is state, not a coefficient:** a held seam can scramble in
flight; carrying it lets the model express that, and a "wobble-seam delivery" is
simply a low-stability release rather than a special code path.

**Data-driven layering** (most specific wins):
```
UCricketBallProfileAsset / UCricketPitchProfileAsset   (per delivery / per pitch)
        falls back to →
UCricketPhysicsSettings defaults                       (project-wide)
```

---

## 4. Physics architecture & update pipeline

Per ball, per frame:

```
TG_PrePhysics:  UCricketBallPhysicsComponent::TickComponent(dt)
   1. Integrator.Advance(State, dt)
        └─ slice dt into fixed 1 ms sub-steps (+ carried remainder)
            └─ per sub-step: RK4 over FCricketAerodynamics::Evaluate + gravity
                              → updates pos, vel, accel, spin, seam
   2. Sweep the Chaos world (SweepSingleByChannel) from prev→new position
        └─ on hit: sample FCricketSurfacePatch, build FCricketImpact,
                    FCricketPitchInteraction::ResolveBounce → mutate State
   3. Cache LastAero = FCricketAerodynamics::Evaluate(...)   (for debug/telemetry)
   4. Mirror State.Position (m) → actor world transform (cm)

TG_PostPhysics: UCricketBallDebugComponent::TickComponent(dt)
   └─ read-only: draw overlays, run FCricketTrajectoryPredictor for the prediction
```

**Chaos usage (per the requirement):** flight is our deterministic integrator;
Chaos owns the collision *world* and answers the contact queries (sweeps against
pitch/stumps/bat geometry and their physical materials). Contacts are then
resolved analytically so swing/seam/spin coupling stays under our control. This
is the deliberate split documented in `ARCHITECTURE.md` §2.

**Determinism:** fixed sub-step + double precision ⇒ identical delta-stream
reproduces bit-for-bit; different frame rates agree to within one sub-step. All
variation (bounce) is a deterministic hash of position, never hidden RNG.

---

## 5. Debug & developer tools

`UCricketBallDebugComponent` (+ `cricket.Debug.*` CVars, defaults from
`UCricketPhysicsSettings`) visualizes every quantity the brief lists:

| Requirement | Visualization |
|---|---|
| Velocity | cyan arrow, length ∝ speed; km/h in readout |
| Spin rate / RPM | magenta spin-axis line + "N rpm" label; readout |
| Swing forces | blue force arrow; `Cs` + magnitude in readout |
| Magnus forces | green force arrow; `Cl` + magnitude in readout |
| (Drag) | red force arrow; `Cd` + magnitude in readout |
| Seam orientation | yellow seam-plane disc + orange seam-normal stub; seam angle + stability in readout |
| Predicted trajectory | emerald path from `FCricketTrajectoryPredictor` + hollow predicted-bounce markers |
| Actual trajectory | white trail (ring buffer of past positions) |
| Bounce points | red spheres at actual bounces (from `OnBounce`) |
| (Diagnostics) | Reynolds number, reverse-swing regime in readout |

The overlay is strictly read-only and never perturbs the simulation, so it is
safe to leave attached; the master switch gates it off for shipping.

---

## 6. Implementation plan (this milestone)

1. ✅ State & condition model (add acceleration, seam stability; formalize condition).
2. ✅ Aerodynamics with separated drag/Magnus/swing outputs for telemetry.
3. ✅ RK4 fixed-substep integrator (records committed acceleration).
4. ✅ Pitch interaction (restitution, grip/skid turn, seam movement, variation).
5. ✅ Trajectory predictor (shared-model integrate-ahead).
6. ✅ Data assets: ball profile, pitch profile (zoned + wear).
7. ✅ Configuration: `UCricketPhysicsSettings` developer settings.
8. ✅ Gameplay bridge: physics component (Chaos sweeps) + ball actor.
9. ✅ Debug visualization component + CVars.
10. ✅ Automation tests + this design doc.

Next (future milestones, not this one): author ball/pitch profile *assets* and a
trajectory-logging harness to calibrate coefficients against reference footage.

---

## 7. Testing strategy

**Headless automation tests** (`CricketSim.Physics.*`, run with `-nullrhi`) —
the core is engine-light so these need no world:

| Test | Guards |
|---|---|
| `Determinism` | bit-identical for same delta-stream; frame-rate consistent within one sub-step |
| `MagnusBackspin` | backspin → upward carry |
| `MagnusDrift` | side-spin → lateral drift |
| `MagnusDip` | top-spin → downward dip |
| `ConventionalSwing` | swing toward seam side; mirrors correctly |
| `ReverseSwing` | fast + rough flips the side force |
| `LateMovement` | swing displacement accelerates (late) through flight |
| `PitchTurn` | spin about the flight line grips & deflects; symmetric on reversal |
| `PredictorConsistency` | predicted path == actual integrated path |

**Test layers, present + planned:**
- *Unit / property* (above): force signs, conservation/decay, determinism.
- *Calibration* (next milestone): trajectory harness vs reference magnitudes
  (swing ≈ 0.5–0.8 m over 20 m; spinner drift/dip; bounce heights) with
  tolerance gates.
- *Regression*: snapshot a delivery's state stream; assert unchanged across
  refactors (determinism makes this exact).
- *Cross-platform parity* (Windows milestone): same inputs → same results on
  macOS and Windows.

CI runs the automation suite on macOS via:
```
UnrealEditor-Cmd CricketSim.uproject \
  -ExecCmds="Automation RunTests CricketSim.Physics; Quit" -unattended -nullrhi
```
A non-zero exit code fails the build.
