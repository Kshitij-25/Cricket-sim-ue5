# CricketSim — Character Pipeline

> One skeleton. One skeletal mesh. Team and archetype are data, not geometry.

Read `Docs/ARCHITECTURE.md` and `Docs/ANIMATION_SYSTEM.md` first. `ANIMATION_SYSTEM.md`
documented the project as "deliberately asset-free" — no skeletal meshes existed
anywhere in the codebase. This doc is the production follow-up: the actual character
*pipeline* (C++ classes + Content folder structure + materials + Blueprints), built
and verified live via Unreal MCP against the running editor, ready to receive real
rigged art the moment it's sourced.

---

## 0. What this pipeline does NOT do, and why

Before the architecture: a hard capability boundary that shaped every decision below.

**There is no tool, in this MCP server or in Unreal generally, that synthesizes 3D
geometry or a bone hierarchy from nothing.** `SkeletalMeshTools.import_file` imports an
*already-rigged* mesh+skeleton from an FBX on disk; everything else (sockets, physics
assets, LODs, materials) operates on a mesh that already exists. This project ships
with no character art and no bundled Mannequin/sample content (confirmed: zero
`SkeletalMesh`/`Skeleton` assets anywhere in `Content/`, and no Manny/Quinn/ThirdPerson
sample content enabled as a plugin).

So the work that's actually possible without sourced art is: build everything *around*
the mesh slot so dropping in a rigged FBX per archetype is the only remaining step. That
is what this pipeline is. Section 6 is the exact import workflow to run that day.

---

## 1. Character architecture

One C++ base actor, `ACricketCharacter` (`Source/CricketGameplay/Public/CricketCharacter.h`),
is the shared body for **every** on-field human — any team, any archetype, future
custom players, career mode. It is deliberately the only character class in C++;
everything that varies (team, archetype, gear) is data layered on top, never a
parallel class hierarchy.

```
ACharacter (engine)
 └─ ACricketCharacter                          [Source/CricketGameplay]
      ├─ USkeletalMeshComponent (GetMesh())     — one shared body mesh + skeleton
      ├─ UCricketCharacterAnimComponent         — existing AnimController (unchanged)
      ├─ ECricketPlayerArchetype Archetype      — Batter | FastBowler | Spinner | Fielder
      ├─ ApplyTeamKit(UCricketTeamKitDataAsset*)     — material/colour swap, no mesh swap
      ├─ ApplyCosmeticLoadout(TArray<FCricketCosmeticAttachment>) — socket-attached gear
      └─ OnArchetypeChanged (BlueprintImplementableEvent) — AnimBP links its Anim Layer here

      Blueprint layer (Content/Characters/Archetypes):
       BP_CricketCharacter_Base                — sets DefaultKitMaterial, shared defaults
        ├─ BP_Batter        (Archetype = Batter)
        ├─ BP_FastBowler    (Archetype = FastBowler)
        ├─ BP_Spinner       (Archetype = Spinner)
        └─ BP_Fielder       (Archetype = Fielder)
```

Three things are deliberately **not** separate classes, because they are not separate
bodies:

- **Archetype is presentation, not identity.** `ECricketPlayerArchetype`
  (`Source/CricketGameplay/Public/CricketPlayerArchetype.h`) selects which Animation
  Layer (idle stance, run-up style) an Anim Blueprint links — it is read by
  `OnArchetypeChanged`, never baked into a mesh. A pace bowler is `FastBowler` while
  bowling, `Batter` while batting, `Fielder` otherwise — same actor, same skeleton,
  `SetArchetype()` just swaps which layer graph drives it. This matches the codebase's
  existing `ECricketRole` (`CricketTeamDataAsset.h`) used for gameplay/AI; archetype is
  the visual analogue, kept in `CricketGameplay` rather than imported from `CricketSim`
  to avoid a circular module dependency (`CricketSim` depends on `CricketGameplay`, not
  the reverse).
- **Team is a material instance, not a mesh.** `ApplyTeamKit` creates a
  `UMaterialInstanceDynamic` from the team's kit material and writes
  `PrimaryColor`/`SecondaryColor`/`TrimColor`. Adding a new team is a new
  `UCricketTeamKitDataAsset` instance — zero C++, zero new meshes.
- **Gear is a data array, not fixed components.** `FCricketCosmeticAttachment`
  (socket name + static mesh) replaces what older "modular character" pipelines did
  with separate skinned torso/legs/head meshes. A bat, helmet, pads, or a future
  custom player's signature gear is one array entry, attached at runtime via
  `ApplyCosmeticLoadout`. The existing `UCricketBattingComponent::SetBatVisual` hook
  is wired automatically when a `socket_bat_grip` attachment is applied
  (`CricketCharacter.cpp`), so the bat the batting physics already simulates is the
  same mesh the gear system attaches — no duplicate bat actor.

The existing `UCricketCharacterAnimComponent` (`CricketCharacterAnimComponent.h`) is
untouched. It already derives locomotion/batting/bowling/fielding state from whatever
gameplay components share its owning actor — `ACricketCharacter` just gives it a body
and a mesh to animate. The "physics is the source of truth, animation visualizes it"
principle from `ANIMATION_SYSTEM.md` is unaffected.

---

## 2. Skeleton strategy

**One shared skeleton, period.** Every archetype, every team, every custom player
retargets onto it — no per-archetype or per-team skeleton variants. This is what makes
the rest of the pipeline (one Anim Blueprint hierarchy, one set of locomotion/bowling/
batting/fielding animations) scale to N teams without N animation sets.

Recommendation: rig the source character in the DCC tool against the **UE5 standard
Mannequin skeleton** (UE5_Mannequin_Skeleton bone names — `pelvis`, `spine_01..05`,
`clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r`, `thigh_l/r`, `calf_l/r`,
`foot_l/r`, `head`, `neck_01`). Reasons:
- UE5's **IK Retargeter** can retarget any UE5-Mannequin-compatible animation (Mixamo
  via the standard retarget, Marketplace packs, Epic's own libraries) onto this
  skeleton with no extra IK rig authoring.
- Cricket-specific actions (the bowling run-up montage, batting strokes, throws) are
  still custom-authored — only the *skeleton bone naming*, not the animations, needs to
  match a known standard. That naming is what makes retargeting unattended.
- All four archetypes share builds close enough in proportion (human adult athletes)
  that one skeleton with sensible default bone lengths covers them; per-player build
  variance (height, frame) is a uniform scale on `RootBone` / a small set of bone-scale
  curves, not a different skeleton.

Required sockets on the shared skeleton (created via `SkeletalMeshTools.add_socket`
once a mesh is imported — see §6):
| Socket | Parent bone | Used by |
|---|---|---|
| `socket_bat_grip` | `hand_r` | Bat mesh; auto-wired to `UCricketBattingComponent::SetBatVisual` |
| `socket_head` | `head` | Cap / helmet |
| `socket_ball` | `hand_r` | Ball-in-hand pose before release (bowling) |

`ACricketCharacter::ApplyCosmeticLoadout` attaches by socket name, so adding a new gear
socket later (pads, gloves) is a one-line `add_socket` call, no C++ change.

---

## 3. Asset structure

Built live via Unreal MCP against the running editor (`AssetTools.create_folder`,
`MaterialTools`, `DataAssetTools`, `BlueprintTools`) — every path below exists in
`Content/Characters` right now:

```
/Game/Characters/
├── Shared/
│   ├── Skeleton/            ← shared Skeleton + base SkeletalMesh land here on import
│   ├── Meshes/               ← shared body SkeletalMesh variants (build/height), if any
│   ├── Materials/
│   │   └── M_CricketKit      ← master material (see §4)
│   └── Animations/
│       ├── Locomotion/
│       ├── Batting/
│       ├── Bowling/Fast/
│       ├── Bowling/Spin/
│       └── Fielding/
├── Archetypes/
│   ├── BP_CricketCharacter_Base   (parent: ACricketCharacter)
│   ├── BP_Batter                  (Archetype = Batter)
│   ├── BP_FastBowler               (Archetype = FastBowler)
│   ├── BP_Spinner                  (Archetype = Spinner)
│   └── BP_Fielder                  (Archetype = Fielder)
├── Teams/
│   ├── India/
│   │   ├── MI_Kit_India        (parent: M_CricketKit; blue/navy/saffron)
│   │   └── DA_Kit_India        (UCricketTeamKitDataAsset; ShortCode "IND")
│   └── Australia/
│       ├── MI_Kit_Australia    (parent: M_CricketKit; gold/green/white)
│       └── DA_Kit_Australia    (UCricketTeamKitDataAsset; ShortCode "AUS")
└── Gear/                        ← cosmetic StaticMeshes (bat, cap, pads, helmet)
```

`DA_Kit_India`/`DA_Kit_Australia` join the existing gameplay roster
(`Content/Data/Teams/DA_Team_India`, `DA_Team_Australia` —
`UCricketTeamDataAsset`/`FCricketPlayer`, `CricketSim` module) by `ShortCode`
("IND"/"AUS") at runtime. The two are intentionally separate assets in separate
modules: `UCricketTeamDataAsset` is gameplay/AI data (ratings, role, name) in the
sim-rules module; `UCricketTeamKitDataAsset` is rendering data (colours, material,
mesh override) in the gameplay/visual module. Neither needs to reference the other's
module.

**Adding a third team** (the "future-proof for additional teams" requirement): one new
`UCricketTeamKitDataAsset` instance + one `MaterialInstanceConstant` + a folder. No C++,
no new Blueprint, no new skeleton.

**Adding a custom player** (career mode): an `FCricketCosmeticAttachment` array
(signature bat, gear) plus whichever `UCricketTeamKitDataAsset` they play for. If a
custom player needs a build outside the shared mesh's range, `BodyMeshOverride` on the
kit asset (or a per-player equivalent) swaps just the SkeletalMesh — same skeleton, same
animations, so nothing else changes.

---

## 4. Material / kit setup

`M_CricketKit` (`/Game/Characters/Shared/Materials/M_CricketKit`) is the one master
material every team kit instances:

| Parameter | Group | Default | Drives |
|---|---|---|---|
| `PrimaryColor` (Vector) | Team Kit | white | `BaseColor` |
| `SecondaryColor` (Vector) | Team Kit | near-black | exposed for trim/mask use once a kit-region texture is authored |
| `TrimColor` (Vector) | Team Kit | mid-grey | exposed for trim/mask use |
| `Roughness` (Scalar) | Fabric | 0.6 | `Roughness` |
| `Metallic` (Scalar) | Fabric | 0.0 | `Metallic` |

Right now `BaseColor` is wired straight to `PrimaryColor` — a solid-colour kit, which is
exactly correct for a body with no UV-mapped kit-region texture yet. The moment a real
mesh ships with a kit mask texture (R = primary region, G = secondary, B = trim), wire a
`TextureSampleParameter2D` + a 3-way `Lerp` into `BaseColor` using that mask; the three
colour parameters and every `MaterialInstanceConstant`/`UCricketTeamKitDataAsset`
already in place need no changes — they were authored against this exact contract.

`MI_Kit_India` / `MI_Kit_Australia` are instances of `M_CricketKit` with each team's
colours baked in and referenced from `DA_Kit_India`/`DA_Kit_Australia`.
`ACricketCharacter::ApplyTeamKit` reads the `UCricketTeamKitDataAsset`, makes a dynamic
instance of its `KitMaterial`, and pushes the three colours — so a kit can also be
recoloured at runtime (e.g. a custom/away kit) without touching the asset.

---

## 5. Animation compatibility plan

All four archetypes and all teams share **one skeleton** (§2) and therefore can share
**one locomotion/action animation set** — the only thing the AnimBP needs to vary per
archetype is *which idle stance and run-up style plays*, not which skeleton or rig.

Use UE5 **Linked Anim Layers**: one parent Anim Blueprint (call it `ABP_CricketCharacter`,
authored once a mesh exists) defines the layer interface (Locomotion, BattingAction,
BowlingAction, FieldingAction); each archetype gets a thin **Linked Anim Layer** Blueprint
overriding only `BowlingAction`/idle pose for its archetype (e.g. `AL_FastBowler` has a
sprint-heavy run-up graph, `AL_Spinner` a shorter, slower one), while Locomotion,
BattingAction, and FieldingAction graphs are shared and untouched. `ACricketCharacter::
OnArchetypeChanged` (a `BlueprintImplementableEvent`) is exactly where the parent AnimBP
calls `Link Anim Class Layers` to swap in the archetype's layer — already wired, waiting
for the AnimBP to exist.

Every graph in every layer reads the same source of truth as before:
`UCricketCharacterAnimComponent`'s getters (`GetLocomotion()`, `GetBowlingState()`,
`GetBattingState()`, `GetFieldingState()`, `GetReleaseOffsetM()`, ...) — see
`ANIMATION_SYSTEM.md` §3 for the full AnimBP wiring contract, which is unchanged by this
pipeline. Archetype selects *which graph* reads them, never *what they report*.

LOD: no custom system needed. `USkeletalMeshComponent`'s standard per-instance LOD
(distance-based or forced) applies automatically once the imported mesh ships LODs
(`SkeletalMeshTools.import_file` / a DCC LOD chain or Skeletal Mesh LOD tool). Document
requirement for sourced art: **ship at least LOD0–LOD3** (cinematic/close fielding camera
down to far-stand crowd shots) per the existing camera system's range
(`CAMERA_REPLAY_SYSTEM.md`).

---

## 6. Unreal asset generation workflow

What's already done (this pass, verified live against the running editor via MCP):

1. C++: `ECricketPlayerArchetype`, `UCricketTeamKitDataAsset`, `ACricketCharacter` added
   to `CricketGameplay`; built clean (`Build.sh CricketSimEditor Mac Development`); the
   live MCP-connected editor picked up the new classes without a restart.
2. MCP: created the full `/Game/Characters` folder tree (§3).
3. MCP: built `M_CricketKit` (parameters wired, compiled clean), `MI_Kit_India`,
   `MI_Kit_Australia`, `DA_Kit_India`, `DA_Kit_Australia` — all saved.
4. MCP: built `BP_CricketCharacter_Base` + `BP_Batter`/`BP_FastBowler`/`BP_Spinner`/
   `BP_Fielder`, each with the correct `Archetype` and `DefaultKitMaterial` set,
   compiled, and saved.

**Update — placeholder art sourced (this pass):** no FBX existed to import, so instead
of waiting on commissioned rigged art, the engine's own UE5-Mannequin template content
(Manny/Quinn — `Engine install/Templates/TemplateResources/High/Characters/Content/
Mannequins`, the same bundle a stock "Third Person" template project gets) was copied
on disk into `/Game/Characters/Mannequins/` (preserving its original relative path so
internal cross-references between the Skeleton/SkeletalMesh/AnimSequences stay intact),
then `AssetTools.move`d into the paths this doc specifies: `SK_Mannequin` →
`/Game/Characters/Shared/Skeleton/SK_Cricket_Skeleton`, `SKM_Manny_Simple` →
`/Game/Characters/Shared/Meshes/SKM_Cricket_Base` (Quinn kept alongside as
`SKM_Cricket_BuildVariant_Quinn`), with `PA_Mannequin` → `PA_Cricket_Skeleton`. Bone
names matched the §2 requirement exactly (full production rig — correctives, IK bones,
the works), so the three required sockets (`socket_bat_grip`, `socket_head`,
`socket_ball`) were added and all five archetype Blueprints now point their mesh
component at `SKM_Cricket_Base`. This is placeholder, not final art — swapping in a
commissioned/Mixamo rig later is the same `import_file` workflow below, passing the
existing `SK_Cricket_Skeleton` in as `skeleton=` so nothing downstream (sockets,
Blueprints, the animation set in `MOTION_CAPTURE_STRATEGY.md`) needs to change.

**What's left for final (non-placeholder) art:** a commissioned/Mixamo FBX rigged to
this same skeleton naming. The import is a single MCP call sequence, repeatable per
archetype:

```python
# 1. Import the rigged mesh once (creates the shared Skeleton on the first import only;
#    every subsequent archetype import passes that Skeleton in to share it).
SkeletalMeshTools.import_file(
    folder_path="/Game/Characters/Shared/Meshes",
    asset_name="SK_Batter_Build01",
    source_file="<absolute path to .fbx>",
    skeleton=None,                 # first import: creates /Game/Characters/Shared/Skeleton/SK_Cricket_Skeleton
    import_materials=True, import_textures=True,
    import_animations=False,       # animations imported separately per §5
    create_physics_asset=True,
)

# 2. Add the required sockets once per shared skeleton (§2 table).
SkeletalMeshTools.add_socket(mesh, "socket_bat_grip", "hand_r")
SkeletalMeshTools.add_socket(mesh, "socket_head", "head")
SkeletalMeshTools.add_socket(mesh, "socket_ball", "hand_r")

# 3. Point each archetype Blueprint's mesh component at the imported SkeletalMesh
#    (ObjectTools.set_properties on the Blueprint CDO's Mesh component), assign the
#    Anim Blueprint class, and compile.

# 4. Re-run ApplyTeamKit in PIE per team to confirm the kit material reads correctly
#    on the real mesh's material slot 0.
```

Every subsequent team or archetype after the first import reuses the same `Skeleton`
asset (pass it as `skeleton=` instead of `None`) — that's what makes the whole roster
(11 archetypes × N teams × custom players) a data problem, not an asset-authoring
explosion.
