"""Create or validate the isolated four-route Training selection TestMap.

The tool never opens or saves the production Lvl_DroneTraining map. The generated
map keeps all four Splines editable in the Editor; at runtime the selector shows
only one Course and maps 1-4 to fixed Routes and 5 to a random Route.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_TRAINING_ROUTE_SELECTION_TESTMAP"
MAP_FOLDER = "/Game/Drone/Maps/TestMap"
MAP_PATH = f"{MAP_FOLDER}/Lvl_DroneTrainingRouteSelectionTest"
PRODUCTION_MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"

COURSE_BLUEPRINT_PATH = "/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingCourse"
GATE_BLUEPRINT_PATH = "/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingGate"
GAME_MODE_BLUEPRINT_PATH = "/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode"
CUBE_PATH = "/Engine/BasicShapes/Cube"

OWNED_TAG = unreal.Name("DroneTrainingRouteSelectionTest.Owned")
FLOOR_LABEL = "TrainingRouteSelectionTest_Floor"
PLAYER_START_LABEL = "TrainingRouteSelectionTest_PlayerStart"
KEY_LIGHT_LABEL = "TrainingRouteSelectionTest_KeyLight"
SKY_LIGHT_LABEL = "TrainingRouteSelectionTest_SkyLight"
SELECTOR_LABEL = "TrainingRouteSelectionTest_Selector"
GATE_COUNT = 5

ROUTES = (
    (
        "TrainingRouteSelectionTest_Route01_Straight",
        "Training.Route.01.Straight",
        unreal.LinearColor(0.02, 0.70, 1.0, 1.0),
        (
            (0.0, 0.0, 300.0),
            (1100.0, 0.0, 350.0),
            (2300.0, 0.0, 475.0),
            (3600.0, 0.0, 375.0),
            (5000.0, 0.0, 300.0),
        ),
    ),
    (
        "TrainingRouteSelectionTest_Route02_LeftCurve",
        "Training.Route.02.LeftCurve",
        unreal.LinearColor(0.10, 1.0, 0.25, 1.0),
        (
            (0.0, 0.0, 300.0),
            (850.0, -500.0, 425.0),
            (1750.0, -1450.0, 650.0),
            (2950.0, -1950.0, 525.0),
            (4150.0, -950.0, 400.0),
            (5200.0, 0.0, 300.0),
        ),
    ),
    (
        "TrainingRouteSelectionTest_Route03_RightCurve",
        "Training.Route.03.RightCurve",
        unreal.LinearColor(1.0, 0.70, 0.05, 1.0),
        (
            (0.0, 0.0, 300.0),
            (850.0, 500.0, 425.0),
            (1750.0, 1450.0, 650.0),
            (2950.0, 1950.0, 525.0),
            (4150.0, 950.0, 400.0),
            (5200.0, 0.0, 300.0),
        ),
    ),
    (
        "TrainingRouteSelectionTest_Route04_ClimbSlalom",
        "Training.Route.04.ClimbSlalom",
        unreal.LinearColor(1.0, 0.08, 0.70, 1.0),
        (
            (0.0, 0.0, 300.0),
            (900.0, 0.0, 900.0),
            (1850.0, 950.0, 1350.0),
            (2850.0, 0.0, 750.0),
            (3850.0, -950.0, 1250.0),
            (5100.0, 0.0, 450.0),
        ),
    ),
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
    require(generated_class is not None, f"Blueprint generated Class unavailable: {path}")
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


def configure_course(
    course: unreal.Actor,
    gate_class: unreal.Class,
    course_id: str,
    color: unreal.LinearColor,
    points: tuple[tuple[float, float, float], ...],
) -> None:
    spline = course.get_course_spline()
    require(spline is not None, f"Course spline unavailable: {course_id}")
    spline.modify()
    spline.clear_spline_points(False)
    for point_index, coordinates in enumerate(points):
        spline.add_spline_point(unreal.Vector(*coordinates), unreal.SplineCoordinateSpace.LOCAL, False)
        spline.set_spline_point_type(point_index, unreal.SplinePointType.CURVE, False)
    spline.set_closed_loop(False, False)
    spline.update_spline()

    course.set_editor_property("course_id", unreal.Name(course_id))
    course.set_editor_property("automatic_gate_class", gate_class)
    course.set_editor_property("course_line_color", color)
    course.set_editor_property("course_line_segment_length_centimeters", 100.0)
    course.set_editor_property("automatic_gate_scale", unreal.Vector(1.0, 1.0, 1.0))
    course.configure_automatic_gate_layout(True, GATE_COUNT, True, 250.0, 1000.0, 250.0)
    course.initialize_automatic_gate_spline_handles_from_current_layout()


def create_or_rebuild_map(
    level_editor: unreal.LevelEditorSubsystem,
    actors: unreal.EditorActorSubsystem,
    rebuild: bool,
) -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem unavailable")
    map_exists = editor_assets.does_asset_exist(MAP_PATH)
    if map_exists:
        require(level_editor.load_level(MAP_PATH), f"Could not load map: {MAP_PATH}")
        require(rebuild, "Map exists; use Validate or explicit Rebuild")
        clear_owned_actors(actors)
    else:
        unreal.EditorAssetLibrary.make_directory(MAP_FOLDER)
        require(level_editor.new_level(MAP_PATH), f"Could not create map: {MAP_PATH}")
        log(f"CREATED_MAP|{MAP_PATH}")

    cube = unreal.EditorAssetLibrary.load_asset(CUBE_PATH)
    require(isinstance(cube, unreal.StaticMesh), f"Missing Engine Cube: {CUBE_PATH}")
    floor = spawn_actor(actors, unreal.StaticMeshActor, FLOOR_LABEL, (200.0, 0.0, -25.0))
    floor.set_actor_scale3d(unreal.Vector(100.0, 65.0, 0.25))
    floor_component = floor.get_component_by_class(unreal.StaticMeshComponent)
    require(floor_component is not None and floor_component.set_static_mesh(cube), "Could not configure floor")
    floor_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)

    spawn_actor(actors, unreal.PlayerStart, PLAYER_START_LABEL, (-3000.0, 0.0, 350.0))
    spawn_actor(actors, unreal.DirectionalLight, KEY_LIGHT_LABEL, (0.0, 0.0, 2600.0), -35.0, -45.0)
    spawn_actor(actors, unreal.SkyLight, SKY_LIGHT_LABEL, (0.0, 0.0, 2200.0))

    course_class = load_blueprint_class(COURSE_BLUEPRINT_PATH)
    gate_class = load_blueprint_class(GATE_BLUEPRINT_PATH)
    courses: list[unreal.Actor] = []
    for label, course_id, color, points in ROUTES:
        course = spawn_actor(actors, course_class, label, (-2200.0, 0.0, 0.0))
        configure_course(course, gate_class, course_id, color, points)
        courses.append(course)

    selector_class = unreal.DroneTrainingRouteSelector.static_class()
    selector = spawn_actor(actors, selector_class, SELECTOR_LABEL, (-2600.0, 0.0, 100.0))
    selector.set_editor_property("initial_route_number", 1)
    selector.set_editor_property("random_seed", 260929)
    selector.set_editor_property("avoid_immediate_random_repeat", True)
    selector.configure_routes(courses)

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World unavailable")
    world.get_world_settings().set_editor_property("default_game_mode", load_blueprint_class(GAME_MODE_BLUEPRINT_PATH))
    require(level_editor.save_current_level(), f"Could not save map: {MAP_PATH}")
    log(f"SAVED_MAP|{MAP_PATH}")


def validate_map(level_editor: unreal.LevelEditorSubsystem, actors: unreal.EditorActorSubsystem) -> None:
    require(MAP_PATH != PRODUCTION_MAP_PATH, "Test map must never resolve to production Training")
    require(level_editor.load_level(MAP_PATH), f"Could not load map: {MAP_PATH}")

    selector = actor_by_label(actors, SELECTOR_LABEL)
    require(selector is not None, "Route Selector missing")
    require(selector.get_configured_route_count() == 4, "Selector Route count mismatch")

    course_ids: set[str] = set()
    for label, course_id, _color, _points in ROUTES:
        course = actor_by_label(actors, label)
        require(course is not None, f"Missing Route Actor: {label}")
        require(course.is_using_automatic_spline_gates(), f"Automatic Gates disabled: {label}")
        require(course.get_resolved_automatic_gate_count() == GATE_COUNT, f"Gate count mismatch: {label}")
        require(course.get_generated_automatic_gate_count() == GATE_COUNT, f"Generated Gate mismatch: {label}")
        require(course.get_course_spline().get_number_of_spline_points() >= 5, f"Spline point count too small: {label}")
        require(str(course.get_course_id()) == course_id, f"Course ID mismatch: {label}")
        course_ids.add(str(course.get_course_id()))
    require(len(course_ids) == 4, "Course IDs must be unique")

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World unavailable during validation")
    expected_game_mode = load_blueprint_class(GAME_MODE_BLUEPRINT_PATH)
    require(world.get_world_settings().get_editor_property("default_game_mode") == expected_game_mode, "GameMode mismatch")
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    log(f"MAP_CHECK_EXECUTED|{MAP_PATH}")
    log("VALIDATION_OK|routes=4|fixed_keys=1,2,3,4|random_key=5|gates_per_route=5")


def main() -> None:
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(level_editor is not None and actors is not None, "Editor subsystems unavailable")
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem unavailable")

    rebuild = os.environ.get("DRONE_TRAINING_ROUTE_TESTMAP_REBUILD") == "1"
    if not editor_assets.does_asset_exist(MAP_PATH) or rebuild:
        create_or_rebuild_map(level_editor, actors, rebuild)
    validate_map(level_editor, actors)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise

