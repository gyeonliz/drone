"""Create Fiber Optic strike and Ground UGV integration assets without touching vendor assets."""

from __future__ import annotations

import traceback

import unreal


PREFIX = "DRONE_EXTENDED_ROLES"
BASE_BP = "/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration"
ROLE_FOLDER = "/Game/Drone/Integrations/RoleDrones"
FIBER_BP = f"{ROLE_FOLDER}/BP_DroneFiberOpticIntegration"
GROUND_BP = f"{ROLE_FOLDER}/BP_DroneGroundUGVIntegration"

DRONE_DATA_FOLDER = "/Game/Drone/Data/Drones"
SCOUT_DEFINITION = f"{DRONE_DATA_FOLDER}/DA_Drone_Scout_Greybox"
FIBER_DEFINITION = f"{DRONE_DATA_FOLDER}/DA_Drone_FiberOptic_Greybox"
GROUND_DEFINITION = f"{DRONE_DATA_FOLDER}/DA_Drone_GroundUGV_Greybox"
MISSION_PATH = "/Game/Drone/Data/Missions/DA_Mission_Tutorial_Training"

FIBER_PREVIEW_MESH = "/Game/Drone/ThirdParty/DronePackFPV/SM_DroneFPVBody"
GROUND_MESH = "/Game/Drone/ThirdParty/GroundDroneKit/Meshes/GC_Drone_1/GC_Drone_1_SK"
FIBER_CABLE_MATERIAL = "/Game/Drone/ThirdParty/OilRig/Assets/Cable/Material_Instance/MI_cable"

FIBER_FPV_PARTS = [
    ("VisualMeshComponent", "/Game/Drone/ThirdParty/DronePackFPV/SM_DroneFPVBody", False),
    ("FPVRotorA", "/Game/Drone/ThirdParty/DronePackFPV/SM_RotorA", True),
    ("FPVRotorB", "/Game/Drone/ThirdParty/DronePackFPV/SM_RotorB", True),
    ("FPVRotorC", "/Game/Drone/ThirdParty/DronePackFPV/SM_RotorC", True),
    ("FPVRotorD", "/Game/Drone/ThirdParty/DronePackFPV/SM_RotorD", True),
]


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def require(condition, message: str):
    if not condition:
        raise RuntimeError(message)
    return condition


def load_asset(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    require(asset is not None, f"Missing asset: {path}")
    return asset


def load_or_duplicate_blueprint(path: str) -> unreal.Blueprint:
    unreal.EditorAssetLibrary.make_directory(ROLE_FOLDER)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        require(unreal.EditorAssetLibrary.duplicate_asset(BASE_BP, path), f"Could not duplicate Blueprint: {path}")
        log(f"CREATED_BLUEPRINT|{path}")
    blueprint = load_asset(path)
    require(isinstance(blueprint, unreal.Blueprint), f"Not a Blueprint: {path}")
    return blueprint


def gather_subobjects(blueprint: unreal.Blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    result = {}
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        name = str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data))
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)
        if name:
            result[name] = (handle, obj)
    return subsystem, result


def clear_inherited_static_visuals(blueprint: unreal.Blueprint) -> None:
    _, entries = gather_subobjects(blueprint)
    for _, component in entries.values():
        if isinstance(component, unreal.StaticMeshComponent):
            component.set_editor_property("static_mesh", None)
            component.set_editor_property("component_tags", [])
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_editor_property("generate_overlap_events", False)
            component.set_editor_property("can_ever_affect_navigation", False)
            component.set_visibility(False, True)
            component.set_hidden_in_game(True, True)


def configure_common_component(component, tags) -> None:
    component.set_editor_property("component_tags", [unreal.Name(tag) for tag in tags])
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("generate_overlap_events", False)
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_visibility(True, True)
    component.set_hidden_in_game(False, True)


def ensure_static_component(blueprint: unreal.Blueprint, variable_name: str):
    subsystem, entries = gather_subobjects(blueprint)
    existing = entries.get(variable_name)
    if existing:
        return existing[1]
    parent = entries.get("VisualTiltPivot")
    require(parent is not None, f"{blueprint.get_name()}: VisualTiltPivot is missing")
    params = unreal.AddNewSubobjectParams(
        parent_handle=parent[0],
        new_class=unreal.StaticMeshComponent,
        blueprint_context=blueprint,
        conform_transform_to_parent=True,
    )
    result = subsystem.add_new_subobject(params)
    handle = result[0] if isinstance(result, tuple) else result
    reason = result[1] if isinstance(result, tuple) and len(result) > 1 else ""
    require(unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle), f"Could not add {variable_name}: {reason}")
    require(subsystem.rename_subobject(handle, variable_name), f"Could not rename {variable_name}")
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)


def ensure_poseable_component(blueprint: unreal.Blueprint, variable_name: str):
    subsystem, entries = gather_subobjects(blueprint)
    existing = entries.get(variable_name)
    if existing:
        return existing[1]
    parent = entries.get("VisualTiltPivot")
    require(parent is not None, f"{blueprint.get_name()}: VisualTiltPivot is missing")
    params = unreal.AddNewSubobjectParams(
        parent_handle=parent[0],
        new_class=unreal.PoseableMeshComponent,
        blueprint_context=blueprint,
        conform_transform_to_parent=True,
    )
    result = subsystem.add_new_subobject(params)
    handle = result[0] if isinstance(result, tuple) else result
    reason = result[1] if isinstance(result, tuple) and len(result) > 1 else ""
    require(unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle), f"Could not add {variable_name}: {reason}")
    require(subsystem.rename_subobject(handle, variable_name), f"Could not rename {variable_name}")
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)


def configure_fiber_blueprint() -> unreal.Blueprint:
    blueprint = load_or_duplicate_blueprint(FIBER_BP)
    clear_inherited_static_visuals(blueprint)
    _, entries = gather_subobjects(blueprint)
    for variable_name, mesh_path, is_rotor in FIBER_FPV_PARTS:
        component = ensure_static_component(blueprint, variable_name)
        require(isinstance(component, unreal.StaticMeshComponent), f"{variable_name} is not a StaticMeshComponent")
        component.set_editor_property("static_mesh", load_asset(mesh_path))
        component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
        component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 90.0, 0.0))
        component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
        tags = ["DroneRoleVisual"]
        if is_rotor:
            tags.append("DroneRotor")
        configure_common_component(component, tags)
    _, entries = gather_subobjects(blueprint)
    spool = entries.get("FiberSpoolMeshComponent")
    require(spool and isinstance(spool[1], unreal.StaticMeshComponent), "Fiber spool Static Mesh slot is missing")
    spool_component = spool[1]
    # 통 Mesh는 제작 후 사용자가 이 빈 슬롯에 지정한다. 임의 Marketplace Mesh를 대신 넣지 않는다.
    spool_component.set_editor_property("static_mesh", None)
    spool_component.set_editor_property("relative_location", unreal.Vector(-32.0, 0.0, -18.0))
    spool_component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    spool_component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    configure_common_component(spool_component, ["FiberSpoolVisual"])
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("rotor_visual_spin_enabled", True)
    cdo.set_editor_property("first_person_camera_boom_offset", unreal.Vector(58.0, 0.0, 10.0))
    cdo.set_editor_property("fiber_spool_exit_offset", unreal.Vector(-14.0, 0.0, -3.0))
    cdo.set_editor_property("fiber_point_spacing_centimeters", 160.0)
    cdo.set_editor_property("fiber_sag_depth_centimeters", 45.0)
    cdo.set_editor_property("fiber_hanging_curve_subdivision_count", 4)
    cdo.set_editor_property("fiber_spline_tangent_scale", 0.75)
    cdo.set_editor_property("fiber_ground_trace_distance_centimeters", 10000.0)
    cdo.set_editor_property("fiber_ground_clearance_centimeters", 2.0)
    cdo.set_editor_property("fiber_cable_thickness_scale", 0.012)
    cdo.set_editor_property("fiber_maximum_laid_points", 96)
    cdo.set_editor_property("fiber_cable_material", load_asset(FIBER_CABLE_MATERIAL))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR, "Fiber Blueprint compile failed")
    require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save Fiber Blueprint")
    log(f"SAVED_BLUEPRINT|{FIBER_BP}")
    return blueprint


def ensure_skeletal_component(blueprint: unreal.Blueprint, variable_name: str):
    subsystem, entries = gather_subobjects(blueprint)
    existing = entries.get(variable_name)
    if existing:
        return existing[1]
    parent = entries.get("VisualTiltPivot")
    require(parent is not None, f"{blueprint.get_name()}: VisualTiltPivot is missing")
    params = unreal.AddNewSubobjectParams(
        parent_handle=parent[0],
        new_class=unreal.SkeletalMeshComponent,
        blueprint_context=blueprint,
        conform_transform_to_parent=True,
    )
    result = subsystem.add_new_subobject(params)
    handle = result[0] if isinstance(result, tuple) else result
    reason = result[1] if isinstance(result, tuple) and len(result) > 1 else ""
    require(unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle), f"Could not add {variable_name}: {reason}")
    require(subsystem.rename_subobject(handle, variable_name), f"Could not rename {variable_name}")
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)


def configure_ground_blueprint() -> unreal.Blueprint:
    blueprint = load_or_duplicate_blueprint(GROUND_BP)
    clear_inherited_static_visuals(blueprint)
    _, entries = gather_subobjects(blueprint)
    legacy_component = entries.get("GroundDroneVisual")
    if legacy_component and isinstance(legacy_component[1], unreal.SkeletalMeshComponent):
        legacy_component[1].set_visibility(False, True)
        legacy_component[1].set_hidden_in_game(True, True)
        legacy_component[1].set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component = ensure_poseable_component(blueprint, "GroundDronePoseableVisual")
    require(isinstance(component, unreal.PoseableMeshComponent), "GroundDronePoseableVisual is not a PoseableMeshComponent")
    component.set_skinned_asset_and_update(load_asset(GROUND_MESH))
    component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -55.0))
    component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    configure_common_component(component, ["DroneRoleVisual", "GroundDroneVisual"])
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("rotor_visual_spin_enabled", False)
    cdo.set_editor_property("ground_steering_rate_degrees_per_second", 95.0)
    cdo.set_editor_property("ground_sample_half_length_centimeters", 105.0)
    cdo.set_editor_property("ground_sample_half_width_centimeters", 75.0)
    cdo.set_editor_property("ground_clearance_centimeters", 58.0)
    cdo.set_editor_property("ground_trace_start_height_centimeters", 120.0)
    cdo.set_editor_property("ground_trace_distance_centimeters", 360.0)
    cdo.set_editor_property("ground_initial_acquire_distance_centimeters", 10000.0)
    cdo.set_editor_property("ground_height_interpolation_speed", 14.0)
    cdo.set_editor_property("ground_rotation_interpolation_speed", 9.0)
    cdo.set_editor_property("ground_upper_yaw_minimum_degrees", -160.0)
    cdo.set_editor_property("ground_upper_yaw_maximum_degrees", 160.0)
    cdo.set_editor_property("ground_weapon_pitch_minimum_degrees", -18.0)
    cdo.set_editor_property("ground_weapon_pitch_maximum_degrees", 38.0)
    cdo.set_editor_property("third_person_camera_arm_length", 650.0)
    cdo.set_editor_property("first_person_camera_boom_offset", unreal.Vector(135.0, 0.0, 72.0))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR, "Ground Blueprint compile failed")
    require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save Ground Blueprint")
    log(f"SAVED_BLUEPRINT|{GROUND_BP}")
    return blueprint


def make_acro_rates():
    return unreal.DroneAcroRateSettings(
        pitch_roll_center_sensitivity_degrees_per_second=180.0,
        maximum_pitch_rate_degrees_per_second=650.0,
        maximum_roll_rate_degrees_per_second=650.0,
        yaw_center_sensitivity_degrees_per_second=140.0,
        maximum_yaw_rate_degrees_per_second=400.0,
        pitch_roll_expo=0.30,
        yaw_expo=0.20,
        maximum_world_vertical_speed_centimeters_per_second=900.0,
        hover_throttle_normalized=0.50,
        gravity_acceleration_centimeters_per_second_squared=980.0,
        linear_drag_per_second=0.12,
        body_rate_response_time_seconds=0.08,
    )


def make_profile(max_speed, acceleration, deceleration, turning, yaw, handling, first_person, highlights):
    return unreal.DroneFlightProfile(
        default_control_mode=unreal.DroneControlMode.ASSISTED_EASY,
        default_handling_preset=handling,
        acro_rate_settings=make_acro_rates(),
        max_speed_centimeters_per_second=max_speed,
        acceleration_centimeters_per_second_squared=acceleration,
        deceleration_centimeters_per_second_squared=deceleration,
        turning_boost=turning,
        yaw_rate_degrees_per_second=yaw,
        maximum_visual_bank_roll_degrees=14.0,
        maximum_visual_tilt_pitch_degrees=11.0,
        start_in_first_person_view=first_person,
        max_health=100.0,
        feature_highlights=highlights,
    )


def duplicate_definition(path: str):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        require(unreal.EditorAssetLibrary.duplicate_asset(SCOUT_DEFINITION, path), f"Could not duplicate definition: {path}")
        log(f"CREATED_DEFINITION|{path}")
    return load_asset(path)


def configure_definition(asset, blueprint, drone_id, display_name, description, role, capabilities, profile, preview_mesh=None):
    pawn_class = blueprint.generated_class()
    asset.set_editor_property("pawn_class", pawn_class)
    asset.set_editor_property("preview_actor_class", pawn_class)
    asset.set_editor_property("preview_mesh", preview_mesh)
    asset.set_editor_property("drone_id", unreal.Name(drone_id))
    asset.set_editor_property("display_name", display_name)
    asset.set_editor_property("description", description)
    asset.set_editor_property("mission_role", role)
    asset.set_editor_property("player_controllable_in_current_build", True)
    asset.set_editor_property("planned_capabilities", capabilities)
    asset.set_editor_property("implemented_capabilities", capabilities)
    asset.set_editor_property("flight_profile", profile)
    asset.set_editor_property("locked", False)
    require(asset.is_definition_valid(), f"Definition validation failed: {drone_id}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), f"Could not save definition: {drone_id}")
    log(f"SAVED_DEFINITION|{drone_id}")


def configure_catalog_assets(fiber_blueprint, ground_blueprint) -> None:
    fiber = duplicate_definition(FIBER_DEFINITION)
    ground = duplicate_definition(GROUND_DEFINITION)
    configure_definition(
        fiber,
        fiber_blueprint,
        "Drone.FiberOptic.Greybox",
        "광섬유 자폭 드론 (그레이박스)",
        "유선 신호를 가정해 재밍 영향을 받지 않으며 충돌 자폭을 사용하는 시험 기체입니다.",
        unreal.DroneMissionRole.FIBER_OPTIC_STRIKE,
        [unreal.DroneGameplayCapability.IMPACT_DETONATION, unreal.DroneGameplayCapability.JAMMING_IMMUNITY],
        make_profile(1650.0, 3000.0, 2400.0, 8.0, 100.0, unreal.DroneHandlingPreset.BALANCED, True,
                     ["재밍 면역", "충돌 자폭", "1인칭 기본 시점"]),
        load_asset(FIBER_PREVIEW_MESH),
    )
    configure_definition(
        ground,
        ground_blueprint,
        "Drone.GroundUGV.Greybox",
        "지상 드론 UGV (그레이박스)",
        "W/S 전후 주행과 A/D 조향, 네 지점 지면 추종 및 총·유탄을 사용하는 무인 지상 차량 시험 기체입니다.",
        unreal.DroneMissionRole.GROUND_UGV,
        [unreal.DroneGameplayCapability.GROUND_DRIVE, unreal.DroneGameplayCapability.GROUND_WEAPONS],
        make_profile(900.0, 1400.0, 2200.0, 5.0, 95.0, unreal.DroneHandlingPreset.STABLE, False,
                     ["W/S 전후 주행", "상부 독립 조준", "Primary 총 / Secondary 유탄"]),
    )

    mission = load_asset(MISSION_PATH)
    mission.set_editor_property("allowed_drone_ids", [
        unreal.Name("Drone.Scout.Greybox"),
        unreal.Name("Drone.FPVStrike.Greybox"),
        unreal.Name("Drone.Drop.Greybox"),
        unreal.Name("Drone.FiberOptic.Greybox"),
        unreal.Name("Drone.GroundUGV.Greybox"),
    ])
    mission.set_editor_property("default_drone_id", unreal.Name("Drone.Scout.Greybox"))
    require(unreal.EditorAssetLibrary.save_loaded_asset(mission, only_if_is_dirty=False), "Could not save Tutorial mission")
    log("UPDATED_MISSION|allowed_drones=5")


def main() -> None:
    require(unreal.EditorAssetLibrary.does_asset_exist(BASE_BP), f"Missing base Blueprint: {BASE_BP}")
    require(unreal.EditorAssetLibrary.does_asset_exist(SCOUT_DEFINITION), f"Missing base Definition: {SCOUT_DEFINITION}")
    fiber_blueprint = configure_fiber_blueprint()
    ground_blueprint = configure_ground_blueprint()
    configure_catalog_assets(fiber_blueprint, ground_blueprint)
    log("COMPLETE|fiber=1|ground=1|vendor_assets_modified=0")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
