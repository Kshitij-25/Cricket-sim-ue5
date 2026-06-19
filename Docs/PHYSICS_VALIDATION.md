# CricketSim — Physics Validation

> Whether the ball behaves like a cricket ball. The physics core already ships with a
> deep automation suite; this document maps those tests to the brief's validation
> checklist, records what is proven today, and flags the one calibration gap.

The physics model is a custom fixed-substep RK4 aerodynamic integrator (drag, Magnus,
swing, seam) with analytic pitch-contact resolution — see `BALL_PHYSICS.md` and
`PITCH_SYSTEM.md`. It is pure SI and unit-testable headlessly.

Run: `Automation RunTests CricketSim.Physics+CricketSim.Bowling+CricketSim.Pitch+CricketSim.Bat`

---

## 1. Coverage map

| Brief check | Validated by | What is asserted today | Status |
|---|---|---|:--:|
| **Swing amount** | `Physics.ConventionalSwing`, `Bowling.Outswing/Inswing` | Seam orientation produces lateral force toward the seam side (+Y); mirrored seam swings −Y; integrated flight deviates laterally. | ✅ direction & sign |
| **Reverse swing** | `Physics.ReverseSwing`, `Bowling.ReverseSwing` | Above the reverse-swing speed threshold the side force **flips** relative to conventional (−Y vs +Y). | ✅ direction & sign |
| **Seam movement** | `Bowling.WobbleSeam`, `Pitch.WobbleSeam`, `Pitch.SameSeamDifferentPitches` | Seam-strike deviation off the pitch; same seam on different surfaces deviates by surface-appropriate amounts. | ✅ direction & surface response |
| **Spin drift** | `Physics.MagnusDrift`, `Physics.MagnusDip` | Side-spin produces in-air lateral drift; top-spin adds dip (downward Magnus). | ✅ direction & sign |
| **Spin turn** | `Physics.PitchTurn`, `Bowling.OffSpin/LegSpin`, `Pitch.SameSpinDifferentPitches` | Spin about the line of flight grips and deflects off the pitch; reversing spin turns the other way; **turn is symmetric**; turn scales with surface. | ✅ direction, symmetry, surface response |
| **Bounce consistency** | `Pitch.RestitutionMonotonic`, `Pitch.GripSkidThreshold`, `Pitch.Determinism` | Restitution responds monotonically to surface; grip/skid threshold behaves; **bit-identical** bounce for the same landing (deterministic variance hash). | ✅ consistency & determinism |
| **Bat–ball outcomes** | `Bat.CenterImpact`, `Bat.BatSpeed`, `Bat.ImpactLocationSweep`, `Bat.MistimeProducesEdge`, `Bat.EnergyConservation`, `Bat.ShotHemispheres` | Centre vs edge impact, bat-speed scaling, impact-location sweep, mistime → edge, energy conservation, shot direction by hemisphere. | ✅ qualitative + 1 known fail |

Determinism (`Physics.Determinism`, `Bowling.Determinism`, `Bat.Determinism`,
`Pitch.Determinism`) guarantees an identical delta-stream reproduces a flight
bit-for-bit — the basis for replay and repro.

---

## 2. What this proves — and the one gap

**Proven:** every aerodynamic and pitch effect acts in the **correct direction, with
the correct sign and symmetry, and responds correctly to surface and seam** — and is
fully deterministic. The physics is behaviourally cricket-correct.

**Gap (calibration):** the suite asserts *direction and relative magnitude*, not
*absolute magnitude against published reference figures*. We do not yet assert, e.g.,
"a 135 km/h outswinger on a swinging day deviates 60–80 cm over 20 m" or "a good legspinner
turns 4–8° off a dry pitch." The model is grounded in wind-tunnel coefficients
(`CricketAeroCoefficients`), so the magnitudes are *plausible*, but they are not pinned
to a measured band the way the **gameplay** metrics now are in `VALIDATION_REPORT.md`.

### Real-world reference bands (for the recommended magnitude tests)

| Quantity | Approx. real-world range | Suggested measurement |
|---|---|---|
| Conventional swing (lateral) | 40–90 cm over a full length | integrate release → pitch, measure Y displacement |
| Reverse swing (lateral) | 50–110 cm, later & sharper | same, above reverse threshold |
| Seam deviation off pitch | 2–15 cm | pre/post-bounce Y delta on a seam strike |
| Spin drift (in air) | 20–60 cm | Y displacement, side-spin, no pitch |
| Spin turn off pitch | 2–8° (off/leg), up to ~10° rank turner | post-bounce direction change angle |
| Bounce height (good length) | 65–95 cm at the stumps | apex Z after bounce |

**Recommendation:** add a `CricketSim.Physics.Magnitudes` test group asserting the above
bands against `FCricketBenchmarkRange`, reusing the analytics grading from the
balancing framework. This extends statistical validation from gameplay down into the
physics layer. Tracked in `TUNING_RECOMMENDATIONS.md` §Physics magnitudes.

---

## 3. Known failure

`CricketSim.Bat.EdgeImpact` is a **pre-existing** bat–ball collision tuning failure,
unrelated to the balancing work, and is the only red test in the project. It should be
resolved as part of bat-collision calibration (not gameplay balancing).
