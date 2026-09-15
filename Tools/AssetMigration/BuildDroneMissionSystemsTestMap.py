"""Create or validate the lightweight Mission and signal systems test map.

This script never opens or saves the production Training map.  It owns only the
actors carrying DroneMissionSystemsTest.Owned in
`/Game/Drone/Maps/TestMap/Lvl_DroneMissionSystemsTest`.

Set DRONE_MISSION_TESTMAP_REBUILD=1 to rebuild the owned actors explicitly.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_MISSION_TESTMAP"
MAP_FOLDER = "/Game/Drone/Maps/TestMap"
MAP_PATH = f"{MAP_FOLDER}/Lvl_DroneMissionSystemsTest"
TRAINING_MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"
GAME_MODE_BLUEPRINT_PATH = "/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode"
CARRYABLE_BLUEPRINT_PATH = "/Game/Drone/Abilities/Payload/BP_DroneCarryablePayload"
CUBE_PATH = "/Engine/BasicShapes/Cube"

OWNED_TAG = unreal.Name("DroneMissionSystemsTest.Owned")
RETURN_TARGET_TAG = unreal.Name("Test.Mission.ReturnZone")
RECON_TARGET_TAG = unreal.Name("Test.Mission.ReconTarget")
IMPACT_TARGET_TAG = unreal.Name("Test.Mission.ImpactTarget")
PAYLOAD_TARGET_TAG = unreal.Name("Test.Mission.PayloadTarget")

FLOOR_LABEL = "MissionSystemsTest_Floor"
PLAYER_START_LABEL = "MissionSystemsTest_PlayerStart"
KEY_LIGHT_LABEL = "MissionSystemsTest_KeyLight"
SKY_LIGHT_LABEL = "MissionSystemsTest_SkyLight"
CARRYABLE_LABEL = "MissionSystemsTest_CarryablePayload"
RETURN_ZONE_LABEL = "MissionSystemsTest_ReturnZone"

JAMMER_SPECS = (
    ("MissionSystemsTest_JammerWeak", (800.0, 0.0, 450.0), (900.0, 1400.0, 500.0), 0.35),
    ("MissionSystemsTest_JammerStrong", (2200.0, 0.0, 450.0), (900.0, 1400.0, 500.0), 0.80),
)

ROLE_TARGETS = (
    (
        "MissionSystemsTest_ReconTarget",
        "/Game/Drone/Abilities/RoleTargets/BP_RoleTest_ReconTarget",
        (800.0, 1750.0, 250.0),
        RECON_TARGET_TAG,
    ),
    (
        "MissionSystemsTest_ImpactTarget",
        "/Game/Drone/Abilities/RoleTargets/BP_RoleTest_ImpactTarget",
        (2200.0, 1750.0, 250.0),
        IMPACT_TARGET_TAG,
    ),
    (
        "MissionSystemsTest_PayloadTarget",
        "/Game/Drone/Abilities/RoleTargets/BP_RoleTest_PayloadTarget",
        (4200.0, -1500.0, 15.0),
        PAYLOAD_TARGET_TAG,
    ),
)

MARKER_SPECS = (
    ("MissionSystemsTest_WeakZonePad", (800.0, 0.0, 5.0), (18.0, 28.0, 0.08)),
    ("MissionSystemsTest_StrongZonePad", (2200.0, 0.0, 10.0), (18.0, 28.0, 0.16)),
    ("MissionSystemsTest_ReturnZonePad", (4800.0, 0.0, 15.0), (10.0, 20.0, 0.24)),
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


def actor_by_label(actors: unreal.EditorActorSubsystem, label: str) -> unreal.Actor | None:
    matches = [actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == label]
    require(len(matches) <= 1, f"Duplicate actor label: {label}")
    return matches[0] if matches else None


def spawn_actor(
    actors: unreal.EditorActorSubsystem,
    actor_class: unreal.Class,
    label: str,
    location: tuple[float, float, float],
    tags: tuple[unreal.Name, ...] = (),
) -> unreal.Actor:
    actor = actors.spawn_actor_from_class(actor_class, unreal.Vector(*location), unreal.Rotator())
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
        require(rebuild, "Test map already exists; use validation mode or set DRONE_MISSION_TESTMAP_REBUILD=1")
        clear_owned_actors(actors)
        log("REBUILDING_EXISTING_MAP")
    else:
        unreal.EditorAssetLibrary.make_directory(MAP_FOLDER)
        require(level_editor.new_level(MAP_PATH), f"Could not create test map: {MAP_PATH}")
        log(f"CREATED_MAP|{MAP_PATH}")

    cube = unreal.EditorAssetLibrary.load_asset(CUBE_PATH)
    require(isinstance(cube, unreal.StaticMesh), f"Missing Engine Cube: {CUBE_PATH}")

    floor = spawn_actor(actors, unreal.StaticMeshActor, FLOOR_LABEL, (1400.0, 0.0, -35.0))
    configure_static_mesh(floor, cube, (90.0, 55.0, 0.20), unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    spawn_actor(actors, unreal.PlayerStart, PLAYER_START_LABEL, (-3000.0, 0.0, 325.0))
    spawn_actor(actors, unreal.DirectionalLight, KEY_LIGHT_LABEL, (0.0, 0.0, 1800.0))
    spawn_actor(actors, unreal.SkyLight, SKY_LIGHT_LABEL, (0.0, 0.0, 1400.0))

    for label, location, scale in MARKER_SPECS:
        marker = spawn_actor(actors, unreal.StaticMeshActor, label, location)
        configure_static_mesh(marker, cube, scale, unreal.CollisionEnabled.NO_COLLISION)

    jammer_class = load_native_class("/Script/Drone.DroneJammingVolume")
    for label, location, extent, strength in JAMMER_SPECS:
        jammer = spawn_actor(actors, jammer_class, label, location)
        jammer.set_editor_property("normalized_jamming_strength", strength)
        bounds = jammer.get_jamming_bounds()
        require(bounds is not None, f"Jamming Bounds are unavailable: {label}")
        bounds.set_box_extent(unreal.Vector(*extent), True)

    return_zone_class = load_native_class("/Script/Drone.DroneMissionReturnZone")
    return_zone = spawn_actor(
        actors,
        return_zone_class,
        RETURN_ZONE_LABEL,
        (4800.0, 0.0, 300.0),
        (RETURN_TARGET_TAG,),
    )
    return_trigger = return_zone.get_return_trigger()
    require(return_trigger is not None, "Return Trigger is unavailable")
    return_trigger.set_box_extent(unreal.Vector(500.0, 1000.0, 350.0), True)

    for label, blueprint_path, location, target_tag in ROLE_TARGETS:
        spawn_actor(actors, load_blueprint_class(blueprint_path), label, location, (target_tag,))

    spawn_actor(
        actors,
        load_blueprint_class(CARRYABLE_BLUEPRINT_PATH),
        CARRYABLE_LABEL,
        (-1500.0, -900.0, 70.0),
    )

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable")
    game_mode_class = load_blueprint_class(GAME_MODE_BLUEPRINT_PATH)
    world.get_world_settings().set_editor_property("default_game_mode", game_mode_class)
    require(level_editor.save_current_level(), f"Could not save test map: {MAP_PATH}")
    log(f"SAVED_MAP|{MAP_PATH}")


def validate_map(level_editor: unreal.LevelEditorSubsystem, actors: unreal.EditorActorSubsystem) -> None:
    require(level_editor.load_level(MAP_PATH), f"Could not load test map: {MAP_PATH}")
    require(MAP_PATH != TRAINING_MAP_PATH, "Test map path must never resolve to Training")

    expected_labels = {
        FLOOR_LABEL,
        PLAYER_START_LABEL,
        KEY_LIGHT_LABEL,
        SKY_LIGHT_LABEL,
        CARRYABLE_LABEL,
        RETURN_ZONE_LABEL,
        *(spec[0] for spec in JAMMER_SPECS),
        *(spec[0] for spec in ROLE_TARGETS),
        *(spec[0] for spec in MARKER_SPECS),
    }
    for label in expected_labels:
        require(actor_by_label(actors, label) is not None, f"Missing test actor: {label}")

    for label, _, extent, strength in JAMMER_SPECS:
        jammer = actor_by_label(actors, label)
        require(jammer is not None and jammer.is_jammer_active(), f"Jammer is inactive: {label}")
        require(abs(jammer.get_normalized_jamming_strength() - strength) < 0.001, f"Jammer strength mismatch: {label}")
        actual_extent = jammer.get_jamming_bounds().get_unscaled_box_extent()
        require(actual_extent.equals(unreal.Vector(*extent)), f"Jammer extent mismatch: {label}")

    return_zone = actor_by_label(actors, RETURN_ZONE_LABEL)
    require(return_zone is not None, "Return Zone is missing")
    require(RETURN_TARGET_TAG in return_zone.get_editor_property("tags"), "Return Zone target Tag is missing")
    require(
        return_zone.get_return_trigger().get_unscaled_box_extent().equals(unreal.Vector(500.0, 1000.0, 350.0)),
        "Return Zone extent mismatch",
    )

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable during validation")
    expected_game_mode = load_blueprint_class(GAME_MODE_BLUEPRINT_PATH)
    require(world.get_world_settings().get_editor_property("default_game_mode") == expected_game_mode, "GameMode override mismatch")
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    log(f"MAP_CHECK_EXECUTED|{MAP_PATH}")
    log("VALIDATION_OK|jammers=2|overlap=1|return=1|targets=3|carryable=1")


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(editor_assets is not None and level_editor is not None and actors is not None, "Editor subsystems are unavailable")

    map_exists = editor_assets.does_asset_exist(MAP_PATH)
    rebuild = os.environ.get("DRONE_MISSION_TESTMAP_REBUILD") == "1"
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
