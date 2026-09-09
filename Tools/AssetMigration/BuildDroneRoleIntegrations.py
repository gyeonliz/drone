"""Build distinct Scout/FPV/Drop integration Pawns from project-owned Drone Pack assets."""

import unreal


BASE_BP = "/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration"
ROLE_FOLDER = "/Game/Drone/Integrations/RoleDrones"
SCOUT_BP = f"{ROLE_FOLDER}/BP_DroneScoutIntegration"
DROP_BP = f"{ROLE_FOLDER}/BP_DroneDropIntegration"

FPV_EXPLOSION = "/Game/Drone/ThirdParty/ArmyVFX/Niagara/Destroyed/NS_Expl_Tank_1"
FPV_SOUND = "/Game/Drone/ThirdParty/InfantrySFX/Explosions/Cues/Cue_Explosion01_Cue"
FPV_ROTOR_VARIABLE_NAMES = ("FPVRotorA", "FPVRotorB", "FPVRotorC", "FPVRotorD")

SCOUT_PARTS = [
    ("VisualMeshComponent", "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01Body", (0, 0, 0), (0, -90, 0), (6, 6, 6), "DroneRoleVisual"),
    ("FPVRotorA", "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r1", (67.44, 6.12, 0.24), (0, 0, 0), (6, 6, 6), "DroneRoleVisual"),
    ("FPVRotorB", "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r2", (-6.072, -67.56, 0.24), (0, 0, 0), (6, 6, 6), "DroneRoleVisual"),
    ("FPVRotorC", "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r3", (6.12, 67.32, 0.24), (0, 0, 0), (6, 6, 6), "DroneRoleVisual"),
    ("FPVRotorD", "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r4", (-67.392, -6.24, 0.24), (0, 0, 0), (6, 6, 6), "DroneRoleVisual"),
    ("RoleCameraMesh", "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_pCamera", (0, 0, 0), (0, -90, 0), (6, 6, 6), "DroneRoleVisual"),
]

DROP_PARTS = [
    ("VisualMeshComponent", "/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_DeliveryBody", (0, 0, 0), (0, 90, 0), (1, 1, 1), "DroneRoleVisual"),
    ("FPVRotorA", "/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R1", (0, 0, 0), (0, 90, 0), (1, 1, 1), "DroneRoleVisual"),
    ("FPVRotorB", "/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R2", (0, 0, 0), (0, 90, 0), (1, 1, 1), "DroneRoleVisual"),
    ("FPVRotorC", "/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R3", (0, 0, 0), (0, 90, 0), (1, 1, 1), "DroneRoleVisual"),
    ("FPVRotorD", "/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R4", (0, 0, 0), (0, 90, 0), (1, 1, 1), "DroneRoleVisual"),
    ("RoleRotor5", "/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R5", (0, 0, 0), (0, 90, 0), (1, 1, 1), "DroneRoleVisual"),
    ("RoleRotor6", "/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R6", (0, 0, 0), (0, 90, 0), (1, 1, 1), "DroneRoleVisual"),
    ("RoleCameraMesh", "/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_CAM", (0, 0, 0), (0, 90, 0), (1, 1, 1), "DroneRoleVisual"),
    ("CarriedPayloadVisual", "/Engine/BasicShapes/Cube", (0, 0, -65), (0, 0, 0), (0.25, 0.25, 0.25), "DroneCarriedPayload"),
]


def require_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"필수 Asset을 불러오지 못했습니다: {path}")
    return asset


def load_or_duplicate_blueprint(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.make_directory(ROLE_FOLDER)
        if not unreal.EditorAssetLibrary.duplicate_asset(BASE_BP, path):
            raise RuntimeError(f"역할 Blueprint 복제 실패: {path}")
    return require_asset(path)


def gather(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    result = {}
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        name = str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data))
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)
        if name:
            result[name] = (handle, obj)
    return subsystem, result


def ensure_static_mesh_component(bp, variable_name):
    subsystem, entries = gather(bp)
    existing = entries.get(variable_name)
    if existing:
        return existing[1]

    parent = entries.get("VisualTiltPivot")
    if not parent:
        raise RuntimeError(f"{bp.get_name()}: VisualTiltPivot를 찾지 못했습니다")
    params = unreal.AddNewSubobjectParams(
        parent_handle=parent[0],
        new_class=unreal.StaticMeshComponent,
        blueprint_context=bp,
        conform_transform_to_parent=True,
    )
    result = subsystem.add_new_subobject(params)
    handle = result[0] if isinstance(result, tuple) else result
    fail_reason = result[1] if isinstance(result, tuple) and len(result) > 1 else ""
    if not unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle):
        raise RuntimeError(f"{bp.get_name()}: {variable_name} 추가 실패: {fail_reason}")
    if not subsystem.rename_subobject(handle, variable_name):
        raise RuntimeError(f"{bp.get_name()}: {variable_name} 이름 변경 실패")
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)


def configure_mesh(component, variable_name, mesh_path, location, rotation, scale, tag):
    mesh = require_asset(mesh_path)
    component.set_editor_property("static_mesh", mesh)
    component.set_editor_property("relative_location", unreal.Vector(*location))
    component.set_editor_property(
        "relative_rotation",
        unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]),
    )
    component.set_editor_property("relative_scale3d", unreal.Vector(*scale))
    component_tags = [unreal.Name(tag)]
    if "Rotor" in variable_name:
        component_tags.append(unreal.Name("DroneRotor"))
    component.set_editor_property("component_tags", component_tags)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("generate_overlap_events", False)
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_visibility(True, True)
    component.set_hidden_in_game(False, True)


def configure_blueprint(path, parts):
    bp = load_or_duplicate_blueprint(path)
    for variable_name, mesh_path, location, rotation, scale, tag in parts:
        component = ensure_static_mesh_component(bp, variable_name)
        if not component:
            raise RuntimeError(f"{bp.get_name()}: {variable_name} Component template을 찾지 못했습니다")
        configure_mesh(component, variable_name, mesh_path, location, rotation, scale, tag)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    unreal.log(f"DRONE_ROLE_BP|saved={path}|parts={len(parts)}")


def configure_fpv_presentation():
    bp = require_asset(BASE_BP)
    _, entries = gather(bp)

    for variable_name in FPV_ROTOR_VARIABLE_NAMES:
        entry = entries.get(variable_name)
        if not entry or not entry[1]:
            raise RuntimeError(f"FPV Blueprint의 {variable_name}를 찾지 못했습니다")
        component = entry[1]

        # FPV 공급 Mesh는 각 Rotor Geometry가 이미 본체 기준 네 모서리 좌표에 있다.
        # Component Location까지 반대 Offset을 주면 Geometry 중심이 기체 중앙으로 상쇄된다.
        component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
        component.set_editor_property(
            "relative_rotation",
            unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0),
        )
        component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
        tags = list(component.get_editor_property("component_tags"))
        rotor_tag = unreal.Name("DroneRotor")
        if rotor_tag not in tags:
            tags.append(rotor_tag)
            component.set_editor_property("component_tags", tags)

    impact = entries.get("ImpactDetonationComponent")
    if not impact or not impact[1]:
        raise RuntimeError("FPV Blueprint의 ImpactDetonationComponent를 찾지 못했습니다")
    impact[1].set_editor_property("explosion_effect", require_asset(FPV_EXPLOSION))
    impact[1].set_editor_property("explosion_sound", require_asset(FPV_SOUND))
    impact[1].set_editor_property("explosion_effect_scale", unreal.Vector(0.35, 0.35, 0.35))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    unreal.log("DRONE_ROLE_BP|FPV explosion presentation connected")


configure_blueprint(SCOUT_BP, SCOUT_PARTS)
configure_blueprint(DROP_BP, DROP_PARTS)
configure_fpv_presentation()
