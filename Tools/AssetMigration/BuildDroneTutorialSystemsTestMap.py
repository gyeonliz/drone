"""Create or validate the lightweight Drone tutorial systems test map.

This tool never targets or saves Lvl_DroneTraining. The Editor may load its configured
startup map read-only before the script runs. The script creates only
`/Game/Drone/Maps/TestMap/Lvl_DroneTutorialSystemsTest` and owns the exact actor labels
declared below. Once the map exists, the default behavior is read-only validation so a
designer's later handle adjustments are not silently overwritten.

Set DRONE_TUTORIAL_TESTMAP_REBUILD=1 to rebuild the owned actors explicitly.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_TUTORIAL_TESTMAP"
MAP_FOLDER = "/Game/Drone/Maps/TestMap"
MAP_PATH = f"{MAP_FOLDER}/Lvl_DroneTutorialSystemsTest"
TRAINING_MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"

COURSE_BLUEPRINT_PATH = "/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingCourse"
GATE_BLUEPRINT_PATH = "/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingGate"
GAME_MODE_BLUEPRINT_PATH = "/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode"
CARRYABLE_BLUEPRINT_PATH = "/Game/Drone/Abilities/Payload/BP_DroneCarryablePayload"
CUBE_PATH = "/Engine/BasicShapes/Cube"

COURSE_LABEL = "TutorialSystemsTest_Course"
FLOOR_LABEL = "TutorialSystemsTest_Floor"
PLAYER_START_LABEL = "TutorialSystemsTest_PlayerStart"
KEY_LIGHT_LABEL = "TutorialSystemsTest_KeyLight"
SKY_LIGHT_LABEL = "TutorialSystemsTest_SkyLight"
CARRYABLE_LABEL = "RoleTest_CarryablePayload"
OWNED_TAG = unreal.Name("DroneTutorialSystemsTest.Owned")
GATE_COUNT = 5

ROLE_TARGETS = (
    (
        "RoleTest_ReconTarget",
        "/Game/Drone/Abilities/RoleTargets/BP_RoleTest_ReconTarget",
        (900.0, 1500.0, 250.0),
        -90.0,
    ),
    (
        "RoleTest_ImpactTarget",
        "/Game/Drone/Abilities/RoleTargets/BP_RoleTest_ImpactTarget",
        (1900.0, 1500.0, 250.0),
        -90.0,
    ),
    (
        "RoleTest_PayloadTarget",
        "/Game/Drone/Abilities/RoleTargets/BP_RoleTest_PayloadTarget",
        (1400.0, -1500.0, 15.0),
        90.0,
    ),
)

COURSE_POINTS = (
    (0.0, 0.0, 300.0),
    (1200.0, 0.0, 425.0),
    (2400.0, 650.0, 550.0),
    (3600.0, 0.0, 475.0),
    (4800.0, -650.0, 350.0),
    (6000.0, 0.0, 300.0),
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


def actor_by_label(actors: unreal.EditorActorSubsystem, label: str) -> unreal.Actor | None:
    matches = [actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == label]
    require(len(matches) <= 1, f"Duplicate actor label: {label}")
    return matches[0] if matches else None


def spawn_actor(
    actors: unreal.EditorActorSubsystem,
    actor_class: unreal.Class,
    label: str,
    location: tuple[float, float, float],
    yaw: float = 0.0,
    pitch: float = 0.0,
) -> unreal.Actor:
    actor = actors.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0),
    )
    require(actor is not None, f"Could not spawn actor: {label}")
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [OWNED_TAG])
    return actor


def clear_owned_actors(actors: unreal.EditorActorSubsystem) -> None:
    for actor in list(actors.get_all_level_actors()):
        if OWNED_TAG in actor.get_editor_property("tags"):
            label = actor.get_actor_label()
            require(actors.destroy_actor(actor), f"Could not remove owned actor: {label}")
            log(f"REMOVED_OWNED_ACTOR|{label}")


def configure_course(course: unreal.Actor, gate_class: unreal.Class) -> None:
    spline = course.get_course_spline()
    require(spline is not None, "Course spline is unavailable")
    spline.modify()
    spline.clear_spline_points(False)
    for point_index, coordinates in enumerate(COURSE_POINTS):
        spline.add_spline_point(
            unreal.Vector(*coordinates),
            unreal.SplineCoordinateSpace.LOCAL,
            False,
        )
        spline.set_spline_point_type(point_index, unreal.SplinePointType.CURVE, False)
    spline.set_closed_loop(False, False)
    spline.update_spline()

    course.set_editor_property("automatic_gate_class", gate_class)
    course.set_editor_property("course_line_segment_length_centimeters", 100.0)
    course.set_editor_property("automatic_gate_scale", unreal.Vector(1.0, 1.0, 1.0))
    course.configure_automatic_gate_layout(True, GATE_COUNT, True, 300.0, 1200.0, 300.0)
    course.initialize_automatic_gate_spline_handles_from_current_layout()


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
        require(rebuild, "Test map already exists; use validation mode or set DRONE_TUTORIAL_TESTMAP_REBUILD=1")
        clear_owned_actors(actors)
        log("REBUILDING_EXISTING_MAP")
    else:
        unreal.EditorAssetLibrary.make_directory(MAP_FOLDER)
        require(level_editor.new_level(MAP_PATH), f"Could not create test map: {MAP_PATH}")
        log(f"CREATED_MAP|{MAP_PATH}")

    cube = unreal.EditorAssetLibrary.load_asset(CUBE_PATH)
    require(isinstance(cube, unreal.StaticMesh), f"Missing Engine Cube: {CUBE_PATH}")
    floor = spawn_actor(actors, unreal.StaticMeshActor, FLOOR_LABEL, (600.0, 0.0, -20.0))
    floor.set_actor_scale3d(unreal.Vector(80.0, 50.0, 0.2))
    floor_component = floor.get_component_by_class(unreal.StaticMeshComponent)
    require(floor_component is not None and floor_component.set_static_mesh(cube), "Could not configure floor")
    floor_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)

    spawn_actor(actors, unreal.PlayerStart, PLAYER_START_LABEL, (-2250.0, 0.0, 325.0), 0.0)
    spawn_actor(actors, unreal.DirectionalLight, KEY_LIGHT_LABEL, (0.0, 0.0, 1500.0), -35.0, -45.0)
    spawn_actor(actors, unreal.SkyLight, SKY_LIGHT_LABEL, (0.0, 0.0, 1200.0))

    course_class = load_blueprint_class(COURSE_BLUEPRINT_PATH)
    gate_class = load_blueprint_class(GATE_BLUEPRINT_PATH)
    course = spawn_actor(actors, course_class, COURSE_LABEL, (-2000.0, 0.0, 0.0))
    configure_course(course, gate_class)

    for label, blueprint_path, location, yaw in ROLE_TARGETS:
        spawn_actor(actors, load_blueprint_class(blueprint_path), label, location, yaw)

    spawn_actor(
        actors,
        load_blueprint_class(CARRYABLE_BLUEPRINT_PATH),
        CARRYABLE_LABEL,
        (600.0, -1000.0, 70.0),
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
        COURSE_LABEL,
        CARRYABLE_LABEL,
        *(spec[0] for spec in ROLE_TARGETS),
    }
    for label in expected_labels:
        require(actor_by_label(actors, label) is not None, f"Missing test actor: {label}")

    course = actor_by_label(actors, COURSE_LABEL)
    require(course is not None, "Training Course is missing")
    require(course.is_using_automatic_spline_gates(), "Automatic Gate mode is disabled")
    require(course.is_using_independent_automatic_gate_handles(), "Independent Ring Handle mode is disabled")
    require(course.get_resolved_automatic_gate_count() == GATE_COUNT, "Resolved Ring count mismatch")
    require(course.get_generated_automatic_gate_count() == GATE_COUNT, "Generated Ring count mismatch")
    require(len(course.get_editor_property("automatic_gate_spline_handles")) == GATE_COUNT, "Ring Handle count mismatch")
    sequence = course.get_gate_sequence_component()
    require(sequence is not None and sequence.is_configuration_valid(), "Gate Sequence is invalid")
    require(sequence.get_configured_gate_count() == GATE_COUNT, "Gate Sequence count mismatch")

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable during validation")
    expected_game_mode = load_blueprint_class(GAME_MODE_BLUEPRINT_PATH)
    require(world.get_world_settings().get_editor_property("default_game_mode") == expected_game_mode, "GameMode override mismatch")
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    log(f"MAP_CHECK_EXECUTED|{MAP_PATH}")
    log(f"VALIDATION_OK|map={MAP_PATH}|rings={GATE_COUNT}|targets=3|carryable=1")


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(editor_assets is not None and level_editor is not None and actors is not None, "Editor subsystems are unavailable")

    map_exists = editor_assets.does_asset_exist(MAP_PATH)
    rebuild = os.environ.get("DRONE_TUTORIAL_TESTMAP_REBUILD") == "1"
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
