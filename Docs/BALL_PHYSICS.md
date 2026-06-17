# CricketSim — Ball Physics Model

> The cricket ball is the most important object in the game. Swing, reverse
> swing, seam movement, wobble seam, spin, drift, dip, bounce variation, pitch
> interaction — all of it emerges from the model below. Nothing is scripted.

All symbols are SI (metres, kg, seconds, radians). Code lives in the
`CricketPhysics` module:

- `CricketPhysicsConstants.h` — constants & unit conventions
- `CricketPhysicsTypes.h` — state, surface, environment, coefficients
- `CricketAerodynamics.{h,cpp}` — the force model
- `CricketBallIntegrator.{h,cpp}` — RK4 sub-stepped integration
- `CricketPitchInteraction.{h,cpp}` — bounce / seam-off-pitch / turn

---

## 1. State

`FCricketBallState` is the complete dynamic state:

| Symbol | Field | Meaning |
|---|---|---|
| **x** | `Position` | ball-centre position (m) |
| **v** | `Velocity` | linear velocity (m/s) |
| **ω** | `AngularVelocity` | spin axis · rate (rad/s) |
| **n̂** | `SeamNormal` | unit normal of the seam great-circle plane |
| t | `TimeSinceRelease` | drives wobble phase (s) |

The **seam normal** is carried in state so it can precess during flight (the
wobble-seam effect, §6).

---

## 2. The force field

Net force on the ball:

```
F = F_gravity + F_drag + F_Magnus + F_swing
```

Gravity is added in the integrator (`-g ẑ`, g = 9.81). The other three are the
aerodynamic model, evaluated by `FCricketAerodynamics::Evaluate` against the air-
relative velocity **v_rel = v − v_wind**, with `S = ½ ρ A` and `q·A = ½ ρ A |v_rel|²`.

Air density ρ is computed from temperature, humidity and pressure
(`FCricketEnvironment::ComputeAirDensity`, partial-pressure method). A = πr².

### 2.1 Drag — with the drag crisis

```
F_drag = − C_d · (½ ρ A |v_rel|²) · v̂_rel
```

`C_d` is **not constant**. A cricket ball goes through the *drag crisis*: as the
boundary layer transitions from laminar to turbulent (around the critical
Reynolds number), separation moves rearward and `C_d` drops sharply. We model
this as a smooth interpolation driven by the same "reverse regime" factor used
for swing (§2.3):

```
C_d = lerp(C_d_subcritical, C_d_supercritical, r)      // ~0.45 → ~0.24
```

Reynolds number `Re = ρ |v_rel| d / μ`, `d = 2r`, `μ = ρ ν`.

### 2.2 Magnus — carry, dip, drift

Spin produces a force perpendicular to both spin axis and velocity:

```
F_Magnus = C_l · (½ ρ A |v_rel|²) · (ω̂ × v̂_rel)
C_l      = min(k_L · S, C_l,max)          // saturating lift curve
S        = |ω| r / |v_rel|                // spin ratio
```

Direction `ω × v` is verified against UE's left-handed cross product:

| Spin | ω direction (v along +X) | F_Magnus | Cricket effect |
|---|---|---|---|
| Backspin | −Y | +Z (up) | **carry** — ball holds its line, fuller |
| Topspin | +Y | −Z (down) | **dip** — drops early and steep (the overspin off-break) |
| Sidespin | +Z | +Y (sideways) | **drift** — the ball drifts in the air (classic spinner's drift) |

So **dip** and **drift** are the *same* Magnus term with different spin axes —
exactly as in reality. A leg-spinner imparts a tilted axis → simultaneous drift
*and* dip.

### 2.3 Swing — conventional and reverse from one equation

Swing is a side force from asymmetric boundary-layer separation set up by the
**seam angle** and the **surface asymmetry**.

**Seam angle** θ — signed angle between velocity and the seam plane:

```
sinθ = v̂_rel · n̂          θ = asin(v̂_rel · n̂)
```

**Lateral direction** L̂ — the seam normal projected perpendicular to velocity:

```
L̂ = normalise( n̂ − (v̂_rel · n̂) v̂_rel )
```

**Seam-presentation shape** — swing peaks near the optimal seam angle (~20°) and
"stalls" beyond it. We use a gamma-like curve that is 0 at θ=0 and exactly 1 at
θ = θ_opt:

```
shape(θ) = (θ/θ_opt) · exp(1 − θ/θ_opt)
```

**Regime factor** r ∈ [0,1] — `ComputeReverseRegime`. Rises with airspeed around
a transition speed; a rougher ball reverses *earlier* (lower effective speed):

```
v_crit_eff = v_transition − 8·roughness
r          = smoothstep(v_crit_eff − 3, v_crit_eff + 3, |v_rel|)
```

**Side-force coefficient** — conventional swing needs a polished side; reverse
needs a roughened surface and pace. They push in *opposite* lateral directions:

```
conv = (1 − r) · |shineAsymmetry|
rev  =  r      · (0.5 + 0.5·roughness)
C_s  = C_s,max · shape(θ) · seamProudness · (conv − rev)

F_swing = C_s · (½ ρ A |v_rel|²) · L̂
```

- **New ball, low/medium pace, polished side, seam angled** → r≈0, `conv`
  dominates → ball swings toward the seam side. **Conventional swing.**
- **Old/scuffed ball at high pace** → r≈1, `rev` dominates, sign flips → ball
  swings the *other* way (toward the shiny side). **Reverse swing.**
- **Symmetric surface** (`shineAsymmetry≈0`, `roughness≈0`) → no swing, however
  the seam is held. Correct: you need an asymmetry.

This single expression reproduces both modes and the unsettling fact that as a
quick bowls faster, the ball can *stop* conventional-swinging and start
reverse-swinging mid-spell.

---

## 3. Spin decay

Air friction slowly bleeds spin:

```
dω/dt = − k_spin · ω
```

---

## 4. Integration — fixed sub-step RK4

`FCricketBallIntegrator` advances the state with classic 4th-order Runge–Kutta at
a **fixed 1 ms sub-step**, slicing the frame's wall-clock delta and carrying the
remainder. RK4 (not Euler) because the forces are velocity-squared and spin-
coupled; over a ~0.5 s delivery, Euler visibly mis-predicts swing and dip.

The derivative per sub-step:

```
dx/dt = v
dv/dt = F_aero/m − g ẑ
dω/dt = −k_spin ω
dn̂/dt = ω_seam × n̂           // seam precession (wobble)
```

`n̂` is renormalised each step. Fixed-step + double precision ⇒ frame-rate-
independent, machine-independent flight (see ARCHITECTURE §4).

---

## 5. Pitch interaction — bounce, seam movement, turn

`FCricketPitchInteraction::ResolveBounce` resolves a single contact using an
impulse model. Inputs: the local `FCricketSurfacePatch` (hardness, friction,
moisture, unevenness) and an `FCricketImpact` (contact normal, seam-strike
flushness, deterministic variance).

**Normal — restitution & bounce variation**

```
e = (0.28 + 0.34·hardness) · (1 − 0.5·moisture) · 1/(1 + 0.012·|v_n|)
e *= 1 + 0.35·unevenness·variance          // cracks/footmarks, deterministic
v_n' = − e · v_n
```

Faster impacts lose more energy (e falls with speed); soft/wet pitches give low,
dead bounce. **Bounce variation** is the unevenness·variance term — and
`variance` is a *hash of the landing position*, so the same crack always misbehaves
the same way (reproducible, not random).

**Tangential — grip vs skid, and TURN**

The contact-point velocity couples spin into the bounce:

```
u = v_t + ω × (−r n̂)
```

The impulse to fully arrest sliding is `J_stick = |u|/k`, with the rolling factor
`k = 1 + mr²/I = 2.5`. Coulomb limit `J_max = friction · J_n`.

- `J_stick ≤ J_max` → **grip**: the ball bites and spin converts to translation —
  this is **turn** (off/leg break) for a side-spinning ball, extra bounce for
  topspin, skid-on for backspin.
- otherwise → **skid**: only Coulomb friction is applied; the ball slides on.

The tangential impulse also updates spin via `Δω = (r_contact × J)/I`.

**Seam movement off the pitch**

If the seam strikes flush (`seamContact` high), an extra lateral impulse along the
seam's in-plane projection deflects the ball — the seam bowler's nip/jag:

```
Δv_seam = 0.12 · |v_in| · seamContact · friction · (1 + 0.5·variance) · L̂_seam
```

So **seam movement** depends on landing the seam squarely (luck + skill), the
surface friction, and pace — exactly the real-world levers.

---

## 6. Wobble seam

A wobble-seam delivery scrambles the seam so its presentation oscillates and its
orientation at landing is unpredictable. Modelled as a precession of n̂ about the
flight line:

```
ω_seam = A · f · cos(2π f t) · v̂_rel        // amplitude A, rate f
```

Consequences fall out automatically: the swing side force (§2.3) oscillates and
tends to average out (so a wobble ball doesn't reliably swing), and the seam
normal at the moment of bounce varies delivery-to-delivery → variable seam
movement (§5). Set `WobbleSeamAmplitudeRad`/`WobbleSeamRateRadS` to 0 for a
held-seam delivery.

---

## 7. Coefficient reference (`FCricketAeroCoefficients`)

| Field | Default | Typical range | Effect |
|---|---|---|---|
| `BaseDragCoefficient` | 0.45 | 0.40–0.50 | subcritical drag |
| `SupercriticalDragCoefficient` | 0.24 | 0.20–0.30 | post drag-crisis drag |
| `MaxSwingSideForceCoefficient` | 0.30 | 0.20–0.40 | peak swing strength |
| `OptimalSeamAngleRad` | 0.349 (20°) | 0.26–0.44 | seam angle of max swing |
| `MagnusLiftSlope` | 0.5 | 0.4–0.6 | lift per unit spin ratio |
| `MaxMagnusLiftCoefficient` | 0.45 | 0.35–0.55 | lift saturation |
| `SpinDecayRate` | 0.1 /s | 0.05–0.2 | how fast spin bleeds |
| `SwingTransitionSpeed` | 30 m/s | 28–35 | conventional→reverse crossover |
| `WobbleSeamRateRadS` / `…AmplitudeRad` | 0 / 0 | tune per delivery | wobble precession |

These are the tuning surface. Calibrate them against reference deliveries
(documented in `ROADMAP.md`, Phase 1 exit criteria) before locking values into
`UCricketBallProfileAsset` presets.

---

## 8. Validation checklist (physical sanity)

Each must reproduce without code changes — only inputs/coefficients:

- [ ] Outswinger: seam angled to off, polished off side, ~33 m/s → curves away.
- [ ] Inswinger: mirror the seam → curves in.
- [ ] Reverse swing: roughen the ball, bowl >35 m/s → swing direction flips.
- [ ] Off-break: side-spin axis + grippy pitch → grips and turns in; drifts away in air.
- [ ] Leg-break: opposite axis → drifts in, turns away; overspin variant dips.
- [ ] Backspin quick: skids on, lower bounce; topspin: bounces higher.
- [ ] Wobble seam: inconsistent swing, variable seam movement at the pitch.
- [ ] Bounce variation: same crack ⇒ same misbehaviour; true length ⇒ true bounce.
