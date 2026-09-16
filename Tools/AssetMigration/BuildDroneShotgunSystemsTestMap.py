"""Create or validate the isolated Shotgun NPC systems test map and Pellet Blueprint.

The existing Smart Object greybox keeps its original Rifle/Shotgun/Friendly
population.  This script owns only actors tagged DroneShotgunSystemsTest.Owned
inside `/Game/Drone/Maps/TestMap/Lvl_DroneShotgunSystemsTest`.

Set DRONE_SHOTGUN_TESTMAP_REBUILD=1 to rebuild the owned actors explicitly.
The production Training map is never opened or saved.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_SHOTGUN_TESTMAP"
MAP_FOLDER = "/Game/Drone/Maps/TestMap"
MAP_PATH = f"{MAP_FOLDER}/Lvl_DroneShotgunSystemsTest"
TRAINING_MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"
GAME_MODE_BLUEPRINT_PATH = "/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode"
SHOTGUN_NPC_BLUEPRINT_PATH = "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Shotgun"
PELLET_BLUEPRINT_FOLDER = "/Game/Drone/AI/Blueprints/Projectiles"
PELLET_BLUEPRINT_NAME = "BP_ShotgunPelletProjectile"
PELLET_BLUEPRINT_PATH = f"{PELLET_BLUEPRINT_FOLDER}/{PELLET_BLUEPRINT_NAME}"
PELLET_PARENT_CLASS_PATH = "/Script/Drone.DroneNPCProjectile"
CUBE_PATH = "/Engine/BasicShapes/Cube"

OWNED_TAG = unreal.Name("DroneShotgunSystemsTest.Owned")
SHOOTER_TAG = unreal.Name("DroneShotgunSystemsTest.Shooter")

FLOOR_LABEL = "ShotgunSystemsTest_Floor"
NAVIGATION_FLOOR_LABEL = "ShotgunSystemsTest_NavigationFloor"
NAVIGATION_BOUNDS_LABEL = "ShotgunSystemsTest_NavMeshBounds"
PLAYER_START_LABEL = "ShotgunSystemsTest_PlayerStart"
SHOOTER_LABEL = "ShotgunSystemsTest_HostileShotgun"
KEY_LIGHT_LABEL = "ShotgunSystemsTest_KeyLight"
SKY_LIGHT_LABEL = "ShotgunSystemsTest_SkyLight"

MARKER_SPECS = (
    ("ShotgunSystemsTest_5mMarker", (350.0, -450.0, 5.0), (0.10, 1.25, 0.10)),
    ("ShotgunSystemsTest_10mMarker", (850.0, -450.0, 5.0), (0.10, 1.25, 0.10)),
    ("ShotgunSystemsTest_15mMarker", (1350.0, -450.0, 5.0), (0.10, 1.25, 0.10)),
)


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load_blueprint_class(path: str) -> unreal.Class:
    blueprint = unreal.EditorAssetLibrary.load_asset(path)
    require(isinstance(blueprint, unreal.Blueprint), f"Missing Blueprint: {path}")
    generated_class = blueprint.generated_class()
    require(generated_class is not None, f"Blueprint generated Class is unavailable: {path}")
    return generated_class


def load_native_class(path: str) -> unreal.Class:
    actor_class = unreal.load_class(None, path)
    require(actor_class is not None, f"Missing native Class: {path}")
    return actor_class


def compile_blueprint(blueprint: unreal.Blueprint) -> None:
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(
        blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
        f"Blueprint compile failed: {blueprint.get_path_name()}",
    )


def ensure_shotgun_pellet_assets(editor_assets: unreal.EditorAssetSubsystem) -> unreal.Class:
    parent_class = load_native_class(PELLET_PARENT_CLASS_PATH)
    unreal.EditorAssetLibrary.make_directory(PELLET_BLUEPRINT_FOLDER)
    pellet_blueprint = editor_assets.load_asset(PELLET_BLUEPRINT_PATH)
    created_pellet_blueprint = pellet_blueprint is None
    if pellet_blueprint is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        pellet_blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            PELLET_BLUEPRINT_NAME,
            PELLET_BLUEPRINT_FOLDER,
            unreal.Blueprint.static_class(),
            factory,
            overwrite_existing=False,
        )
        log(f"CREATED_PELLET_BLUEPRINT|{PELLET_BLUEPRINT_PATH}")
    require(isinstance(pellet_blueprint, unreal.Blueprint), f"Missing Pellet Blueprint: {PELLET_BLUEPRINT_PATH}")
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(pellet_blueprint) == parent_class,
        f"Pellet Blueprint parent mismatch: {PELLET_BLUEPRINT_PATH}",
    )
    compile_blueprint(pellet_blueprint)
    pellet_cdo = unreal.get_default_object(pellet_blueprint.generated_class())
    require(pellet_cdo is not None, "Pellet Blueprint CDO is unavailable")
    projectile_visual = pellet_cdo.get_projectile_visual()
    projectile_trail = pellet_cdo.get_projectile_trail_visual()
    require(projectile_visual is not None, "Pellet core visual component is unavailable")
    require(projectile_trail is not None, "Pellet tracer visual component is unavailable")
    if created_pellet_blueprint:
        require(editor_assets.save_loaded_asset(pellet_blueprint, only_if_is_dirty=False), "Could not save Pellet Blueprint")

    shotgun_blueprint = editor_assets.load_asset(SHOTGUN_NPC_BLUEPRINT_PATH)
    require(isinstance(shotgun_blueprint, unreal.Blueprint), f"Missing Shotgun Blueprint: {SHOTGUN_NPC_BLUEPRINT_PATH}")
    compile_blueprint(shotgun_blueprint)
    shotgun_cdo = unreal.get_default_object(shotgun_blueprint.generated_class())
    require(shotgun_cdo is not None, "Shotgun Blueprint CDO is unavailable")
    weapon = shotgun_cdo.get_npc_weapon_component()
    require(weapon is not None, "Shotgun Blueprint Weapon Component is unavailable")
    if created_pellet_blueprint:
        shotgun_blueprint.modify()
        shotgun_cdo.modify()
        weapon.modify()
        weapon.set_editor_property("shotgun_damage_per_pellet", 3.0)
        weapon.set_editor_property("projectile_class", pellet_blueprint.generated_class())
        compile_blueprint(shotgun_blueprint)
        require(editor_assets.save_loaded_asset(shotgun_blueprint, only_if_is_dirty=False), "Could not save Shotgun Blueprint")
        log("CONFIGURED_PELLET|count=8|damage_each=3|core_scale=0.025|tracer=1")
    else:
        require(abs(weapon.get_shotgun_damage_per_pellet() - 3.0) < 0.001, "Shotgun Pellet damage mismatch")
        require(weapon.get_projectile_class() == pellet_blueprint.generated_class(), "Shotgun does not use its Pellet Blueprint")
        log("VALIDATED_PELLET|count=8|damage_each=3|dedicated_class=1|tracer=1")
    return pellet_blueprint.generated_class()


def actor_by_label(actors: unreal.EditorActorSubsystem, label: str) -> unreal.Actor | None:
    matches = [actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == label]
    require(len(matches) <= 1, f"Duplicate actor label: {label}")
    return matches[0] if matches else None


def spawn_actor(
    actors: unreal.EditorActorSubsystem,
    actor_class: unreal.Class,
    label: str,
    location: tuple[float, float, float],
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
    tags: tuple[unreal.Name, ...] = (),
) -> unreal.Actor:
    actor = actors.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]),
    )
    require(actor is not None, f"Could not spawn actor: {label}")
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [OWNED_TAG, *tags])
    return actor


def configure_static_mesh(
    actor: unreal.StaticMeshActor,
    mesh: unreal.StaticMesh,
    scale: tuple[float, float, float],
    collision: unreal.CollisionEnabled,
) -> None:
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    require(component is not None and component.set_static_mesh(mesh), f"Could not configure mesh: {actor.get_actor_label()}")
    component.set_collision_enabled(collision)


def clear_owned_actors(actors: unreal.EditorActorSubsystem) -> None:
    for actor in list(actors.get_all_level_actors()):
        if OWNED_TAG in actor.get_editor_property("tags"):
            label = actor.get_actor_label()
            require(actors.destroy_actor(actor), f"Could not remove owned actor: {label}")
            log(f"REMOVED_OWNED_ACTOR|{label}")


def create_map(
    level_editor: unreal.LevelEditorSubsystem,
    actors: unreal.EditorActorSubsystem,
    rebuild: bool,
) -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem is unavailable")
    map_exists = editor_assets.does_asset_exist(MAP_PATH)

    if map_exists:
        require(level_editor.load_level(MAP_PATH), f"Could not load test map: {MAP_PATH}")
        require(rebuild, "Test map already exists; use validation mode or set DRONE_SHOTGUN_TESTMAP_REBUILD=1")
        clear_owned_actors(actors)
        log("REBUILDING_EXISTING_MAP")
    else:
        unreal.EditorAssetLibrary.make_directory(MAP_FOLDER)
        require(level_editor.new_level(MAP_PATH), f"Could not create test map: {MAP_PATH}")
        log(f"CREATED_MAP|{MAP_PATH}")

    cube = unreal.EditorAssetLibrary.load_asset(CUBE_PATH)
    require(isinstance(cube, unreal.StaticMesh), f"Missing Engine Cube: {CUBE_PATH}")

    floor = spawn_actor(actors, unreal.StaticMeshActor, FLOOR_LABEL, (650.0, 0.0, -35.0))
    configure_static_mesh(floor, cube, (32.0, 14.0, 0.20), unreal.CollisionEnabled.NO_COLLISION)

    navigation_floor = spawn_actor(
        actors,
        load_native_class("/Script/Drone.DroneNPCNavigationFloor"),
        NAVIGATION_FLOOR_LABEL,
        (650.0, 0.0, -50.0),
    )
    navigation_collision = navigation_floor.get_navigation_collision()
    require(navigation_collision is not None, "Navigation Floor collision is unavailable")
    navigation_collision.set_box_extent(unreal.Vector(1600.0, 700.0, 50.0), True)

    navigation_bounds = spawn_actor(
        actors,
        load_native_class("/Script/NavigationSystem.NavMeshBoundsVolume"),
        NAVIGATION_BOUNDS_LABEL,
        (650.0, 0.0, 200.0),
    )
    navigation_bounds.set_actor_scale3d(unreal.Vector(18.0, 10.0, 5.0))

    spawn_actor(actors, unreal.PlayerStart, PLAYER_START_LABEL, (0.0, 0.0, 300.0))
    shooter = spawn_actor(
        actors,
        load_blueprint_class(SHOTGUN_NPC_BLUEPRINT_PATH),
        SHOOTER_LABEL,
        (900.0, 0.0, 100.0),
        (0.0, 180.0, 0.0),
        (SHOOTER_TAG,),
    )
    # 역할 BP는 실제 Shotgun을 별도 Gun Component로 표시한다. 부모의 교체용
    # WeaponVisualComponent는 비어 있어 Map Check 경고를 내므로, 이 시험 맵
    # 인스턴스에서만 보이지 않는 Engine Mesh를 채운다. 실제 Gun 외형은 건드리지 않는다.
    for component in shooter.get_components_by_class(unreal.StaticMeshComponent):
        if component.get_name() == "WeaponVisualComponent" and component.get_editor_property("static_mesh") is None:
            require(component.set_static_mesh(cube), "Could not fill hidden WeaponVisual placeholder")
            component.set_visibility(False, True)
            component.set_hidden_in_game(True)
    spawn_actor(actors, unreal.DirectionalLight, KEY_LIGHT_LABEL, (0.0, 0.0, 1800.0), (-35.0, -25.0, 0.0))
    spawn_actor(actors, unreal.SkyLight, SKY_LIGHT_LABEL, (0.0, 0.0, 1400.0))

    for label, location, scale in MARKER_SPECS:
        marker = spawn_actor(actors, unreal.StaticMeshActor, label, location)
        configure_static_mesh(marker, cube, scale, unreal.CollisionEnabled.NO_COLLISION)

    # 중앙 사선은 비워 자동 PIE가 바로 발사할 수 있게 하고, 옆 벽은 수동 LOS 차단 시험에 쓴다.
    side_wall = spawn_actor(
        actors,
        unreal.StaticMeshActor,
        "ShotgunSystemsTest_LOSBlocker",
        (450.0, 500.0, 175.0),
    )
    configure_static_mesh(side_wall, cube, (0.50, 2.50, 3.50), unreal.CollisionEnabled.QUERY_AND_PHYSICS)

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable")
    game_mode_class = load_blueprint_class(GAME_MODE_BLUEPRINT_PATH)
    world.get_world_settings().set_editor_property("default_game_mode", game_mode_class)
    require(level_editor.save_current_level(), f"Could not save test map: {MAP_PATH}")
    log(f"SAVED_MAP|{MAP_PATH}")


def validate_map(level_editor: unreal.LevelEditorSubsystem, actors: unreal.EditorActorSubsystem) -> None:
    require(level_editor.load_level(MAP_PATH), f"Could not load test map: {MAP_PATH}")
    require(MAP_PATH != TRAINING_MAP_PATH, "Shotgun test map path must never resolve to Training")

    expected_labels = {
        FLOOR_LABEL,
        NAVIGATION_FLOOR_LABEL,
        NAVIGATION_BOUNDS_LABEL,
        PLAYER_START_LABEL,
        SHOOTER_LABEL,
        KEY_LIGHT_LABEL,
        SKY_LIGHT_LABEL,
        "ShotgunSystemsTest_LOSBlocker",
        *(spec[0] for spec in MARKER_SPECS),
    }
    for label in expected_labels:
        require(actor_by_label(actors, label) is not None, f"Missing test actor: {label}")

    shooter = actor_by_label(actors, SHOOTER_LABEL)
    require(shooter is not None, "Shotgun Shooter is missing")
    require(SHOOTER_TAG in shooter.get_editor_property("tags"), "Shotgun Shooter tag is missing")
    weapon = shooter.get_npc_weapon_component()
    require(weapon is not None, "Shotgun Shooter Weapon Component is missing")
    require(weapon.get_shotgun_pellet_count() == 8, "Shotgun Pellet default mismatch")
    require(abs(weapon.get_shotgun_spread_half_angle_degrees() - 12.0) < 0.001, "Shotgun Spread default mismatch")
    require(abs(weapon.get_shotgun_projectile_speed() - 3500.0) < 0.001, "Shotgun projectile speed mismatch")
    require(abs(weapon.get_shotgun_damage_per_pellet() - 3.0) < 0.001, "Shotgun Pellet damage mismatch")
    pellet_blueprint = unreal.EditorAssetLibrary.load_asset(PELLET_BLUEPRINT_PATH)
    require(isinstance(pellet_blueprint, unreal.Blueprint), "Dedicated Pellet Blueprint is missing")
    require(weapon.get_projectile_class() == pellet_blueprint.generated_class(), "Shotgun does not use its Pellet Blueprint")
    require(weapon.uses_projectile_ballistics(), "Shotgun Shooter must use projectile ballistics")
    require(not weapon.is_shotgun_debug_trace_enabled(), "Shotgun cyan debug rays must be disabled in the test map")
    hidden_weapon_visuals = [
        component
        for component in shooter.get_components_by_class(unreal.StaticMeshComponent)
        if component.get_name() == "WeaponVisualComponent"
    ]
    require(len(hidden_weapon_visuals) == 1, "Expected one inherited WeaponVisual placeholder")
    require(
        hidden_weapon_visuals[0].get_editor_property("static_mesh") is not None
        and not hidden_weapon_visuals[0].is_visible(),
        "Inherited WeaponVisual placeholder must be filled and hidden for clean Map Check",
    )

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable during validation")
    expected_game_mode = load_blueprint_class(GAME_MODE_BLUEPRINT_PATH)
    require(world.get_world_settings().get_editor_property("default_game_mode") == expected_game_mode, "GameMode override mismatch")
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    log(f"MAP_CHECK_EXECUTED|{MAP_PATH}")
    log("VALIDATION_OK|shotgun_npc=1|player_start=1|markers=3|los_blocker=1")


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(editor_assets is not None and level_editor is not None and actors is not None, "Editor subsystems are unavailable")

    ensure_shotgun_pellet_assets(editor_assets)

    map_exists = editor_assets.does_asset_exist(MAP_PATH)
    rebuild = os.environ.get("DRONE_SHOTGUN_TESTMAP_REBUILD") == "1"
    if not map_exists or rebuild:
        create_map(level_editor, actors, rebuild)
    validate_map(level_editor, actors)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
