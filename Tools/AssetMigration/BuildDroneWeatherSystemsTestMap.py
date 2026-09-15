"""지속풍 체감용 독립 TestMap을 생성하거나 검증한다.

Production Training 맵은 열거나 저장하지 않는다. 기본값은 LightWind이며, 비 Niagara가
아직 없는 상태에서도 쉬운 조작/Rate-Acro의 Drift 차이를 바로 시험할 수 있게 한다.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_WEATHER_TESTMAP"
MAP_FOLDER = "/Game/Drone/Maps/TestMap"
MAP_PATH = f"{MAP_FOLDER}/Lvl_DroneWeatherSystemsTest"
TRAINING_MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"
GAME_MODE_BLUEPRINT_PATH = "/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode"
LIGHT_WIND_PROFILE_PATH = "/Game/Drone/Data/Weather/DA_Weather_LightWind"
CUBE_PATH = "/Engine/BasicShapes/Cube"

OWNED_TAG = unreal.Name("DroneWeatherSystemsTest.Owned")
FLOOR_LABEL = "WeatherSystemsTest_Floor"
PLAYER_START_LABEL = "WeatherSystemsTest_PlayerStart"
KEY_LIGHT_LABEL = "WeatherSystemsTest_KeyLight"
SKY_LIGHT_LABEL = "WeatherSystemsTest_SkyLight"
CONTROLLER_LABEL = "WeatherSystemsTest_Controller"
ARROW_LABELS = (
    "WeatherSystemsTest_WindArrowShaft",
    "WeatherSystemsTest_WindArrowLeft",
    "WeatherSystemsTest_WindArrowRight",
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


def spawn_actor(actors, actor_class, label, location, rotation=unreal.Rotator()) -> unreal.Actor:
    actor = actors.spawn_actor_from_class(actor_class, unreal.Vector(*location), rotation)
    require(actor is not None, f"Could not spawn actor: {label}")
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [OWNED_TAG])
    return actor


def configure_cube(actor, mesh, scale, collision) -> None:
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    require(component is not None and component.set_static_mesh(mesh), f"Could not configure {actor.get_actor_label()}")
    component.set_collision_enabled(collision)


def clear_owned_actors(actors: unreal.EditorActorSubsystem) -> None:
    for actor in list(actors.get_all_level_actors()):
        if OWNED_TAG in actor.get_editor_property("tags"):
            require(actors.destroy_actor(actor), f"Could not remove owned actor: {actor.get_actor_label()}")


def create_map(level_editor, actors, rebuild: bool) -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    map_exists = editor_assets.does_asset_exist(MAP_PATH)
    if map_exists:
        require(level_editor.load_level(MAP_PATH), f"Could not load test map: {MAP_PATH}")
        require(rebuild, "Test map already exists; use validation mode or set DRONE_WEATHER_TESTMAP_REBUILD=1")
        clear_owned_actors(actors)
        log("REBUILDING_EXISTING_MAP")
    else:
        unreal.EditorAssetLibrary.make_directory(MAP_FOLDER)
        require(level_editor.new_level(MAP_PATH), f"Could not create test map: {MAP_PATH}")
        log(f"CREATED_MAP|{MAP_PATH}")

    cube = unreal.EditorAssetLibrary.load_asset(CUBE_PATH)
    profile = unreal.EditorAssetLibrary.load_asset(LIGHT_WIND_PROFILE_PATH)
    require(isinstance(cube, unreal.StaticMesh), f"Missing Engine Cube: {CUBE_PATH}")
    require(isinstance(profile, unreal.DroneWeatherProfile), f"Missing Weather Profile: {LIGHT_WIND_PROFILE_PATH}")

    floor = spawn_actor(actors, unreal.StaticMeshActor, FLOOR_LABEL, (1500.0, 0.0, -35.0))
    configure_cube(floor, cube, (100.0, 65.0, 0.20), unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    spawn_actor(actors, unreal.PlayerStart, PLAYER_START_LABEL, (-3500.0, 0.0, 325.0))
    spawn_actor(actors, unreal.DirectionalLight, KEY_LIGHT_LABEL, (0.0, 0.0, 1800.0))
    spawn_actor(actors, unreal.SkyLight, SKY_LIGHT_LABEL, (0.0, 0.0, 1400.0))

    # Profile 풍향 35도와 같은 방향을 가리키는 큰 바닥 화살표다.
    arrow_rotation = unreal.Rotator(0.0, 35.0, 0.0)
    shaft = spawn_actor(actors, unreal.StaticMeshActor, ARROW_LABELS[0], (500.0, -1800.0, 10.0), arrow_rotation)
    left = spawn_actor(actors, unreal.StaticMeshActor, ARROW_LABELS[1], (1350.0, -1205.0, 10.0), unreal.Rotator(0.0, 75.0, 0.0))
    right = spawn_actor(actors, unreal.StaticMeshActor, ARROW_LABELS[2], (1350.0, -1205.0, 10.0), unreal.Rotator(0.0, -5.0, 0.0))
    configure_cube(shaft, cube, (18.0, 1.2, 0.08), unreal.CollisionEnabled.NO_COLLISION)
    configure_cube(left, cube, (7.0, 1.2, 0.08), unreal.CollisionEnabled.NO_COLLISION)
    configure_cube(right, cube, (7.0, 1.2, 0.08), unreal.CollisionEnabled.NO_COLLISION)

    controller_class = unreal.load_class(None, "/Script/Drone.DroneWeatherController")
    require(controller_class is not None, "DroneWeatherController native Class is unavailable")
    controller = spawn_actor(actors, controller_class, CONTROLLER_LABEL, (0.0, 0.0, 100.0))
    controller.set_editor_property("weather_profile", profile)
    controller.set_editor_property("apply_instantly", True)

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable")
    world.get_world_settings().set_editor_property("default_game_mode", load_blueprint_class(GAME_MODE_BLUEPRINT_PATH))
    require(level_editor.save_current_level(), f"Could not save test map: {MAP_PATH}")
    log(f"SAVED_MAP|{MAP_PATH}")


def validate_map(level_editor, actors) -> None:
    require(MAP_PATH != TRAINING_MAP_PATH, "Weather test map must never resolve to Training")
    require(level_editor.load_level(MAP_PATH), f"Could not load test map: {MAP_PATH}")
    for label in (FLOOR_LABEL, PLAYER_START_LABEL, KEY_LIGHT_LABEL, SKY_LIGHT_LABEL, CONTROLLER_LABEL, *ARROW_LABELS):
        require(actor_by_label(actors, label) is not None, f"Missing test actor: {label}")

    controller = actor_by_label(actors, CONTROLLER_LABEL)
    profile = controller.get_editor_property("weather_profile") if controller else None
    require(profile is not None and profile.get_path_name().startswith(LIGHT_WIND_PROFILE_PATH), "LightWind Profile is not configured")
    require(controller.get_editor_property("apply_instantly"), "Weather Controller should apply instantly in the test map")

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable during validation")
    require(
        world.get_world_settings().get_editor_property("default_game_mode") == load_blueprint_class(GAME_MODE_BLUEPRINT_PATH),
        "GameMode override mismatch",
    )
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    log(f"MAP_CHECK_EXECUTED|{MAP_PATH}")
    log("VALIDATION_OK|profile=Weather.LightWind|controller=1|wind_arrow=1|production_map_touched=0")


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(editor_assets is not None and level_editor is not None and actors is not None, "Editor subsystems are unavailable")
    map_exists = editor_assets.does_asset_exist(MAP_PATH)
    rebuild = os.environ.get("DRONE_WEATHER_TESTMAP_REBUILD") == "1"
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
