# CricketSim — Motion Capture & Motion Matching Strategy

> One skeleton, two playback strategies. Continuous movement is **motion-matched**;
> physics-critical actions stay **authored montages** that motion matching only
> blends into and out of. Physics remains the source of truth either way.

Read `Docs/ANIMATION_SYSTEM.md` and `Docs/CHARACTER_PIPELINE.md` first. This doc is
the production plan for the mocap library and UE5.8 Motion Matching setup that those
two land on: `CHARACTER_PIPELINE.md` built every C++/Blueprint/material/socket
structure around an empty mesh slot; this does the same for the empty animation
slots, plus the Motion Matching infrastructure that didn't exist when
`ANIMATION_SYSTEM.md` was written ("§8 Future: retarget-ready skeleton + motion
captures behind these states").

---

## 0. Capability boundary (verified live against the running editor via MCP)

Same finding as `CHARACTER_PIPELINE.md` §0, one level up the pipeline: **there is no
tool, in this MCP server or in Unreal generally, that synthesizes a mocap performance
or builds a `PoseSearchDatabase`/`PoseSearchSchema`/`MirrorDataTable`/`AnimSequence`/
`AnimMontage` from nothing.** Checked every relevant toolset against the live editor:

- `DataAssetTools.create` only instantiates **`UDataAsset` subclasses** — Pose Search
  and animation classes are not `UDataAsset`, so this tool cannot create them.
- `SkeletalMeshTools` covers mesh/socket/bone inspection, not animation assets.
- `MaterialTools` / `MaterialInstanceTools` / `CurveTableTools` / `DataTableTools` /
  `StringTableTools` are each scoped to exactly one asset class, none of them animation.
- There is no generic "create asset of arbitrary class" factory tool.

So, same shape of answer as the character pipeline: build everything *around* the
animation slot via MCP, so the moment mocap is captured and imported, the remaining
work (wiring a `PoseSearchSchema`/`PoseSearchDatabase`/Mirror Data Table, slicing
notify timelines into montages) is in-editor authoring a human does once, not a
pipeline that has to be invented that day.

**Done this pass, live, via MCP:**
- Enabled the `PoseSearch` plugin in `CricketSim.uproject` (was present in the
  installed `UE_5.8` engine — confirmed at
  `Engine/Plugins/Animation/PoseSearch` — but not in the project's Plugins array, so
  Motion Matching nodes/assets would not have appeared in the editor at all).
- Created `/Game/Characters/Shared/MotionMatching/` — destination for the
  `PoseSearchSchema`, `PoseSearchDatabase`, and Mirror Data Table assets a human
  builds once clips exist (§4).
- Created `/Game/Characters/Shared/Animations/Bowling/Spin/Off/` and `.../Spin/Leg/`
  — the existing `Bowling/Spin/` folder (from `CHARACTER_PIPELINE.md`) didn't
  distinguish the two spin actions, and they need separate run-up/release captures
  (§2).
- `Locomotion/`, `Batting/`, `Bowling/Fast/`, `Fielding/` folders already existed
  from the character pipeline pass and need no change — their existing flat
  structure is exactly what §6's naming convention is designed for.

**Update — placeholder mocap sourced (this pass):** real capture still doesn't exist,
but rather than ship every gameplay action with zero animation, the engine's own UE5
Mannequin AnimSequence library (Manny/Quinn template content — same source as
`CHARACTER_PIPELINE.md`'s skeleton/mesh update above) was duplicated and renamed onto
`SK_Cricket_Skeleton` per the §6 naming grammar, covering every action this doc and the
goal list call for:

| Slot | Placeholder source clip | Notes |
|---|---|---|
| `AM_Loco_Idle` | `MM_Idle` | |
| `AM_Loco_Walk_Fwd` | `MF_Unarmed_Walk_Fwd` | |
| `AM_Loco_Jog_Fwd` | `MF_Unarmed_Jog_Fwd` | |
| `AM_Loco_Sprint_Fwd` | `MF_Unarmed_Jog_Fwd` (reused) | No distinct sprint clip in the source library — same jog clip stands in until real capture exists. |
| `AM_Bat_Defensive_Front` | `MM_Attack_01` | |
| `AM_Bat_StraightDrive_Front` | `MM_Attack_02` | |
| `AM_Bat_CoverDrive` | `MM_Attack_03` | |
| `AM_Bat_Pull` | `MM_ChargedAttack` | |
| `AM_Bowl_Fast_Full` | `MM_Dash` | |
| `AM_Bowl_OffSpin_Full` | `MM_Attack_01` (reused) | |
| `AM_Bowl_LegSpin_Full` | `MM_Attack_02` (reused) | |
| `AM_Field_Catch_High` | `MM_Land` | |
| `AM_Field_Pickup_Clean` | `MM_Attack_03` (reused) | |
| `AM_Field_Throw_Flat` | `MM_ChargedAttack` (reused) | |

All 14 are real `AnimSequence` assets bound to `SK_Cricket_Skeleton`, saved under the
existing `Animations/{Locomotion,Batting,Bowling/Fast,Bowling/Spin/Off,Bowling/Spin/
Leg,Fielding}/` folders, and verified in PIE with no load/compile errors. **Not done:**
these clips carry no `BallRelease`/`BatImpact`/etc. notify markers, because — confirmed
by reading `CricketAnimationTypes.h`/`CricketCharacterAnimComponent.cpp` this pass —
gameplay does not currently read notifies off animation assets at all. `BallRelease`,
`BatImpact`, `CatchAttempt`, `PickupContact`, and `ThrowRelease` are fired by a pure
C++ deterministic timeline (`FCricketMontagePlayer`, scheduled by `MakeBowlingMontage`/
`MakeBattingMontage`/etc. in `CricketAnimationModel.cpp`), completely decoupled from
whatever `AnimSequence` is visually playing. So placeholder or real, swapping in mocap
today changes the visual only — wiring real notify-track-driven timing (a
`UAnimNotify_CricketGameplayEvent` class read by `UCricketCharacterAnimComponent`,
replacing the synthetic player) is separate, not-yet-started work, exactly as this
section anticipated below.

**What's left for real capture** is everything below, still true for replacing these
placeholders with grounded mocap.

---

## 1. How capture maps onto the existing animation architecture

This is the one decision that shapes the whole library, so it comes before the
animation list. The project already has four state machines and a deterministic
notify-timeline engine (`ANIMATION_SYSTEM.md` §2) that times **one physics event per
action** (`BallRelease`, `BatImpact`, `CatchAttempt`, `ThrowRelease`,
`PickupContact`) at an exact authored frame. Motion Matching's pose-search picks
*whichever frame of whichever clip* best matches the current trajectory — which is
exactly the wrong tool for a system whose entire contract is "this notify fires at
this frame, on time, every time." Mixing the two naively would make bat-ball contact
timing nondeterministic.

So capture and playback split by category, not by a blanket "motion match
everything":

| Category | Playback strategy | Why |
|---|---|---|
| **Locomotion** (Walk/Jog/Sprint/Turn/Stop/Idle) | **Full Motion Matching** — continuous `PoseSearchDatabase`, no authored blendspace | Continuous, no physics-critical instant to protect; this is exactly Motion Matching's home case and replaces what would otherwise be a hand-tuned blendspace. |
| **Batting** (every shot) | **Authored montage**, captured as **one continuous take** per shot (Guard→Backlift→Downswing→Impact→FollowThrough→Recover), notify-marked exactly as today | `BatImpact` must land on the frame the swing model resolved contact. Motion Matching only drives the **entry blend** (locomotion → Guard) and **exit blend** (Recover → locomotion). |
| **Bowling** (Fast / Off Spin / Leg Spin) | **Authored montage**, one continuous take per action (RunUp→DeliveryStride→Release→FollowThrough→Recover) | Same reasoning — `BallRelease` is the protected instant. Motion Matching drives entry (Idle → RunUp start) and exit (Recover → locomotion) only. |
| **Fielding** (Catch/Pickup/Throw) | **Discrete blendable clips**, selected by `ECricketFieldingAnimState`, blended (not pose-searched) between states | The state machine already treats these as separate states the AnimBP transitions between (`ANIMATION_SYSTEM.md` §1: anim "follows" `UCricketFielderComponent`) — not a single continuous performance, so they stay individually triggerable clips. `CatchAttempt`/`ThrowRelease`/`PickupContact` notifies stay frame-exact within each clip. |
| **Fielding — Diving, Sprinting** | **Motion Matching** (sprint loop reuses the Locomotion database) **+ one authored Dive clip** triggered at the dive decision point | Diving is a new addition not in the current `ECricketFieldingAnimState` enum (`Idle·Run·GroundStop·Pickup·Catch·Throw·ReturnToPosition`) — see §2 design note. |

UE5.8's **"Motion Matching for Anim Montages"** feature (matured from the
experimental 5.4/5.5 version) is the mechanism for the entry/exit blends in rows 2–3:
it pose-searches only the *lead-in* and *lead-out* poses of a montage against a small
`PoseSearchDatabase` built from those poses, then hands off to the montage's own
fixed-rate playback — the interior notify timeline is completely untouched. This is
the one place Motion Matching touches the protected actions, and it only ever picks
*which* recovery/entry pose to blend from, never *when* the ball is released or
struck.

---

## 2. Animation list

Naming previews use the convention defined in full in §6. "Full take" = one
continuous captured performance covering every phase of the existing state machine
for that action (no splicing required; phase boundaries become `FCricketAnimNotifyDef`
markers on a single sequence, exactly like the bowling run-up already does per
`ANIMATION_SYSTEM.md`).

### Batting (`AM_Bat_*`, full takes; existing `ECricketBattingAnimState`: Guard→Backlift→Downswing→Impact→FollowThrough→Recover)

| Shot | Footwork | Notes |
|---|---|---|
| Defensive | Front-foot **and** back-foot variants | The two most-played deliveries; both are cheap to capture back-to-back and meaningfully different (downswing angle, weight transfer). |
| Straight drive | Front-foot | Canonical "full extension" shot — good P0 anchor for the whole batting pipeline. |
| Cover drive | Front-foot, off side | |
| Pull shot | Back-foot, leg side, horizontal bat | Distinct backlift arc from the drives — don't try to reuse drive backlift. |
| Cut shot | Back-foot, off side, horizontal bat | Shares the pull's horizontal-bat family; capture back-to-back with Pull for actor/session efficiency. |
| Lofted drive | Front-foot, straight/off | Same downswing family as Straight/Cover drive but with an open-up follow-through for elevation — capture as its own take, not a blend, since the swing model's contact-to-launch-angle relationship needs the real follow-through extension. |

Each shot: capture **3–5 raw takes** of the same shot (not just one) — these become
the variety pool a small `PoseSearchDatabase` selects from at the moment the swing
starts (§4), so the same shot doesn't look identical every time it's played, without
touching the deterministic notify timing of whichever take gets selected.

**Handedness:** capture every shot from a **right-handed batting stance**. Left-handed
batters are *not* a separate capture set — see §4/§5 for why mirroring covers this for
free given the project's one-shared-skeleton design.

### Bowling (`AM_Bowl_*`, full takes; existing `ECricketBowlingAnimState`: Idle→RunUp→DeliveryStride→Release→FollowThrough→Recover)

| Action | Run-up length | Notes |
|---|---|---|
| Fast bowling | Long run-up (15–20 m approach) | Needs the largest capture volume of the whole library — plan the stage for this first. |
| Off spin | Short walk-in (4–6 steps) | Finger-spin wrist/release pose is the part worth extra release-frame fidelity on (see §7 frame-rate note). |
| Leg spin | Short walk-in (4–6 steps), distinct wrist snap | Capture separately from off spin even though the run-up looks similar — the release-frame wrist articulation is what the ball physics' spin model cares about, and reusing the off-spin clip with a different notify time would not change the captured wrist pose. |

Delivery stride and follow-through are **not** separate assets — they're notify-marked
phases inside each of the three takes above, matching how the bowling run-up already
works in the shipped C++ (`MakeBowlingMontage`).

### Fielding (`AM_Field_*`, discrete blendable clips; existing `ECricketFieldingAnimState`)

| Action | Variants to capture | Maps to state |
|---|---|---|
| Catching | High catch, low catch, slip-height catch | `Catch` |
| Ground pickup | Clean pickup (left hand / right hand), pickup-on-the-run | `Pickup` |
| Throwing | Flat overarm throw, direct-hit run-out throw | `Throw` |
| Diving | Dive-and-stop, dive-and-catch | **New** — see design note below |
| Sprinting | Covered by Locomotion's `Sprint` capture (§ below) — no separate fielding sprint clip needed | `Run` |

**Design note — Diving needs a new state.** `ECricketFieldingAnimState` today has no
`Dive` value (`ANIMATION_SYSTEM.md` §2 lists `Idle·Run·GroundStop·Pickup·Catch·Throw·
ReturnToPosition`). Recommend adding `Dive` to the enum (`CricketAnimationTypes.h`)
the same low-risk way the rest of that enum was built — it's a presentation state, not
a physics one, exactly like every other entry. The captured dive clips use **baked,
non-cosmetic-only root motion** for the body's forward/lateral travel during the dive
(the one root-motion exception `ANIMATION_SYSTEM.md` §3 already documents for "fielding
dives"), while the actual catch/stop outcome stays resolved by
`UCricketFielderComponent`'s existing physics, not the clip.

### Locomotion (`AM_Loco_*`, Motion Matching pool; existing `ECricketLocomotionState`: Idle·Walk·Jog·Sprint·Turn·Stop)

| Clip | Direction(s) to capture | Notes |
|---|---|---|
| Idle | Stand, weight-shift idle (1–2 variants) | Not in the goal's list but required — it's already in `ECricketLocomotionState` and is the rest pose every Motion Matching search returns to. |
| Walk | Forward only | Lateral/backward covered by mirroring + the trajectory-driven blend (§4), not separate captures. |
| Jog | Forward only | Same mirroring note. |
| Sprint | Forward only | Same mirroring note; this is also the clip Fielding's "Sprinting" reuses. |
| Turn | 90° left, 90° right, 180° | Capture both left/right even though they're mirror pairs — turns are speed/weight-shift-sensitive enough that motion-matching cost functions benefit from real captured data on both sides, not a guaranteed-clean mirror (see §4 mirroring caveat). |
| Stop | From jog, from sprint | Different deceleration footwork; capture both. |

---

## 3. Capture priority

Three waves, ordered so the **first wave alone exercises every existing notify hook
end-to-end** (proves the whole pipeline before spending budget on shot variety).

### P0 — vertical-slice anchor (exercises every existing gameplay notify)

- Locomotion: Idle, Walk, Jog, Sprint, Turn (one direction), Stop (one variant)
- Bowling: **Fast bowling**, full take (this is the only bowling action currently
  wired live in `ACricketBowlingRig` per `ANIMATION_SYSTEM.md` §4)
- Batting: **Straight drive**, front-foot, full take
- Fielding: one Catch variant, one Pickup variant, one Throw variant

### P1 — full requested library

- Remaining batting shots: Defensive (both footwork variants), Cover drive, Pull
  shot, Cut shot, Lofted drive
- Remaining bowling: Off spin, Leg spin
- Remaining fielding: Diving (both variants), remaining Catch/Pickup/Throw variants
- Remaining locomotion: Turn (other directions), Stop (other variant)

### P2 — Motion Matching quality + variety pool

- 3–5 extra raw takes per batting shot (the variety-pool point in §2)
- Mirror-pair spot captures for Turn if the mirrored result doesn't read well in
  review (§4 caveat)
- Idle weight-shift variants, breathing-idle loops
- Any additional camera-driven flourish (walk-back-to-mark after a delivery,
  acknowledging a boundary) — pure polish, not load-bearing for any system test

No silent scope cuts: if a session runs out of time mid-wave, capture P0 to
completion before starting P1 — a complete P0 set is a working, demonstrable loop;
a partial P1 set with an incomplete P0 is not.

---

## 4. Motion Matching requirements (UE 5.8 Pose Search)

Prerequisite, done this pass: **`PoseSearch` plugin enabled** in `CricketSim.uproject`
(§0). Everything below is authored in-editor once clips exist — no further code
change is required to use it; `UCricketCharacterAnimComponent`'s existing
`GetLocomotion()`/velocity getters are already exactly the signal a Motion Trajectory
component needs (see the trajectory note below).

**Schema (`PS_Schema_Locomotion`, in `/Game/Characters/Shared/MotionMatching/`)**
- Bones sampled: `pelvis`, `foot_l`, `foot_r`, `head` (matches the socket/bone set
  already named in `CHARACTER_PIPELINE.md` §2 — no new bones to add).
- Trajectory channels: past ~0.4 s / future ~0.8 s, position + facing.
- Pose channels: position + velocity per sampled bone.
- **Enable schema mirroring**, pointed at the Mirror Data Table (next item). This is
  what lets one captured-forward Walk/Jog/Sprint clip cover both left- and
  right-leading variants, and — combined with retargeting (§5) — lets the same
  right-handed batting captures serve left-handed batters with no separate shoot.

**Mirror Data Table (`DT_Mirror_CricketSkeleton`)** — one table, authored once
against the shared skeleton (`CHARACTER_PIPELINE.md` §2: "one shared skeleton,
period"), reused by every database below. Bone pairs follow the documented UE5
Mannequin naming (`upperarm_l/r`, `thigh_l/r`, `calf_l/r`, `foot_l/r`, ...).

**Caveat — don't mirror what's inherently handed.** Mirroring is correct for
symmetric locomotion (a left turn is just a mirrored right turn). It is *not*
automatically correct for **batting stance** (a left-handed batter doesn't just mirror
a right-handed swing — grip, head position, and which foot leads all flip together,
but the *visual read* of "which side is the off side" needs a one-time sanity pass)
or for **throwing arm** (most fielders are captured right-arm; mirroring covers a
left-arm thrower's general body mechanics, but flag it for a quick visual QA pass per
§3 P2 rather than assuming it's correct unreviewed).

**Databases (`PoseSearchDatabase`, one per row, all in `MotionMatching/`)**

| Database | Source clips | Used for |
|---|---|---|
| `PSD_Locomotion` | All Locomotion clips (§2), mirrored | The main continuous-movement database every archetype shares. |
| `PSD_Bowling_EntryExit` | First/last ~0.5 s of each Bowling full take | "Motion Matching for Anim Montages" lead-in/lead-out blend only (§1). |
| `PSD_Batting_EntryExit` | First/last ~0.5 s of each Batting full take | Same, for batting. |
| `PSD_Bat_<Shot>` (one per shot) | The 3–5 raw takes of that shot (§2 P2) | Variety-pool selection at swing start — picks the take whose entry pose best matches the live trajectory, then hands off to the deterministic montage. |

**Trajectory source — respect the existing physics-first rule.** `ANIMATION_SYSTEM.md`
§3 is explicit that locomotion is **in-place, capsule-driven** — animation must never
fight the gameplay components' kinematic movement. So the Motion Trajectory feeding
`PSD_Locomotion`'s search must be driven from the **capsule's actual
velocity/acceleration** (exactly what `UCricketCharacterAnimComponent::GetLocomotion()`
already classifies from, per `ANIMATION_SYSTEM.md` §2), not from any root-motion
curve. This is a Motion Trajectory **predicted from gameplay movement input**, the
same pattern Lyra-style Motion Matching projects use, just fed by this project's own
already-existing locomotion classifier instead of a generic character movement
component.

**Cost function tuning starting point:** bias toward continuing-pose (lower
trajectory-error tolerance, higher pose-continuity weight) to avoid foot popping
during the high-frequency direction changes cricket fielding produces (sprint → sudden
stop → turn → sprint again) — tune against the Sprint/Stop/Turn captures specifically,
since those are where a bad cost-function weighting shows up first.

---

## 5. Retargeting strategy

Extends `CHARACTER_PIPELINE.md` §2/§5 — the skeleton plan was written for the
character mesh; this is the same plan applied to mocap clips.

1. **Rig (or retarget) every capture onto UE5-Mannequin-compatible bone names**
   before it reaches this project, exactly as `CHARACTER_PIPELINE.md` §2 already
   requires for the body mesh. This means the IK Retargeter needs **zero new IK Rig
   authoring** for clips that already match — the retarget profile from the mesh
   pipeline (UE5 Mannequin → `SK_Cricket_Skeleton`) is reused unchanged for every
   clip, archetype, and team. One retarget profile, authored once, for the whole
   library — the same "one skeleton" economics that made the character pipeline scale
   to N teams without N asset sets.
2. **If raw mocap comes off a system with its own skeleton** (optical: Vicon; inertial:
   Xsens/Rokoko) rather than already-Mannequin-named, insert **one extra retarget
   hop**: author a single IK Rig for that system's native skeleton once, retarget
   System Skeleton → UE5 Mannequin Skeleton → `SK_Cricket_Skeleton` (the second hop is
   the existing profile from step 1, untouched). The per-clip cost after that one-time
   setup is just export + import, never new retarget authoring — same principle as
   `CHARACTER_PIPELINE.md` §6's "every subsequent archetype reuses the same Skeleton
   asset."
3. **Root motion settings differ by category, matching §1's split:**
   - Locomotion clips: import with root motion **extracted into curves the Motion
     Trajectory reads**, not consumed as actual capsule displacement — the capsule
     stays kinematically driven by gameplay, per `ANIMATION_SYSTEM.md` §3.
   - Bowling run-up / Diving clips: root motion **enabled** for the cosmetic body
     travel only — already the documented exception
     ("baked root motion only for cosmetic body travel that does not affect the
     ball" — `ANIMATION_SYSTEM.md` §3).
   - Batting clips: **in-place** (no meaningful root translation in a batting stance).
4. **Batch the import/retarget via Unreal MCP** once the IK Rig/Retargeter assets
   exist, following the same one-MCP-call-sequence-per-asset pattern
   `CHARACTER_PIPELINE.md` §6 already established for the mesh import — the
   per-clip MCP call (import FBX → assign shared skeleton → run retarget context)
   is mechanical and repeatable across the whole library once the two retarget
   profiles in steps 1–2 exist.
5. **Validate visually, not just numerically.** The IK Retargeter's chain mapping
   (pelvis/spine/arm/leg chains) is authored once and trusted thereafter, but the
   first clip through each chain (especially the bowling run-up's long stride and any
   diving clip) should get a side-by-side visual pass against source footage before
   the rest of that category is batch-retargeted — cheap insurance against a chain
   mapping that's numerically valid but visually wrong (foot sliding, knee pop).

---

## 6. Asset naming conventions

Folder layout already exists from `CHARACTER_PIPELINE.md` §3 (plus §0's two new spin
subfolders this pass):

```
/Game/Characters/Shared/Animations/
├── Locomotion/                 ← AM_Loco_*
├── Batting/                    ← AM_Bat_*
├── Bowling/
│   ├── Fast/                   ← AM_Bowl_Fast_*
│   └── Spin/
│       ├── Off/                 ← AM_Bowl_OffSpin_*
│       └── Leg/                 ← AM_Bowl_LegSpin_*
└── Fielding/                   ← AM_Field_*
/Game/Characters/Shared/MotionMatching/  ← PS_Schema_*, PSD_*, DT_Mirror_*
```

**Grammar:** `AM_<Category>_<Action>[_<Variant>][_<Footwork/Side>][_Take<NN>]`

| Example | Reads as |
|---|---|
| `AM_Loco_Walk_Fwd` | Locomotion, walk, forward |
| `AM_Loco_Turn_90L` / `AM_Loco_Turn_90R` / `AM_Loco_Turn_180` | Locomotion, turn, angle+side |
| `AM_Loco_Stop_FromSprint` | Locomotion, stop, entry-speed variant |
| `AM_Bat_Defensive_Front` / `AM_Bat_Defensive_Back` | Batting, defensive, footwork |
| `AM_Bat_CoverDrive_Take01` … `Take05` | Batting, cover drive, variety-pool raw take |
| `AM_Bowl_Fast_Full` | Bowling, fast, the one continuous take (RunUp→Recover phases inside) |
| `AM_Bowl_OffSpin_Full` / `AM_Bowl_LegSpin_Full` | Bowling, spin type, continuous take |
| `AM_Field_Catch_High` / `_Low` / `_Slip` | Fielding, catch, height variant |
| `AM_Field_Pickup_Clean_L` / `_R` | Fielding, pickup, leading hand |
| `AM_Field_Throw_Flat` / `AM_Field_Throw_RunOut` | Fielding, throw, type |
| `AM_Field_Dive_Stop` / `AM_Field_Dive_Catch` | Fielding, dive, outcome |
| `PS_Schema_Locomotion` | PoseSearchSchema |
| `PSD_Locomotion` / `PSD_Bowling_EntryExit` / `PSD_Bat_CoverDrive` | PoseSearchDatabase |
| `DT_Mirror_CricketSkeleton` | MirrorDataTable |

Rules baked into the grammar:
- **No handedness suffix on captures** (no `_RH`/`_LH`) — every clip is captured
  right-handed/right-arm by convention and mirroring (§4) covers the opposite hand at
  runtime. Don't author a mirrored *asset*; author a mirrored *database entry*.
- **`_Take<NN>` only appears on variety-pool raw takes** (batting shots, §2 P2) —
  every other category captures exactly one clip per variant, so a bare name with no
  `_TakeNN` always means "the" clip for that variant.
- **`_Full` only appears on Bowling/Batting continuous takes** (§1) — it's the visual
  flag that this asset contains an internal notify-marked phase timeline, not a
  short discrete clip like Fielding's.

---

## 7. Capture session logistics

- **Actors:** minimum 2 (bowler + batter for crease-line timing reference, fielder
  partner for catch/throw pairs). A 3rd as a permanent throw/catch partner speeds up
  the Fielding wave considerably — those captures are inherently two-person.
- **Volume:** fast bowling's run-up needs the longest straight-line capture space in
  the whole library (15–20 m approach + follow-through) — size and schedule the stage
  around this requirement first; every other action fits inside a much smaller volume.
- **Props:** mocap-safe bat and ball (real-weight replicas are fine; reflective-marker
  or inertial-suit compatible, depending on system), stump markers for crease-line
  spatial reference during review (not required to be load-bearing in the capture
  itself).
- **Frame rate:** capture at **≥60 fps minimum**, and prefer **120 fps for every
  Batting take** specifically — `BatImpact`'s authored notify frame has to align with
  the swing model's geometrically-resolved contact instant
  (`ANIMATION_SYSTEM.md` §6 `BatCollisionTiming` test), and a slower capture rate
  makes hand-picking that exact frame in review meaningfully harder. Bowling release
  benefits from the same treatment but is less time-critical than bat-ball contact.
- **Cleanup pass immediately after capture, before retarget:** standard mocap
  foot-locking + foot-strike marker pass, especially on Sprint/Stop/Turn — these feed
  Motion Matching directly (§4) and are the clips most sensitive to foot sliding once
  pose-search starts blending between them.
- **Multiple takes by default for Batting** (§2/§3 P2) — schedule 3–5 takes per shot
  in the plan from the start rather than treating it as an afterthought re-shoot; it's
  far cheaper to capture variety in the same session than to bring an actor back.

---

## 8. Integration checklist (back to the existing C++ contract)

Whoever builds the first montage from an imported clip needs exactly these notify
placements — names and meaning are already fixed by `CricketAnimationTypes.h`
(`ECricketAnimNotify`) and exercised by the `CricketSim.Anim` suite
(`ANIMATION_SYSTEM.md` §6):

| Clip family | Place this notify | At |
|---|---|---|
| `AM_Bowl_*_Full` | `BallRelease` | The captured release frame — drives `UCricketBowlingComponent::BowlNow()`. |
| `AM_Bat_*` | `BatImpact` | The captured bat-ball contact frame. |
| `AM_Field_Catch_*` | `CatchAttempt` | Hands-close frame. |
| `AM_Field_Pickup_*` | `PickupContact` | Hand-reaches-ball frame. |
| `AM_Field_Throw_*` | `ThrowRelease` | Ball-leaves-hand frame. |

None of these are new — they're the same five notifies `ACricketBowlingRig` already
forwards to gameplay (`ANIMATION_SYSTEM.md` §4). The mocap library's only job is to
give each one a real, physically-grounded frame to fire at instead of the
debug-driven timing the project shipped with.

**Recommended sequencing once capture starts landing:**
1. Import + retarget the P0 set (§3) onto `SK_Cricket_Skeleton`.
2. Build `AM_Bowl_Fast_Full` and `AM_Bat_StraightDrive_Front` first; place their
   notifies; swap them into the existing `ACricketBowlingRig`/batting rig and confirm
   the live Input→Animation→Physics loop (`ANIMATION_SYSTEM.md` §7 "Trying it in the
   editor") still fires `BallRelease`/`BatImpact` at the right instant with real mocap
   instead of the placeholder timing.
3. Only then author `PS_Schema_Locomotion` / `PSD_Locomotion` and wire the parent
   AnimBP's Locomotion state to a Motion Matching node instead of a blendspace — this
   order means the protected, physics-critical actions are validated against real
   capture before any Motion Matching infrastructure is layered on top of them.
