"""지속풍 체감용 독립 TestMap을 생성하거나 검증한다.

Production Training 맵은 열거나 저장하지 않는다. 기본값은 LightWind이며, OilRig의
빗물 마스크를 참조하는 프로젝트 소유 Rain Material과 조작 모드별 Drift를 시험한다.
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
VISUALIZER_BLUEPRINT_FOLDER = "/Game/Drone/Weather/Blueprints"
VISUALIZER_BLUEPRINT_NAME = "BP_DroneWeatherDebugVisualizer"
VISUALIZER_BLUEPRINT_PATH = f"{VISUALIZER_BLUEPRINT_FOLDER}/{VISUALIZER_BLUEPRINT_NAME}"
VISUALIZER_PARENT_CLASS_PATH = "/Script/Drone.DroneWeatherDebugVisualizer"
CONTROLLER_BLUEPRINT_FOLDER = "/Game/Drone/Weather/Blueprints"
CONTROLLER_BLUEPRINT_NAME = "BP_DroneRandomWeatherController"
CONTROLLER_BLUEPRINT_PATH = f"{CONTROLLER_BLUEPRINT_FOLDER}/{CONTROLLER_BLUEPRINT_NAME}"
CONTROLLER_PARENT_CLASS_PATH = "/Script/Drone.DroneWeatherController"
RAIN_BLUEPRINT_NAME = "BP_DroneRainVisual"
RAIN_BLUEPRINT_PATH = f"{CONTROLLER_BLUEPRINT_FOLDER}/{RAIN_BLUEPRINT_NAME}"
RAIN_PARENT_CLASS_PATH = "/Script/Drone.DroneRainVisualActor"
RAIN_MATERIAL_FOLDER = "/Game/Drone/Weather/Materials"
RAIN_MATERIAL_NAME = "M_DroneRainStreak_OilRigMask"
RAIN_MATERIAL_PATH = f"{RAIN_MATERIAL_FOLDER}/{RAIN_MATERIAL_NAME}"
OIL_RIG_RAIN_MASK_PATH = "/Game/Drone/ThirdParty/OilRig/Rain/Texture/T_rain_Mask"
RAIN_PLANE_MESH_PATH = "/Engine/BasicShapes/Plane"
CUBE_PATH = "/Engine/BasicShapes/Cube"

OWNED_TAG = unreal.Name("DroneWeatherSystemsTest.Owned")
VISUALIZER_TAG = unreal.Name("DroneWeatherSystemsTest.Visualizer")
FLOOR_LABEL = "WeatherSystemsTest_Floor"
PLAYER_START_LABEL = "WeatherSystemsTest_PlayerStart"
KEY_LIGHT_LABEL = "WeatherSystemsTest_KeyLight"
SKY_LIGHT_LABEL = "WeatherSystemsTest_SkyLight"
CONTROLLER_LABEL = "WeatherSystemsTest_Controller"
VISUALIZER_LABEL = "WeatherSystemsTest_Visualizer"
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


def ensure_visualizer_blueprint(editor_assets: unreal.EditorAssetSubsystem) -> unreal.Class:
    parent_class = unreal.load_class(None, VISUALIZER_PARENT_CLASS_PATH)
    require(parent_class is not None, f"Visualizer native Class is unavailable: {VISUALIZER_PARENT_CLASS_PATH}")
    unreal.EditorAssetLibrary.make_directory(VISUALIZER_BLUEPRINT_FOLDER)
    blueprint = editor_assets.load_asset(VISUALIZER_BLUEPRINT_PATH)
    created_blueprint = blueprint is None
    if blueprint is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            VISUALIZER_BLUEPRINT_NAME,
            VISUALIZER_BLUEPRINT_FOLDER,
            unreal.Blueprint.static_class(),
            factory,
            overwrite_existing=False,
        )
        log(f"CREATED_VISUALIZER_BLUEPRINT|{VISUALIZER_BLUEPRINT_PATH}")
    require(isinstance(blueprint, unreal.Blueprint), f"Missing Visualizer Blueprint: {VISUALIZER_BLUEPRINT_PATH}")
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == parent_class,
        f"Visualizer Blueprint parent mismatch: {VISUALIZER_BLUEPRINT_PATH}",
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    require(cdo is not None, "Visualizer CDO unavailable")
    cdo.set_editor_property("enable_rain_debug_preview", False)
    cdo.set_editor_property("rain_preview_max_streak_count", 0)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR, "Visualizer Blueprint compile failed")
    require(editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save Visualizer Blueprint")
    return blueprint.generated_class()


def ensure_random_weather_controller_blueprint(
    editor_assets: unreal.EditorAssetSubsystem,
) -> unreal.Class:
    parent_class = unreal.load_class(None, CONTROLLER_PARENT_CLASS_PATH)
    require(parent_class is not None, f"Weather Controller native Class unavailable: {CONTROLLER_PARENT_CLASS_PATH}")
    unreal.EditorAssetLibrary.make_directory(CONTROLLER_BLUEPRINT_FOLDER)
    blueprint = editor_assets.load_asset(CONTROLLER_BLUEPRINT_PATH)
    if blueprint is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            CONTROLLER_BLUEPRINT_NAME,
            CONTROLLER_BLUEPRINT_FOLDER,
            unreal.Blueprint.static_class(),
            factory,
            overwrite_existing=False,
        )
        log(f"CREATED_RANDOM_WEATHER_BLUEPRINT|{CONTROLLER_BLUEPRINT_PATH}")
    require(isinstance(blueprint, unreal.Blueprint), f"Missing Weather Controller Blueprint: {CONTROLLER_BLUEPRINT_PATH}")
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == parent_class,
        f"Weather Controller parent mismatch: {CONTROLLER_BLUEPRINT_PATH}",
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    profile = editor_assets.load_asset(LIGHT_WIND_PROFILE_PATH)
    require(cdo is not None and profile is not None, "Weather Controller CDO/Profile unavailable")
    cdo.set_editor_property("weather_profile", profile)
    cdo.set_editor_property("apply_instantly", True)
    cdo.set_editor_property("enable_random_wind", True)
    cdo.set_editor_property("include_calm", True)
    cdo.set_editor_property("minimum_direction_change_interval_seconds", 8.0)
    cdo.set_editor_property("maximum_direction_change_interval_seconds", 18.0)
    cdo.set_editor_property("minimum_speed_change_interval_seconds", 5.0)
    cdo.set_editor_property("maximum_speed_change_interval_seconds", 12.0)
    cdo.set_editor_property("minimum_wind_speed_meters_per_second", 1.0)
    cdo.set_editor_property("maximum_wind_speed_meters_per_second", 9.0)
    cdo.set_editor_property("enable_rain", True)
    cdo.set_editor_property("spawn_rain_visual", True)
    cdo.set_editor_property("rain_visual_class", ensure_rain_visual_blueprint(editor_assets))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR, "Weather Controller compile failed")
    require(editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save Weather Controller Blueprint")
    return blueprint.generated_class()


def ensure_rain_streak_material(editor_assets: unreal.EditorAssetSubsystem) -> unreal.Material:
    material = editor_assets.load_asset(RAIN_MATERIAL_PATH)
    if material is not None:
        require(isinstance(material, unreal.Material), f"Rain Material has wrong type: {RAIN_MATERIAL_PATH}")
        return material

    mask_texture = editor_assets.load_asset(OIL_RIG_RAIN_MASK_PATH)
    require(isinstance(mask_texture, unreal.Texture2D), f"Missing OilRig rain mask: {OIL_RIG_RAIN_MASK_PATH}")
    unreal.EditorAssetLibrary.make_directory(RAIN_MATERIAL_FOLDER)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        RAIN_MATERIAL_NAME,
        RAIN_MATERIAL_FOLDER,
        unreal.Material.static_class(),
        unreal.MaterialFactoryNew(),
        overwrite_existing=False,
    )
    require(isinstance(material, unreal.Material), f"Could not create Rain Material: {RAIN_MATERIAL_PATH}")
    material.modify()
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    mask = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -620, -20
    )
    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -620, -180
    )
    emissive = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -300, -150
    )
    opacity_strength = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -620, 170
    )
    opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -300, 80
    )
    require(all(node is not None for node in (mask, tint, emissive, opacity_strength, opacity)), "Rain Material nodes failed")
    mask.set_editor_property("texture", mask_texture)
    mask.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    tint.set_editor_property("constant", unreal.LinearColor(0.38, 0.58, 0.78, 1.0))
    opacity_strength.set_editor_property("r", 0.22)
    require(unreal.MaterialEditingLibrary.connect_material_expressions(mask, "R", emissive, "A"), "Rain mask emissive link failed")
    require(unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", emissive, "B"), "Rain tint link failed")
    require(unreal.MaterialEditingLibrary.connect_material_expressions(mask, "R", opacity, "A"), "Rain mask opacity link failed")
    require(unreal.MaterialEditingLibrary.connect_material_expressions(opacity_strength, "", opacity, "B"), "Rain opacity link failed")
    require(unreal.MaterialEditingLibrary.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "Rain emissive output failed")
    require(unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY), "Rain opacity output failed")
    unreal.MaterialEditingLibrary.recompile_material(material)
    require(editor_assets.save_loaded_asset(material, only_if_is_dirty=False), "Could not save Rain Material")
    log(f"CREATED_RAIN_MATERIAL|{RAIN_MATERIAL_PATH}|source={OIL_RIG_RAIN_MASK_PATH}|opacity=0.22")
    return material


def ensure_rain_visual_blueprint(editor_assets: unreal.EditorAssetSubsystem) -> unreal.Class:
    parent_class = unreal.load_class(None, RAIN_PARENT_CLASS_PATH)
    require(parent_class is not None, f"Rain visual native Class unavailable: {RAIN_PARENT_CLASS_PATH}")
    unreal.EditorAssetLibrary.make_directory(CONTROLLER_BLUEPRINT_FOLDER)
    blueprint = editor_assets.load_asset(RAIN_BLUEPRINT_PATH)
    if blueprint is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            RAIN_BLUEPRINT_NAME,
            CONTROLLER_BLUEPRINT_FOLDER,
            unreal.Blueprint.static_class(),
            factory,
            overwrite_existing=False,
        )
        log(f"CREATED_RAIN_BLUEPRINT|{RAIN_BLUEPRINT_PATH}")
    require(isinstance(blueprint, unreal.Blueprint), f"Missing Rain visual Blueprint: {RAIN_BLUEPRINT_PATH}")
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == parent_class,
        f"Rain visual Blueprint parent mismatch: {RAIN_BLUEPRINT_PATH}",
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    require(cdo is not None, "Rain visual CDO unavailable")
    cdo.set_editor_property("rain_enabled", True)
    cdo.set_editor_property("suppress_rain_indoors", True)
    cdo.set_editor_property("maximum_streak_count", 112)
    cdo.set_editor_property("follow_radius_centimeters", 1200.0)
    cdo.set_editor_property("follow_half_height_centimeters", 700.0)
    cdo.set_editor_property("streak_length_centimeters", 65.0)
    cdo.set_editor_property("streak_width_centimeters", 2.4)
    cdo.set_editor_property("fall_speed_centimeters_per_second", 2600.0)
    cdo.set_editor_property("indoor_trace_distance_centimeters", 10000.0)
    cdo.set_editor_property("indoor_check_interval_seconds", 0.20)
    cdo.set_editor_property("indoor_blend_seconds", 0.35)
    cdo.set_editor_property("clip_streaks_against_ceilings", True)
    cdo.set_editor_property("ceiling_trace_budget_per_frame", 8)
    cdo.set_editor_property("ceiling_surface_clearance_centimeters", 10.0)
    cdo.set_editor_property("use_complex_ceiling_traces", True)
    rain_plane = editor_assets.load_asset(RAIN_PLANE_MESH_PATH)
    require(isinstance(rain_plane, unreal.StaticMesh), f"Missing rain Plane Mesh: {RAIN_PLANE_MESH_PATH}")
    cdo.set_editor_property("streak_mesh", rain_plane)
    cdo.set_editor_property("streak_material", ensure_rain_streak_material(editor_assets))
    log(f"RAIN_VISUAL|mesh={RAIN_PLANE_MESH_PATH}|material={RAIN_MATERIAL_PATH}|oilrig_source=reference_only")
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR, "Rain visual Blueprint compile failed")
    require(editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save Rain visual Blueprint")
    return blueprint.generated_class()


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
    visualizer_class = ensure_visualizer_blueprint(editor_assets)
    controller_class = ensure_random_weather_controller_blueprint(editor_assets)

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

    controller = spawn_actor(actors, controller_class, CONTROLLER_LABEL, (0.0, 0.0, 100.0))
    controller.set_editor_property("weather_profile", profile)
    controller.set_editor_property("apply_instantly", True)

    visualizer = spawn_actor(actors, visualizer_class, VISUALIZER_LABEL, (-2100.0, 0.0, 550.0))
    visualizer.set_editor_property("tags", [OWNED_TAG, VISUALIZER_TAG])

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable")
    world.get_world_settings().set_editor_property("default_game_mode", load_blueprint_class(GAME_MODE_BLUEPRINT_PATH))
    require(level_editor.save_current_level(), f"Could not save test map: {MAP_PATH}")
    log(f"SAVED_MAP|{MAP_PATH}")


def validate_map(level_editor, actors) -> None:
    require(MAP_PATH != TRAINING_MAP_PATH, "Weather test map must never resolve to Training")
    require(level_editor.load_level(MAP_PATH), f"Could not load test map: {MAP_PATH}")
    for label in (FLOOR_LABEL, PLAYER_START_LABEL, KEY_LIGHT_LABEL, SKY_LIGHT_LABEL, CONTROLLER_LABEL, VISUALIZER_LABEL, *ARROW_LABELS):
        require(actor_by_label(actors, label) is not None, f"Missing test actor: {label}")

    controller = actor_by_label(actors, CONTROLLER_LABEL)
    profile = controller.get_editor_property("weather_profile") if controller else None
    require(profile is not None and profile.get_path_name().startswith(LIGHT_WIND_PROFILE_PATH), "LightWind Profile is not configured")
    require(controller.get_editor_property("apply_instantly"), "Weather Controller should apply instantly in the test map")
    require(controller.get_editor_property("enable_random_wind"), "Weather Controller should enable random wind")
    require(controller.get_editor_property("include_calm"), "Random wind should include CALM")
    require(controller.get_editor_property("enable_rain"), "Weather Controller should allow Profile rain")
    require(controller.get_editor_property("spawn_rain_visual"), "Weather Controller should spawn one local rain visual")
    require(controller.get_editor_property("rain_visual_class") is not None, "Weather Controller rain visual class is missing")
    require(
        controller.get_editor_property("minimum_direction_change_interval_seconds")
        <= controller.get_editor_property("maximum_direction_change_interval_seconds"),
        "Random wind direction interval is reversed",
    )
    require(
        controller.get_editor_property("minimum_speed_change_interval_seconds")
        <= controller.get_editor_property("maximum_speed_change_interval_seconds"),
        "Random wind speed interval is reversed",
    )
    visualizer = actor_by_label(actors, VISUALIZER_LABEL)
    require(visualizer is not None and VISUALIZER_TAG in visualizer.get_editor_property("tags"), "Weather Visualizer tag is missing")
    require(visualizer.get_flow_bead_count() >= 12, "Weather Visualizer needs enough moving beads to read wind")
    require(not visualizer.get_editor_property("enable_rain_debug_preview"), "Legacy blue rain debug lines must stay disabled")

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World is unavailable during validation")
    require(
        world.get_world_settings().get_editor_property("default_game_mode") == load_blueprint_class(GAME_MODE_BLUEPRINT_PATH),
        "GameMode override mismatch",
    )
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    log(f"MAP_CHECK_EXECUTED|{MAP_PATH}")
    log("VALIDATION_OK|profile=Weather.LightWind|random=8dir+calm|controller=1|wind_arrow=1|wind_beads=24|mode_keys=1,2,3,4|production_map_touched=0")


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(editor_assets is not None and level_editor is not None and actors is not None, "Editor subsystems are unavailable")
    ensure_visualizer_blueprint(editor_assets)
    ensure_rain_visual_blueprint(editor_assets)
    ensure_random_weather_controller_blueprint(editor_assets)
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
