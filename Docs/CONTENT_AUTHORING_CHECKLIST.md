# CricketSim — Content-Authoring Checklist (Unblock Packaging)

_Created 2026-06-18. Resolves `KNOWN_ISSUES.md` **B1** (no cookable content / startup map)._

The code, all 10 runtime modules, and the Shipping link step are verified
(`RELEASE_CANDIDATE.md`). `Content/` is **empty** and the configured startup map
`/Game/Maps/L_Nets` does not exist, so `Scripts/package_mac.sh` fails at cook.
This document is the in-editor work to fix that — **optimised for under 4 hours**.

---

## 0. The one fact that makes this fast

This project is architected **"press Play, no assets."** Verified against the C++:

- **HUD** — `ACricketHUDGameMode` sets `HUDClass = ACricketHUD` in C++; `ACricketHUD`
  self-defaults `LayoutClass` to `UCricketHUDLayout::StaticClass()`; the layout
  self-defaults its 4 panel classes. **No `meta=(BindWidget)` anywhere** → the UMG
  HUD renders fully with **zero Widget Blueprints**.
- **Input** — `UCricketPlayerInputComponent::BuildInput()` `NewObject`-constructs all
  5 mapping contexts + ~36 input actions and binds keys in C++. The UPROPERTYs are
  bare `UPROPERTY()` (not `EditAnywhere`) → **no IMC/IA assets can or should be authored.**
- **AI / balance** — `FCricketAIPlayerProfile::Derive()` builds personalities from
  roster ratings; `FCricketBalanceConfig` defaults to a neutral identity → **no assets.**
- **Physics profiles** — Ball/Bat/Pitch/BowlingAction all carry valid C++ struct
  defaults / static factories ("runs with zero authored assets").
- **Match** — `ACricketMatchRunner` and `ACricketPlayerPawn` are **self-possessing
  pawns** (`AutoPossessPlayer = Player0`); the runner hardcodes the India/Australia
  squads in C++. Drop one in a level and press Play.

**Consequence: the only *hard* gate to a packageable, runnable build is a single
authored level containing one gameplay pawn.** Everything else below is either
already done in C++ or is content-completeness / polish you do with the remaining time.

### Critical path (≈30–45 min → packageable build)

1. Create level, **Save As** `/Game/Maps/L_Nets`.
2. Add a Directional Light + Sky Atmosphere + SkyLight + a Floor plane (so it isn't black).
3. Drag **one `CricketPlayerPawn`** into the level near origin. Add an `APlayerStart`.
4. Save. Run `Scripts/package_mac.sh Shipping`. Done.

Steps 5+ (data assets, visible ball, second level) raise quality but are **not**
required for the cook to succeed. The time budget below front-loads the critical
path, then spends the rest on completeness.

---

## 1. Time budget (target: < 4 hours)

| Block | Task | Req? | Est |
|---|---|---|---|
| **A. Unblock** | §4 Create & light `L_Nets`; place pawn + PlayerStart | ✅ | 45 min |
| **B. First cook** | §9 Run pre-package gate + first `package_mac.sh`; confirm artifact | ✅ | 30 min |
| **C. Scan hygiene** | §7 `/Game/Data/Balls` ×1, `/Game/Data/Teams` ×2 (kills AssetManager warnings) | ⭐ | 45 min |
| **D. Visible polish** | §6 `BP_CricketBall` w/ sphere mesh; ball/bat/pitch variety assets | ◻ | 45 min |
| **E. Second level** | §4 `L_Match` (auto-playing `CricketMatchRunner` demo) | ◻ | 20 min |
| **F. Re-cook + QA** | §9 Re-package, run `QA_CHECKLIST.md` §7 manual-play items | ✅ | 30 min |
| | | **Total** | **~3h35m** |

✅ required · ⭐ strongly recommended (removes cook warnings) · ◻ optional polish.
If time is tight, **A + B + F alone (≈1h45m) yields a shippable slice.**

---

## 2. Required Project Settings  (Edit ▸ Project Settings — mostly *verify*)

Everything here is **already set** in `Config/`. Confirm, don't re-enter.

- [ ] **Maps & Modes ▸ Default GameMode** = `CricketHUDGameMode`
      (`DefaultEngine.ini` → `GlobalDefaultGameMode=/Script/CricketUI.CricketHUDGameMode`). ✅ set.
      → C++ class used directly; **no BP GameMode required.**
- [ ] **Maps & Modes ▸ Editor Startup Map** & **Game Default Map** = `L_Nets`. ✅ set
      (will resolve once §4 creates the map).
- [ ] **Packaging ▸ Build Configuration** = `Shipping`; **Use Pak File** ✅; **Use Io Store** ✅.
      ✅ set in `DefaultGame.ini`.
- [ ] **Packaging ▸ List of maps to include in a packaged build** contains `/Game/Maps/L_Nets`.
      ✅ set (`+MapsToCook`). Add a line per extra level you author (e.g. `L_Match`).
- [ ] **Asset Manager** scans `TeamData → /Game/Data/Teams` and `BallProfile → /Game/Data/Balls`.
      ✅ set. (Scanning an empty/missing dir is a **warning, not a cook error** — §7 populates them.)
- [ ] **Game ▸ Cricket Physics** (`UCricketPhysicsSettings`, a `UDeveloperSettings`) — **no asset.**
      Verify `bEnableDebugByDefault=false`, `bEnablePitchDebug=false` (shipping baseline). ✅ default.
- [ ] **Game ▸ Cricket Performance** (`UCricketPerformanceSettings`) — verify `bShowDashboardByDefault=false`. ✅ default.
- [ ] **Plugins** — confirm **CommonUI** = enabled (✅ in `.uproject`; HUD widgets derive from
      `UCommonUserWidget`). **EnhancedInput**: a code dependency (`*.Build.cs`) but **not** listed
      in `.uproject`. It is enabled-by-default in UE 5.7 so cook succeeds; **recommended** to add
      `{ "Name": "EnhancedInput", "Enabled": true }` to the Plugins array to make it explicit.

> No code changes are needed. The only `.uproject` edit worth making is the explicit
> EnhancedInput entry (1 line, optional).

---

## 3. Required startup map configuration

Already wired in `Config/DefaultEngine.ini` / `DefaultGame.ini` — **no action beyond §4**:

- `GameDefaultMap = /Game/Maps/L_Nets.L_Nets`  ✅
- `EditorStartupMap = /Game/Maps/L_Nets.L_Nets`  ✅
- `+MapsToCook = (FilePath="/Game/Maps/L_Nets")`  ✅

The path is exact: the level **must** be saved as `L_Nets` in `/Game/Maps/`. Saving it
anywhere else (or misnaming it) leaves B1 unresolved and the cook fails.

---

## 4. Level creation checklist

### `L_Nets` — interactive nets (REQUIRED — the startup map)

- [ ] **File ▸ New Level ▸ Empty Level** (not "Open World"; this is a tiny slice).
- [ ] **Save As** → `/Game/Maps/` → name exactly **`L_Nets`**.
- [ ] Lighting (so the viewport isn't black — cosmetic but expected for a demo):
  - [ ] `Directional Light` (Mobility: Movable or Stationary).
  - [ ] `Sky Atmosphere`.
  - [ ] `Sky Light` (Mobility: Movable; Real-Time Capture on).
  - [ ] *(optional)* `Exponential Height Fog`.
- [ ] **Floor**: add a `Plane` (or `Cube` scaled flat) at Z=0, scale ~`50×50`, for ground reference.
      Assign any engine material. *(Cosmetic — sim does not need it.)*
- [ ] **Place one `CricketPlayerPawn`** (Place Actors ▸ search "Cricket") near origin,
      e.g. location `(0, 0, 90)`. See §5 for why this is the whole gameplay setup.
- [ ] **Place one `APlayerStart`** off to the side (suppresses the "no PlayerStart" warning;
      the pawn self-possesses regardless).
- [ ] *(Recommended)* Place one `CricketStadium` at origin for boundary/venue context (§5).
- [ ] **World Settings**: leave default (§8).
- [ ] **Build ▸ Build Lighting** (or rely on dynamic) and **Save**.

> **Do not** also place a `CricketMatchRunner` or a second batting pawn here — every
> Cricket pawn sets `AutoPossessPlayer = Player0`, so two would fight over the player.

### `L_Match` — auto-playing T20 demo (OPTIONAL)

- [ ] New Empty Level → **Save As** `/Game/Maps/L_Match`.
- [ ] Add the same lighting set.
- [ ] **Place one `CricketMatchRunner`** (only this actor — it's fully self-contained,
      runs India vs Australia ball-by-ball, `OversPerInnings=20`, `Seed=12345`).
- [ ] Add `+MapsToCook=(FilePath="/Game/Maps/L_Match")` to `DefaultGame.ini`.
- [ ] *(Note)* The runner's on-screen debug scoreboard is **compiled out of Shipping**;
      in a Shipping package the player-facing `CricketUI` HUD shows the score instead
      (it auto-resolves the runner as its data source).

---

## 5. Required actors (placement — all are C++ classes, no Blueprint needed)

| Actor | Place in | Req? | Why / behaviour |
|---|---|---|---|
| `CricketPlayerPawn` | `L_Nets` (×1) | ✅ | Self-possesses Player0; spawns its own `CricketBall` + auto-feeder (`FeedInterval=3.5s`, `PitchLengthCm=2000`); builds all Enhanced Input in C++; drives camera/replay. **This single actor is the playable slice.** |
| `APlayerStart` | `L_Nets` (×1) | ⭐ | Clean spawn transform + no warning. Not load-bearing. |
| `CricketStadium` | `L_Nets` (×1) | ◻ | Boundaries (four/six), field positions, atmosphere → ball physics. Pure debug-draw (cvar `cricket.Debug.Stadium`, default 0); **no meshes/materials**. Defaults sane (`StraightBoundaryM=75`, `SquareBoundaryM=68`). |
| `CricketMatchRunner` | `L_Match` (×1) | ◻ | Self-contained auto-playing T20. **Never co-place with a batting pawn.** |

Alternative to `CricketPlayerPawn`: `CricketBattingRig` (polled-keyboard equivalent,
also self-possessing, also asset-free). Pick exactly one.

---

## 6. Required Blueprints

**None are required.** Every class is usable directly from C++. List below is **optional polish**:

- [ ] ◻ **`BP_CricketBall`** (`/Game/Art/Ball`, parent `CricketBall`) — the only *visible*
      gap. `CricketBall`'s `UStaticMeshComponent` has **no mesh assigned in C++**, so the
      ball is invisible (physics still works). To see it:
  - Parent a Blueprint to `CricketBall`; assign **`/Engine/BasicShapes/Sphere`** (it is
    100 cm, matching the C++ scale assumption) + a red material.
  - Set the pawn's **`BallClass`** to `BP_CricketBall` (on the placed `CricketPlayerPawn`).
- [ ] ◻ **`BP_CricketHUDGameMode`** (`/Game/Core`, parent `CricketHUDGameMode`) — only if
      you prefer to set `DefaultPawnClass = CricketPlayerPawn` via the GameMode instead of
      placing the pawn in the level. If you make it, repoint `GlobalDefaultGameMode` to
      `/Game/Core/BP_CricketHUDGameMode.BP_CricketHUDGameMode_C`. (Placing the pawn is simpler.)
- [ ] ◻ **`WBP_*` HUD reskins** (`/Game/UI/HUD`) — only to restyle the HUD. Reparent an
      empty Widget Blueprint to `CricketHUDLayout` / `CricketScoreboardWidget` / etc. and leave
      the design tree empty (C++ `RebuildWidget` builds it). No `BindWidget` to satisfy.

---

## 7. Required Data Assets

**Zero are required for the build to run.** Two are **strongly recommended** to make the
configured Asset Manager scans non-empty (removes cook warnings); the rest are optional
tuning content. Create via **Content Browser ▸ Add ▸ Miscellaneous ▸ Data Asset ▸ pick class.**

### ⭐ Recommended (satisfy Asset Manager scans)

- [ ] ⭐ **`DA_Ball_RedKookaburra`** → `/Game/Data/Balls` — class **`CricketBallProfileAsset`**.
      Scanned as `BallProfile`. **Keep the struct defaults** — they are physically valid.
      Suggested values:
  - `ProfileName = "Red Kookaburra (New)"`, `DefaultArchetype = SeamUp`
  - `Coefficients`: `BaseDragCoefficient=0.45`, `SupercriticalDragCoefficient=0.24`,
    `MaxSwingSideForceCoefficient=0.30`, `OptimalSeamAngleRad=0.349`, `MagnusLiftSlope=0.5`,
    `MaxMagnusLiftCoefficient=0.45`, `SpinDecayRate=0.1`, `SwingTransitionSpeed=30.0`
  - `InitialSurface`: `ShineAsymmetry=1.0`, `Roughness=0.0`, `SeamProudness=1.0`
  - ⚠️ **Do not zero** `SwingTransitionSpeed` (→ forces reverse-swing regime) or
    `MaxSwingSideForceCoefficient`/`MagnusLiftSlope` (→ dead, straight ball).
- [ ] ⭐ **`DA_Team_India`** → `/Game/Data/Teams` — class **`CricketTeamDataAsset`**.
      `TeamName="India"`, `ShortCode="IND"`, `Players` = 11 (mirror the C++ XI: Rohit, Gill,
      Kohli, Suryakumar, Pandya, Jadeja, Pant, Axar, Bumrah, Arshdeep, Chahal). Set `Role`
      (Pant=WicketKeeper; Bumrah/Arshdeep=PaceBowler; Chahal/Axar/Jadeja=SpinBowler;
      Pandya=AllRounder) and `PaceKmh≈140` for quicks. Ratings can stay `0.5`.
- [ ] ⭐ **`DA_Team_Australia`** → `/Game/Data/Teams` — `TeamName="Australia"`, `ShortCode="AUS"`,
      11 players (Head, Warner, Marsh, Smith, Maxwell, Stoinis, Wade, Cummins, Starc,
      Hazlewood, Zampa; Wade=WicketKeeper, Cummins/Starc/Hazlewood=PaceBowler, Zampa=SpinBowler,
      Maxwell/Stoinis=AllRounder).

> The runtime match hardcodes its squads, so these TeamData assets are content-completeness,
> not load-bearing — but they make the registered scan meaningful and silence warnings.

### ◻ Optional tuning content (not scanned, not loaded by path)

- [ ] ◻ `DA_Bat_Standard` → `/Game/Data/Bats` (`CricketBatProfileAsset`). Keep defaults;
      `EdgeCurvature≈1.1` gives realistic edge squirt. Don't zero `EffectiveMassSweetSpotKg`,
      `RestitutionSweetSpot`, or `QualityFalloffM`.
- [ ] ◻ `DA_Pitch_Balanced` (+ `_Hard`/`_Dry`/`_Green`) → `/Game/Data/Pitches`
      (`CricketPitchProfileAsset`). Set `PitchType` then run **Configure From Type** —
      don't hand-fill `BasePatch`. Don't zero `Hardness`/`Restitution`.
- [ ] ◻ `DA_BowlAction_ExpressQuick` (+ Swing/OffSpin/LegSpin) → `/Game/Data/BowlingActions`
      (`CricketBowlingActionAsset`). Constructor already = `MakeExpressQuick()`.
- [ ] ◻ `DA_AudioBank_Default` → `/Game/Data/Audio` (`CricketAudioBankAsset`). **Skip** unless
      you also import `SoundWave`/`SoundCue` assets — an empty bank just plays silence, which is fine.
- [ ] ◻ `DA_AIProfile_*` (`CricketAIProfileDataAsset`) — skip; AI derives from rosters.

> If you want the optional Bat/Pitch/BowlingAction assets discoverable at runtime, add
> matching `+PrimaryAssetTypesToScan` lines to `DefaultGame.ini`. Otherwise they remain
> editor-only references — fine for the slice. None of these reach the integrator unless a
> level/Blueprint explicitly calls `ApplyProfile`/`SetActionAsset`, so they are pure tuning.

---

## 8. Required World Settings (per level)

**Leave default.** Confirmed sufficient:

- [ ] **GameMode Override** = `None` → inherits `CricketHUDGameMode` from project settings. ✅
- [ ] **KillZ** — engine default is fine for a flat nets level. Optionally set `-1000` if
      actors could fall out of the world. ◻
- [ ] Physics (substepping, `DefaultGravityZ=-981`) is **global** in `DefaultEngine.ini` —
      no per-level physics settings. ✅
- [ ] No NavMesh / NavMeshBoundsVolume needed (fielders/rigs move via components, not navigation). ✅

---

## 9. Packaging validation checklist

### Pre-package gate (run first — `QA_CHECKLIST.md` §0)

- [ ] `Scripts/run_tests.sh` → **122/122**, exit 0.
- [ ] `Scripts/build.sh CricketSimEditor Development` → compiles.
- [ ] `Scripts/build.sh CricketSim Shipping` → compiles **and links** (proves the shipping codepath).

### In-editor pre-flight

- [ ] PIE in `L_Nets`: HUD scoreboard appears; ball is auto-fed (visible if §6 done);
      input responds (D/W footwork, Space plays the shot — see Appendix).
- [ ] No load errors / missing-asset warnings in the Output Log.
- [ ] `/Game/Maps/L_Nets` is saved and listed under Packaging ▸ Maps to include.

### Cook + package

- [ ] Run `Scripts/package_mac.sh Shipping` (cooks the `CricketSim` Game target, Mac, from
      the `MapsToCook` list — no `-map` override).
- [ ] **Success =** RunUAT exits 0; `BUILD SUCCESSFUL`; artifact under `Build/Mac/`.
- [ ] Inspect the cook log (results land in `~/Library/Logs/...`, **not** stdout): `L_Nets`
      cooked; **no** "Could not find object for asset" / missing-reference **errors**.
- [ ] Launch the packaged app → it loads into `L_Nets`, the HUD shows, and a match plays
      to completion. Confirm `Saved/Analytics/Matches.csv` gets a row (telemetry — `QA_CHECKLIST.md` §7).

### Common cook failures (empty-content start)

| Symptom | Cause | Fix |
|---|---|---|
| `Could not find object for asset /Game/Maps/L_Nets` | Map missing/misnamed (B1) | §4 — save exactly as `/Game/Maps/L_Nets` |
| Packaged app boots to a black screen, no cricket | No pawn placed → plain `ADefaultPawn` | §5 — place one `CricketPlayerPawn` |
| Black viewport but HUD works | No lights | §4 — add the lighting set |
| Ball invisible | `CricketBall` has no mesh | §6 — `BP_CricketBall` + engine sphere (optional) |
| "Asset Manager found 0 primary assets in /Game/Data/Teams" | Empty scan dir | **Warning only** — §7 to silence |
| Input does nothing in package | EnhancedInput plugin inactive | §2 — add explicit `.uproject` plugin entry |

---

## 10. Appendix

### A. Do **NOT** author these (would be orphaned/unused)

- Any `IMC_*` / `IA_*` Input assets — all built in C++; the UPROPERTYs aren't editor-exposed.
- Any GameMode/PlayerController/HUD Blueprint as a *requirement* — C++ classes are wired directly.
- Any WBP as a *requirement* — no `BindWidget`; C++ widgets are complete.

### B. `CricketPlayerPawn` control reference (for PIE/QA — Enhanced Input, batting layer)

```
Footwork:  D front foot · W back foot · S defensive · LeftShift lofted (hold)
Direction: ↑ straight · → off · ← leg · ↓ fine · E cover · Q midwicket
Play shot: Space / LMB (timing)
Shared:    C cycle cam · B ball-follow · F free cam · V replay last delivery
Replay:    P pause · [ slower · ] faster · , step back · . step fwd
```

### C. `CricketMatchRunner` controls (in `L_Match`, Development build)

```
Space bowl next · Enter toggle auto-play · ↑/↓ auto speed · R restart (re-toss)
```

---

### Sign-off

When §4–§5 + §9 are green, **B1 is resolved** and the vertical slice is packageable
and runnable. Update `KNOWN_ISSUES.md` B1 → resolved, bump `ProjectVersion`, and tag
per `RELEASE_CANDIDATE.md`.
