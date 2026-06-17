# CricketSim — Pitch Simulation System

> The pitch is the second protagonist. With the ball, it is the primary
> differentiator of the game: the same delivery must behave like a different
> ball on a hard, a dry, and a green deck — and that difference must *emerge*
> from physical surface properties, never from a scripted outcome.

This document is the technical design + reference for the pitch system added in
Phase 2. Read `Docs/ARCHITECTURE.md` and `Docs/BALL_PHYSICS.md` first; this
builds directly on the SI ball-physics core and the contact-resolution boundary
described there.

---

## 1. Design goals & scope

The pitch is a **first-class simulation system** that influences:

- bounce height, bounce consistency, and ball pace after the bounce,
- seam deviation off the surface,
- spin turn amount,
- friction (grip vs skid) and energy loss.

It ships with three distinct, recognisable **pitch types** — **Hard**, **Dry**,
**Green** — plus a neutral *Balanced* baseline and a *Custom* passthrough.

**In scope (this milestone):** the surface data model, a surface-material
system, the pitch physics module and its three solvers, debug tooling, and
comparative automation tests.

**Designed-for but intentionally NOT implemented yet:** pitch deterioration,
footmark creation, day progression, multi-day cricket. The data structures and
integration hooks exist (see §7) so these can be layered on without reworking
the model.

**Out of scope (per the brief):** batting, fielding, match rules, AI, UI,
presentation.

---

## 2. Where it sits (integration with existing systems)

Nothing about the ball-flight integrator, the bowling generator, or the cm↔m
boundary changed. The pitch system slots into the **contact-resolution** seam
that already existed:

```
 CricketPhysics (SI)
   FCricketBallState ──RK4──► aerodynamic field            (unchanged)
        │
        │ contact point + normal (from a world sweep)
        ▼
   FCricketPitchInteraction::ResolveBounce   ◄── FCricketSurfacePatch
        │  (the Pitch Physics Module — orchestrator)        (sampled per impact)
        ├─► FCricketBounceSolver   (normal: restitution, pace, bounce angle/height)
        ├─► FCricketSpinSolver     (tangential: friction grip/skid, TURN)
        └─► FCricketSeamSolver     (seam-strike lateral deviation, wobble seam)
        │
        ▼  mutated FCricketBallState (+ FCricketBounceReport)

 CricketGameplay (cm)
   UCricketBallPhysicsComponent   sweeps the world, samples the surface,
                                  calls ResolveBounce, broadcasts OnBounce.
   UCricketBowlingComponent       pushes the active surface onto the ball.
   UCricketPitchDebugComponent    visualises every bounce (read-only).
```

Key integration facts:

- **`UCricketBallPhysicsComponent::HandlePitchContact`** is the single call site
  for the live ball. It already builds the `FCricketImpact` (contact normal,
  seam-flushness, deterministic variance) and calls `ResolveBounce`. No change
  was needed there — the richer surface struct flows through transparently.
- **`FCricketTrajectoryPredictor`** uses the *same* `ResolveBounce`, so the
  predicted bounce equals the actual bounce (the basis of predicted-vs-actual
  debug and any future shot/AI reasoning).
- **`UCricketBowlingComponent`** owns the live `PitchSurface` and writes it onto
  the ball at release; the off/leg-break turn tests in `CricketSim.Bowling`
  exercise this end-to-end.

---

## 3. Data models

### 3.1 `FCricketSurfacePatch` — the local surface at one point of impact
*(`CricketPitchInteraction.h`)*

The eight authored fields. The first seven are the brief's mandated material
parameters; `Unevenness` is the bounce-consistency driver.

| Field | Range | Meaning / effect |
|---|---|---|
| `Hardness` | 0–1 | Springy, true, fast bounce ↑ |
| `Roughness` | 0–1 | Micro-texture; spin bite (turn) ↑, ball scuffing ↑ |
| `Moisture` | 0–1 | Deadens restitution, greases surface (skid, less turn) |
| `GrassCoverage` | 0–1 | Binds surface; **grips a landing seam** (seam movement ↑); skids ball on (less turn) |
| `Friction` | ≥0 | Base Coulomb coefficient |
| `Restitution` | 0–0.95 | Intrinsic low-speed bounciness |
| `Wear` | 0–1 | Lowers hardness/restitution, raises grip/roughness/unevenness |
| `Unevenness` | 0–1 | Scales deterministic bounce variation (inverse of consistency) |

### 3.2 `FCricketImpact` — per-bounce inputs
Contact normal, `SeamContact` (how flush the seam lands, 0–1), and a
deterministic `Variance` in [-1,1] (hashed from the landing spot by the gameplay
layer — **the only source of bounce variation, and it is not random**).

### 3.3 `FCricketBounceReport` — diagnostics out
Restitution used, grip flag + grip level, friction used, incoming angle, bounce
angle, ballistic bounce height, speed-retained fraction, and the split lateral
deviation (`TurnMS` from spin, `SeamDeviationMS` from the seam, plus their sum).
Purely descriptive; consumed by debug + tests.

### 3.4 `FCricketBounceContext` — solver working set (non-UObject)
The orchestrator decomposes the incoming velocity **once** (normal/tangential
split, impact speed) and the three solvers read it and accumulate their
contributions (`NewVNormal`, `TangImpulse`, `SeamImpulse`). Keeps the solvers
independent without recomputing the decomposition three times.

### 3.5 `UCricketPitchProfileAsset` — a full pitch personality
*(`CricketPitchProfileAsset.h`)* A `UPrimaryDataAsset` with a `PitchType`, a
`BasePatch`, length-banded `Zones` (first containing band wins), a global `Wear`
dial, and the future-proofing fields (§7). `ConfigureFromType()` fills it from
the material library; `SamplePatch(distanceDownPitch)` returns the surface at a
length, applying zones then global wear.

---

## 4. Surface material system
*(`CricketPitchMaterial.h` / `.cpp` — `FCricketPitchMaterialLibrary`)*

The **single source of truth** mapping `ECricketPitchType` → physical surface
parameters, so code, data assets, prediction and tests all agree. Pure,
stateless, deterministic.

| Type | Hardness | Restitution | Friction | Moisture | Grass | Roughness | Headline behaviour |
|---|---|---|---|---|---|---|---|
| **Hard** (Perth) | 0.92 | 0.62 | 0.44 | 0.05 | 0.25 | 0.10 | High, fast, true bounce; carries |
| **Dry** (subcontinent) | 0.55 | 0.44 | 0.62 | 0.04 | 0.03 | 0.60 | Low bounce, big grip/turn, pace off |
| **Green** (Hobart) | 0.75 | 0.52 | 0.50 | 0.30 | 0.80 | 0.12 | Strong seam movement, decent carry |

`MakeZones()` adds a representative worn "good length" band (4–7 m from the
batter's stumps) — heavily so for Dry (the spinner's rough), lightly for the
others. Authoring a profile is one click: set `PitchType`, the editor calls
`ConfigureFromType()` (see `PostEditChangeProperty`).

---

## 5. Class hierarchy & the physics module

```
FCricketPitchInteraction              ← public façade = the "Pitch Physics Module"
  static ResolveBounce(State, Patch, Impact) -> FCricketBounceReport
    │  decompose incoming velocity → FCricketBounceContext
    ├── FCricketBounceSolver::Solve   normal response
    │      e = Restitution·(0.75+0.35·Hardness)·(1−0.45·Moisture)
    │            ·(1−0.20·Grass)·1/(1+0.012·v_impact)·(1+0.35·Unevenness·Variance)
    │      → NewVNormal = −e·VNormal ; Jn = (1+e)·|Vn| ; bounce angle/height
    ├── FCricketSpinSolver::Solve      tangential response (friction + TURN)
    │      μ = Friction·(1+0.6·Roughness)·(1+0.5·Wear)·(1−0.35·Moisture)·(1−0.25·Grass)
    │      contact-point velocity folds in ω×r ; grip if J_stick ≤ μ·Jn else skid
    │      gripping converts spin → sideways translation (off/leg-break turn)
    └── FCricketSeamSolver::Solve      seam-strike lateral deviation
           seamGrip = Friction·(1+0.8·Grass)·(1+0.3·Roughness)·(0.7+0.3·Hardness)
           dev = 0.12·v_in·SeamContact·seamGrip·(1+0.5·Variance)·wobble(stability,Variance)
    │
    └─ assemble: V' = NewVNormal + VTangent + TangImpulse + SeamImpulse
```

Each solver is a separate, independently-testable class
(`CricketBounceSolver.h`, `CricketSpinSolver.h`, `CricketSeamSolver.h`). The
orchestrator owns ordering (normal response first, because the friction cone
depends on the normal impulse) and final assembly.

### Physics notes
- **Grip/skid** uses the reduced-mass factor `k = 1 + mR²/I = 2.5`; the impulse
  to arrest slip is `J_stick = v_contact_tangential / k`. Within the friction
  cone the ball *bites* and spin becomes lateral velocity — this **is** the
  turn. Beyond it, the ball skids on and only `μ·Jn` is applied.
- **Drift amplification:** a rough/worn track raises `μ`, so for identical revs
  more of the spin is converted → more turn (the "rip"). Emergent, not a knob.
- **Wobble seam:** a scrambled seam (`FCricketBallState::SeamStability` < 1)
  makes both the *direction* and *magnitude* of the seam kick vary with the
  deterministic `Variance` — inconsistent movement either way, vs a held seam's
  repeatable one-way deviation.
- **Determinism:** no RNG anywhere. Variation is `Unevenness · Variance`, and
  `Variance` is a hash of the landing position supplied by the caller.

---

## 6. Debug tooling
*(`UCricketPitchDebugComponent` — auto-attached to `ACricketBall`)*

Read-only; reads state, never affects the sim. Per bounce it draws:

- **bounce point** (red sphere),
- **bounce angle** — incoming (red, into the spot) + outgoing (green) arrows,
- **surface/friction readout** — floating text: `e`, `μ`, grip/skid + grip
  level, bounce angle, bounce height, pace retained, turn, seam deviation,
- **turn (cyan) and seam-deviation (orange)** lateral kicks,
- **predicted vs actual** — a hollow yellow marker where the model predicted the
  first bounce, and a purple line to where the ball actually landed (the error).

Every overlay has a console variable and a Project-Settings default:

| CVar (`-1` = inherit settings) | Setting (`UCricketPhysicsSettings`) |
|---|---|
| `cricket.Debug.Pitch.Enable` | `bEnablePitchDebug` |
| `cricket.Debug.Pitch.BounceAngle` | `bDrawPitchBounceAngle` |
| `cricket.Debug.Pitch.SurfaceInfo` | `bDrawPitchSurfaceInfo` |
| `cricket.Debug.Pitch.TurnSeam` | `bDrawPitchTurnSeam` |
| `cricket.Debug.Pitch.PredictedVsActual` | `bDrawPitchPredictedVsActual` |

---

## 7. Future-proofing (design only)
*(`CricketPitchTypes.h`, `CricketPitchProfileAsset.h`)*

Data structures and hooks exist; the *behaviour* is deliberately deferred.

- **`FCricketFootmark`** — a localised rough patch (centre, radius, severity).
  Carried on the profile; **not yet consulted** by `SamplePatch` (the marked
  hook is in the function). Will later raise local roughness/wear/unevenness
  around the bowlers' follow-through and the rough outside off stump.
- **`FCricketPitchDayProgression`** — day (1–5), session fraction, overs.
  `ComputeWear()` returns a single 0–1 dial; `ApplyDayProgression()` folds it
  into the profile's global `Wear` (a monotonic deterioration). Per-property
  ageing curves and the 2D (lateral) sampler come later for multi-day cricket.

None of this changes the solver interfaces — they already consume the full
surface patch, so deterioration is "just" evolving the sampled patch over time.

---

## 8. Automated testing
*(`CricketPitchTests.cpp`, suite `CricketSim.Pitch`)*

Run headless:
```sh
UnrealEditor-Cmd CricketSim.uproject \
  -ExecCmds="Automation RunTests CricketSim.Pitch; Quit" -unattended -nullrhi
```

The required comparative tests + solver guards (all green):

| Test | Asserts |
|---|---|
| `SameDeliveryDifferentPitches` | Hard bounces higher & keeps more pace than Dry; all three distinct |
| `SameSpinDifferentPitches` | Dry turns more than Hard/Green for identical revs |
| `SameSeamDifferentPitches` | Green seams more than Hard/Dry for an identical flush seam |
| `RestitutionMonotonic` | restitution ↑ with hardness, ↓ with moisture |
| `GripSkidThreshold` | high-friction abrasive grips; low-friction smooth skids |
| `WobbleSeam` | scrambled seam deviates differently from a held seam |
| `Determinism` | identical inputs reproduce velocity/spin/restitution bit-for-bit |
| `ProfileSampling` | worn good-length band rougher than base; global wear deadens |

Regression: `CricketSim.Physics.*` and `CricketSim.Bowling.*` (incl. the
off/leg-break turn tests, which call the refactored solver) remain green.

---

## 9. Files

```
CricketPhysics/Public
  CricketPitchInteraction.h     surface patch, impact, report, context, module façade
  CricketPitchTypes.h           ECricketPitchType, zone, footmark + day-progression (future)
  CricketPitchMaterial.h        FCricketPitchMaterialLibrary (surface material system)
  CricketBounceSolver.h         normal response
  CricketSpinSolver.h           friction grip/skid + turn
  CricketSeamSolver.h           seam movement + wobble
  CricketPitchProfileAsset.h    UCricketPitchProfileAsset (data asset)
CricketPhysics/Private
  CricketPitchInteraction.cpp   orchestrator
  CricketBounceSolver.cpp / CricketSpinSolver.cpp / CricketSeamSolver.cpp
  CricketPitchMaterial.cpp / CricketPitchProfileAsset.cpp
  CricketPitchTests.cpp         CricketSim.Pitch automation suite
CricketGameplay/Public+Private
  CricketPitchDebugComponent.*  bounce/angle/surface/turn/seam/predicted-vs-actual viz
CricketPhysics/Public
  CricketPhysicsSettings.h      + pitch debug toggles
```
