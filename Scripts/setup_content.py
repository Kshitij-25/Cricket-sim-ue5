"""
CricketSim — automated content setup script.

Run via Scripts/setup_content.sh (which invokes UnrealEditor with -ExecutePythonScript).
Creates:
  - /Game/Maps/L_Nets   : interactive nets level (startup map, human vs auto-fed AI)
  - /Game/Maps/L_Match  : auto-playing T20 match level (scoreboard visible)
  - /Game/Data/Balls/DA_Ball_RedKookaburra  : default ball profile
  - /Game/Data/Teams/DA_Team_India          : India XI
  - /Game/Data/Teams/DA_Team_Australia      : Australia XI

Requires:
  - PythonScriptPlugin + EditorScriptingUtilities enabled in .uproject (done).
  - UnrealEditor (full editor), not UnrealEditor-Cmd.
"""

import unreal

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def log(msg):
    unreal.log(f"[setup_content] {msg}")

def warn(msg):
    unreal.log_warning(f"[setup_content] {msg}")


def ensure_dir(virtual_path):
    if not unreal.EditorAssetLibrary.does_directory_exist(virtual_path):
        unreal.EditorAssetLibrary.make_directory(virtual_path)
        log(f"Created directory: {virtual_path}")


def spawn(actor_class, location=(0, 0, 0), rotation=(0, 0, 0)):
    loc = unreal.Vector(*location)
    rot = unreal.Rotator(*rotation)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, loc, rot)
    if actor is None:
        warn(f"spawn_actor_from_class returned None for {actor_class}")
    return actor


def load_class(script_path):
    cls = unreal.load_class(None, script_path)
    if cls is None:
        warn(f"load_class returned None for: {script_path}")
    return cls


def save_level():
    ok = unreal.EditorLevelLibrary.save_current_level()
    if not ok:
        warn("save_current_level() returned False")
    return ok

# ---------------------------------------------------------------------------
# Lighting helpers (shared between both levels)
# ---------------------------------------------------------------------------

def add_lighting():
    """Add DirectionalLight + SkyAtmosphere + SkyLight."""
    dir_light = spawn(unreal.DirectionalLight, (0, 0, 10000), (-45, 0, 0))
    if dir_light:
        comp = dir_light.get_component_by_class(unreal.DirectionalLightComponent)
        if comp:
            comp.set_editor_property("intensity", 10.0)
            comp.set_editor_property("light_color", unreal.Color(255, 247, 217))
            comp.set_editor_property("use_temperature", True)
            comp.set_editor_property("temperature", 5500.0)
        log("DirectionalLight placed")

    sky_atm = spawn(unreal.SkyAtmosphere, (0, 0, 0))
    if sky_atm:
        log("SkyAtmosphere placed")

    sky_light = spawn(unreal.SkyLight, (0, 0, 0))
    if sky_light:
        sl_comp = sky_light.get_component_by_class(unreal.SkyLightComponent)
        if sl_comp:
            sl_comp.set_editor_property("real_time_capture", True)
        log("SkyLight placed (real-time capture)")

    fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0))
    if fog:
        log("ExponentialHeightFog placed")


def add_floor():
    """Place a scaled 100m x 100m plane as ground reference."""
    floor = spawn(unreal.StaticMeshActor, (0, 0, 0))
    if floor is None:
        return
    plane = unreal.load_object(None, "/Engine/BasicShapes/Plane.Plane")
    if plane:
        mesh_comp = floor.static_mesh_component
        mesh_comp.set_static_mesh(plane)
        # Plane is 1m x 1m at scale 1; scale to 100m x 100m (10000 UU)
        floor.set_actor_scale3d(unreal.Vector(100.0, 100.0, 1.0))
        log("Floor plane placed (100m x 100m)")
    else:
        warn("Could not load /Engine/BasicShapes/Plane — floor skipped")


# ---------------------------------------------------------------------------
# L_Nets — interactive nets level (the startup map)
# ---------------------------------------------------------------------------

def create_l_nets():
    log("=== Creating L_Nets ===")

    ok = unreal.EditorLevelLibrary.new_level("/Game/Maps/L_Nets")
    if not ok:
        warn("new_level('/Game/Maps/L_Nets') returned False — level may already exist; loading it")
        unreal.EditorLevelLibrary.load_level("/Game/Maps/L_Nets")

    add_lighting()
    add_floor()

    # PlayerStart (suppresses engine warning; pawn self-possesses regardless)
    player_start = spawn(unreal.PlayerStart, (500, 0, 100))
    if player_start:
        log("PlayerStart placed")

    # CricketPlayerPawn — the whole playable slice in one actor
    pawn_class = load_class("/Script/CricketGameplay.CricketPlayerPawn")
    if pawn_class:
        pawn = spawn(pawn_class, (0, 0, 90))
        if pawn:
            log("CricketPlayerPawn placed at (0, 0, 90)")
    else:
        warn("CricketPlayerPawn class not found — check module is loaded")

    # CricketStadium — boundary/fielding/atmosphere (debug-drawn, no meshes)
    stadium_class = load_class("/Script/CricketGameplay.CricketStadium")
    if stadium_class:
        stadium = spawn(stadium_class, (0, 0, 0))
        if stadium:
            stadium.set_editor_property("straight_boundary_m", 75.0)
            stadium.set_editor_property("square_boundary_m", 68.0)
            log("CricketStadium placed (boundary 75/68 m)")

    save_level()
    log("=== L_Nets saved ===")


# ---------------------------------------------------------------------------
# L_Match — auto-playing T20 level (scoreboard visible)
# ---------------------------------------------------------------------------

def create_l_match():
    log("=== Creating L_Match ===")

    ok = unreal.EditorLevelLibrary.new_level("/Game/Maps/L_Match")
    if not ok:
        warn("new_level('/Game/Maps/L_Match') returned False — loading existing")
        unreal.EditorLevelLibrary.load_level("/Game/Maps/L_Match")

    add_lighting()
    add_floor()

    # CricketMatchRunner — self-contained India vs Australia T20, auto-play on
    runner_class = load_class("/Script/CricketSim.CricketMatchRunner")
    if runner_class:
        runner = spawn(runner_class, (0, 0, 90))
        if runner:
            runner.set_editor_property("overs_per_innings", 20)
            runner.set_editor_property("ball_interval", 0.4)
            runner.set_editor_property("seed", 12345)
            log("CricketMatchRunner placed (T20, seed=12345, auto-play)")
    else:
        warn("CricketMatchRunner class not found — check CricketSim module is loaded")

    save_level()
    log("=== L_Match saved ===")


# ---------------------------------------------------------------------------
# Data Assets
# ---------------------------------------------------------------------------

def create_ball_profile():
    ensure_dir("/Game/Data/Balls")
    asset_path = "/Game/Data/Balls/DA_Ball_RedKookaburra"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        log("DA_Ball_RedKookaburra already exists — skipping")
        return

    ball_class = load_class("/Script/CricketPhysics.CricketBallProfileAsset")
    if ball_class is None:
        warn("CricketBallProfileAsset class not found — skipping ball profile")
        return

    at = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", ball_class)
    asset = at.create_asset("DA_Ball_RedKookaburra", "/Game/Data/Balls", None, factory)

    if asset is None:
        warn("Failed to create DA_Ball_RedKookaburra")
        return

    asset.set_editor_property("profile_name", "Red Kookaburra (New)")

    unreal.EditorAssetLibrary.save_asset(asset_path)
    log("Created DA_Ball_RedKookaburra")


def _make_player(name, role_str, batting=0.5, bowling=0.5, fielding=0.5, pace=0.0):
    p = unreal.CricketPlayer()
    p.set_editor_property("name", name)
    role_map = {
        "BatterTop":    unreal.CricketRole.BATTER_TOP,
        "BatterMiddle": unreal.CricketRole.BATTER_MIDDLE,
        "AllRounder":   unreal.CricketRole.ALL_ROUNDER,
        "PaceBowler":   unreal.CricketRole.PACE_BOWLER,
        "SpinBowler":   unreal.CricketRole.SPIN_BOWLER,
        "WicketKeeper": unreal.CricketRole.WICKET_KEEPER,
    }
    p.set_editor_property("role", role_map.get(role_str, unreal.CricketRole.BATTER_MIDDLE))
    p.set_editor_property("batting", batting)
    p.set_editor_property("bowling", bowling)
    p.set_editor_property("fielding", fielding)
    p.set_editor_property("pace_kmh", pace)
    return p


def create_team(asset_name, folder, team_name, short_code, players_data):
    ensure_dir(folder)
    asset_path = f"{folder}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        log(f"{asset_name} already exists — skipping")
        return

    team_class = load_class("/Script/CricketSim.CricketTeamDataAsset")
    if team_class is None:
        warn("CricketTeamDataAsset class not found — skipping team data")
        return

    at = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", team_class)
    asset = at.create_asset(asset_name, folder, None, factory)

    if asset is None:
        warn(f"Failed to create {asset_name}")
        return

    asset.set_editor_property("team_name", team_name)
    asset.set_editor_property("short_code", short_code)

    players = []
    for pd in players_data:
        try:
            players.append(_make_player(*pd))
        except Exception as e:
            warn(f"Player struct creation failed ({pd[0]}): {e}")

    if players:
        asset.set_editor_property("players", players)

    unreal.EditorAssetLibrary.save_asset(asset_path)
    log(f"Created {asset_name} ({team_name}, {len(players)} players)")


INDIA_XI = [
    # (name, role, batting, bowling, fielding, paceKmh)
    ("Rohit Sharma",      "BatterTop",    0.85, 0.10, 0.70, 0.0),
    ("Shubman Gill",      "BatterTop",    0.78, 0.05, 0.75, 0.0),
    ("Virat Kohli",       "BatterTop",    0.90, 0.10, 0.80, 0.0),
    ("Suryakumar Yadav",  "BatterMiddle", 0.82, 0.05, 0.70, 0.0),
    ("Hardik Pandya",     "AllRounder",   0.72, 0.68, 0.78, 140.0),
    ("Ravindra Jadeja",   "AllRounder",   0.68, 0.75, 0.88, 0.0),
    ("Rishabh Pant",      "WicketKeeper", 0.78, 0.05, 0.80, 0.0),
    ("Axar Patel",        "SpinBowler",   0.58, 0.72, 0.72, 0.0),
    ("Jasprit Bumrah",    "PaceBowler",   0.20, 0.92, 0.70, 142.0),
    ("Arshdeep Singh",    "PaceBowler",   0.18, 0.78, 0.65, 138.0),
    ("Yuzvendra Chahal",  "SpinBowler",   0.18, 0.80, 0.65, 0.0),
]

AUSTRALIA_XI = [
    ("Travis Head",       "BatterTop",    0.80, 0.12, 0.75, 0.0),
    ("David Warner",      "BatterTop",    0.82, 0.08, 0.72, 0.0),
    ("Mitchell Marsh",    "AllRounder",   0.70, 0.68, 0.75, 138.0),
    ("Steve Smith",       "BatterMiddle", 0.88, 0.18, 0.78, 0.0),
    ("Glenn Maxwell",     "AllRounder",   0.75, 0.65, 0.80, 0.0),
    ("Marcus Stoinis",    "AllRounder",   0.68, 0.62, 0.72, 135.0),
    ("Matthew Wade",      "WicketKeeper", 0.65, 0.05, 0.75, 0.0),
    ("Pat Cummins",       "PaceBowler",   0.38, 0.88, 0.78, 145.0),
    ("Mitchell Starc",    "PaceBowler",   0.28, 0.85, 0.72, 148.0),
    ("Josh Hazlewood",    "PaceBowler",   0.20, 0.82, 0.75, 140.0),
    ("Adam Zampa",        "SpinBowler",   0.22, 0.80, 0.68, 0.0),
]


def create_team_assets():
    create_team(
        "DA_Team_India", "/Game/Data/Teams",
        "India", "IND", INDIA_XI
    )
    create_team(
        "DA_Team_Australia", "/Game/Data/Teams",
        "Australia", "AUS", AUSTRALIA_XI
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    log("======== CricketSim content setup started ========")

    with unreal.ScopedEditorTransaction("CricketSim Content Setup") as trans:
        # Levels (order matters: create L_Nets first so it's open last = startup map)
        create_l_match()
        create_l_nets()  # left open as the active level after setup

        # Data assets
        create_ball_profile()
        create_team_assets()

    log("======== CricketSim content setup complete ========")
    log("Next steps:")
    log("  1. Verify L_Nets in PIE: batting/bowling/physics HUD panels appear")
    log("  2. Verify L_Match in PIE: scoreboard populates, T20 plays out")
    log("  3. Run Scripts/package_mac.sh Shipping")


main()
