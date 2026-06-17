# CricketSim — Stadium & Environment System

> The stadium is a SIMULATION ENVIRONMENT, not a visual asset. It is geometry and
> rules — ground dimensions, the boundary, fielding positions, and the atmosphere
> the ball flies through — that the physics, fielding and match systems plug into.

Read `Docs/ARCHITECTURE.md` first. No visual polish here by design; the priorities
are gameplay accuracy, scale correctness, performance, and extensibility.

---

## 1. Stadium architecture

A pure **geometry + rules model** (`FCricketStadiumModel`) over a data description
of the ground (`FCricketGroundDimensions`), wrapped by the gameplay **Stadium
Manager** (`ACricketStadium`) that builds the geometry from its own transform,
watches the live ball, and drives the venue. Because the model is pure, every
boundary call and fielding position is headless-testable.

```
 ACricketStadium (transform + sizes) ─▶ FCricketGroundDimensions
        │                                     │
        │  watches the live ball              ├─ FCricketStadiumModel::IsInside / ClassifyBoundary
        │  pushes atmosphere to the ball      ├─ FieldPositionWorldM (scaled to the ground)
        ▼                                     └─ ValidateBoundaryCatch
   OnBoundaryEvent(Four/Six) ─▶ (match driver scores it)
```

The five required subsystems:

1. **Stadium Manager** → `ACricketStadium` (owns geometry, environment, field, ball watch).
2. **Boundary System** → live four/six detection + `ClassifyBoundary` + `ValidateBoundaryCatch`.
3. **Field Position System** → `FieldPositionWorldM` + `FCricketFieldPlacement`.
4. **Environment Configuration System** → `FCricketVenueEnvironment`.
5. **Match Environment Controller** → `ApplyEnvironmentToBall` / `SetTimeOfDay` / `SetWindMS` + the boundary→match-result hook.

---

## 2. Environment architecture

`FCricketVenueEnvironment` holds the **time of day** (day / twilight / night +
floodlights) and an `FCricketEnvironment` **atmosphere** — and that atmosphere is
the *same struct the ball aerodynamics already consume*. So wind, humidity and
pressure are a **real, wired integration**: `SetWindMS(...)` pushes onto the ball
integrator and changes the next delivery's flight. Day/night is the floodlight +
future-visibility hook on top. Weather is architected (the config + the apply path)
but not simulated beyond what the aero already does — exactly as the brief asks.

---

## 3. Data models

In `CricketStadiumTypes.h` (SI metres).

| Type | Role |
|---|---|
| `FCricketGroundDimensions` | Centre, pitch axis, straight & square boundary, rope height, pitch length; helpers for the stumps and the off-side axis. |
| `ECricketFieldPosition` | The named positions (slip…third man) + deep variants. |
| `FCricketFieldPositionDef` | A position's parametric angle (from straight, toward off) + depth fraction. |
| `FCricketFieldPlacement` | A named arrangement of positions — the unit a captain-AI will choose/mutate. |
| `ECricketBoundaryResult` | None / InPlay / Four / Six / CaughtAtBoundary. |
| `FCricketVenueEnvironment` | Time of day + floodlights + the aero `FCricketEnvironment`. |
| `ECricketTimeOfDay` | Day / Twilight / Night. |

**Boundary as an ellipse:** straight (along the pitch axis) and square
(perpendicular) distances are independent, so real grounds — a long straight boundary
and short square, or vice-versa — are a data change, and the radius in any direction
is one closed-form expression.

---

## 4. Boundary system design

- **Geometry**: `BoundaryRadiusAtAngleM`, `SignedDistanceInsideM` (positive inside,
  negative over the rope), `IsInsideBoundary`, `BoundaryPointM`.
- **Four vs Six**: `ClassifyBoundary(dims, path, firstBounceIdx)` walks the **actual
  recorded ball path** and, at the first point outside the rope, returns **Four** if
  the ball had bounced *inside* before crossing, **Six** if it cleared the rope on
  the full, **InPlay** if it never crossed. The Stadium Manager does this live:
  it tracks the first bounce (was it inside?) and the rope crossing, then fires
  `OnBoundaryEvent` — physics decides where the ball went; the stadium only reads it.
- **Catch at the boundary**: `ValidateBoundaryCatch(dims, catchPoint)` returns
  *CaughtAtBoundary* if the catch is inside the rope, *Six* if a foot is over it —
  ground-relative, so the same catch is out on a big ground and six on a small one.
- **Rope interaction / retrieval**: a ball that crosses is frozen (dead); a ball
  that stops inside is recorded as in-play (fielded).

---

## 5. Field position system

`FieldPositionWorldM(dims, position, bRightHanded)` places any named position from a
parametric `(angle, depth)`: the angle is measured around the striker (0 = straight
down the ground, + toward the off side), the depth is a fraction of the **boundary
radius in that direction**, so positions automatically **scale with ground size** and
sit the right distance out. Left-handers mirror in Y. Deep positions behind the
striker are clamped a safe margin inside the rope. `DefaultField()` is a balanced
set; the whole thing is built to be chosen and mutated by a future captain-AI.

---

## 6. Debug tooling

`ACricketStadium` draws (gated by `cricket.Debug.Stadium`):

- **Boundary line** (the rope ellipse) and the **catch zone** band just inside it.
- The **30-yard inner ring** and the **pitch** rectangle.
- **Fielding positions** — markers + labels at every position in the active field.
- **Sight screens** beyond each straight boundary, **umpire** positions (bowler's
  end + square leg), and a **team area / pavilion** beyond the square boundary.
- **Ball-landing heatmap** — every delivery's landing marked, coloured by outcome.
- **Shot distribution** — a wagon wheel of lines from the striker to each boundary.
- An on-screen readout of the dimensions, time of day, wind, and 4s/6s tally.

---

## 7. Testing strategy

Headless suite `CricketSim.Stadium` (`CricketPhysics/Private/CricketStadiumTests.cpp`).
Geometry and rules are pure, so they are tested without a level.

| Test | Proves |
|---|---|
| `BoundaryGeometry` | Ellipse radius = straight/square on-axis; inside/outside is correct. |
| `Four` | A ball that bounces inside then crosses the rope is a four. |
| `Six` | A ball that clears the rope on the full is a six. |
| `BoundaryCatch` | A catch inside the rope is out, over it is six; the same spot flips on a smaller ground. |
| `GroundSizes` | The same hit is a six on a small ground, in-play on a big one. |
| `FieldPlacement` | Positions are inside the rope, on the correct side, behind/forward as expected, mirror for a lefty, and scale with ground size. |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Stadium; Quit" -unattended -nullrhi -nosplash
# results in ~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log
```

(The pre-existing `CricketSim.Bat.EdgeImpact` failure is unrelated and predates this work.)

---

## 8. Production-ready C++ — file map

| File | Module | Contents |
|---|---|---|
| `CricketStadiumTypes.h` | CricketPhysics | Data models (§3). |
| `CricketStadiumModel.{h,cpp}` | CricketPhysics | Boundary geometry/rules + field positions (§4,§5). |
| `CricketStadiumTests.cpp` | CricketPhysics/Private | The `CricketSim.Stadium` suite (§7). |
| `CricketStadium.{h,cpp}` | CricketGameplay | Stadium Manager + Environment Controller + debug (§1,§2,§6). |

### Trying it in the editor

Drop an `ACricketStadium` into the level alongside an `ACricketFieldingRig` and
press Play (the stadium auto-finds the ball). Launch deliveries from the rig
(**1–5**): the stadium overlays the rope, the inner ring, the pitch, the fielding
positions, the sight screens, the umpires and the pavilion, and as balls reach the
rope it calls fours and sixes and builds the landing heatmap + wagon wheel. The
boundary calls follow the *actual* ball path; change `StraightBoundaryM` /
`SquareBoundaryM` and the same shots are scored differently.

---

## 9. Boundaries & future work

- **In scope, done:** one MVP ground with configurable dimensions, accurate pitch
  placement, the boundary system (four/six/catch/retrieval), the full named field
  position system, day/night + the wired atmosphere, debug visualization, and tests.
- **Architected, not implemented:** weather and dynamic wind/humidity beyond what the
  aero already does (the config + apply path exist), and the captain-AI / field-setting
  system (the placement model is built for it).
- **Deliberately not done:** any visual/art content — this milestone is the
  simulation environment, not the look.
```
