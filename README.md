# CricketSim

A **physics-first cricket simulation** built in Unreal Engine 5. The cricket
ball is the protagonist: swing, reverse swing, seam movement, wobble seam, spin,
drift, dip, bounce variation and bat–ball contact all **emerge from an
aerodynamic + contact model** — never from scripted trajectories.

> Status: **Phase 0 complete** — engine architecture, module split, and the
> deterministic ball-physics core are implemented and building on macOS
> (UE 5.7.4). See `Docs/ROADMAP.md`.

## What exists today

- A four-module C++ architecture with a strict dependency graph.
- A complete, SI-unit, deterministic **ball-flight model**: quadratic drag with a
  Reynolds-driven drag crisis, Magnus (carry/dip/drift), conventional **and**
  reverse swing from one seam-presentation equation, wobble-seam precession, and
  spin decay — integrated with fixed-substep RK4.
- An impulse-based **pitch-interaction model**: speed-dependent restitution,
  friction grip/skid, spin→translation **turn**, seam movement off the pitch, and
  deterministic bounce variation.
- A gameplay bridge (`ACricketBall` + `UCricketBallPhysicsComponent`) and the
  scaffolding for T20 rules, teams (India/Australia), and game mode.
- Headless automation tests covering determinism and swing/Magnus correctness.

## Documentation

| Doc | What it covers |
|---|---|
| [`Docs/ARCHITECTURE.md`](Docs/ARCHITECTURE.md) | Engine choices, module layout, the custom-integrator-vs-Chaos decision, determinism, units. |
| [`Docs/BALL_PHYSICS.md`](Docs/BALL_PHYSICS.md) | The full physical model — equations, coefficients, validation checklist. |
| [`Docs/ROADMAP.md`](Docs/ROADMAP.md) | Phased plan from physics core → bowling → batting → fielding → rules → cross-platform. |

## Modules

```
CricketSimEditor → CricketSim → CricketGameplay → CricketPhysics → Engine
```

- **CricketPhysics** — pure SI physics (aerodynamics, integrator, pitch). No
  actors/world; unit-testable.
- **CricketGameplay** — actors/components binding physics to the world.
- **CricketSim** — primary game module: T20 rules, game mode, team data.
- **CricketSimEditor** — editor-only tuning/validation tooling.

## Build & run (macOS)

```sh
ENGINE="/Users/Shared/Epic Games/UE_5.7/Engine"

# Build the editor target
"$ENGINE/Build/BatchFiles/Mac/Build.sh" CricketSimEditor Mac Development \
    -project="$PWD/CricketSim.uproject"

# (optional) generate Xcode project files
"$ENGINE/Build/BatchFiles/Mac/GenerateProjectFiles.sh" \
    -project="$PWD/CricketSim.uproject" -game

# open in the editor
open "$ENGINE/Binaries/Mac/UnrealEditor.app" --args "$PWD/CricketSim.uproject"
```

Run the physics tests headlessly:

```sh
"$ENGINE/Binaries/Mac/UnrealEditor-Cmd" "$PWD/CricketSim.uproject" \
    -ExecCmds="Automation RunTests CricketSim.Physics; Quit" -unattended -nullrhi
```

## Principles

1. **Physics realism** — the brief's first priority.
2. **Clean architecture** — strict module boundaries, physics free of engine cruft.
3. **Scalability** — data-driven tuning, deterministic core.
4. **Fast iteration** — coefficients are content, not code.

Outcomes emerge from physics; player/AI skill biases the *inputs* to the model
(release pace, spin, bat speed/angle), never the result. All randomness is
explicit and seedable, applied at the input layer — never inside `CricketPhysics`.

### Out of scope
Open world, career mode, tournaments, multiplayer, microtransactions, live
service. UI/presentation/commentary/crowds are deferred until the simulation is real.
