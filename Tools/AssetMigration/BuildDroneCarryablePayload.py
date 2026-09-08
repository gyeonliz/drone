"""Create a placeable carryable Payload Blueprint and put one pickup in the Training map."""

import unreal


BLUEPRINT_FOLDER = "/Game/Drone/Abilities/Payload"
BLUEPRINT_NAME = "BP_DroneCarryablePayload"
BLUEPRINT_PATH = f"{BLUEPRINT_FOLDER}/{BLUEPRINT_NAME}"
PARENT_CLASS_PATH = "/Script/Drone.DroneDroppedPayload"
CRATE_MESH_PATH = "/Game/FC_MilitaryCamp/Models/MilitaryModels/SM_MilitaryCrate_01"
MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"
DROP_PAWN_BLUEPRINT_PATH = "/Game/Drone/Integrations/RoleDrones/BP_DroneDropIntegration"
ACTOR_LABEL = "RoleTest_CarryablePayload"
ACTOR_LOCATION = unreal.Vector(300.0, -300.0, 70.0)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
crate_mesh = unreal.load_asset(CRATE_MESH_PATH)
require(parent_class is not None, f"Native Payload Class를 찾지 못했습니다: {PARENT_CLASS_PATH}")
require(crate_mesh is not None, f"Crate Mesh를 찾지 못했습니다: {CRATE_MESH_PATH}")

blueprint = unreal.load_asset(BLUEPRINT_PATH)
if blueprint is None:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        BLUEPRINT_NAME,
        BLUEPRINT_FOLDER,
        unreal.Blueprint,
        factory,
    )
require(blueprint is not None, f"Blueprint 생성 실패: {BLUEPRINT_PATH}")
require(
    unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == parent_class,
    f"Blueprint Parent가 {PARENT_CLASS_PATH}가 아닙니다",
)

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
require(
    blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
    f"Blueprint Compile 실패: {BLUEPRINT_PATH}",
)
payload_cdo = unreal.get_default_object(blueprint.generated_class())
require(payload_cdo is not None, "Carryable Payload CDO를 찾지 못했습니다")
payload_cdo.modify()
blueprint.modify()
payload_cdo.set_editor_property("starts_as_carryable_pickup", True)
payload_visual = payload_cdo.get_payload_visual()
require(payload_visual is not None, "PayloadVisual Component를 찾지 못했습니다")
payload_visual.set_static_mesh(crate_mesh)
payload_visual.set_editor_property("relative_scale3d", unreal.Vector(0.35, 0.35, 0.35))

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
require(
    blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
    f"설정 후 Blueprint Compile 실패: {BLUEPRINT_PATH}",
)
require(
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False),
    f"Blueprint 저장 실패: {BLUEPRINT_PATH}",
)

drop_pawn_blueprint = unreal.load_asset(DROP_PAWN_BLUEPRINT_PATH)
require(drop_pawn_blueprint is not None, f"Drop Pawn Blueprint를 찾지 못했습니다: {DROP_PAWN_BLUEPRINT_PATH}")
subobject_subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
payload_drop_template = None
for handle in subobject_subsystem.k2_gather_subobject_data_for_blueprint(drop_pawn_blueprint):
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    variable_name = str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data))
    if variable_name == "PayloadDropComponent":
        payload_drop_template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(
            data,
            drop_pawn_blueprint,
        )
        break
require(payload_drop_template is not None, "Drop Pawn의 PayloadDropComponent Template을 찾지 못했습니다")
payload_drop_template.set_editor_property("payload_class", blueprint.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(drop_pawn_blueprint)
require(
    drop_pawn_blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
    f"Drop Pawn Blueprint Compile 실패: {DROP_PAWN_BLUEPRINT_PATH}",
)
require(
    unreal.EditorAssetLibrary.save_loaded_asset(drop_pawn_blueprint, only_if_is_dirty=False),
    f"Drop Pawn Blueprint 저장 실패: {DROP_PAWN_BLUEPRINT_PATH}",
)

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
require(world is not None, f"Training Map을 불러오지 못했습니다: {MAP_PATH}")
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actor_subsystem.get_all_level_actors():
    if actor.get_actor_label() == ACTOR_LABEL:
        actor_subsystem.destroy_actor(actor)

carryable_actor = actor_subsystem.spawn_actor_from_class(
    blueprint.generated_class(),
    ACTOR_LOCATION,
    unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0),
)
require(carryable_actor is not None, "Training Map Carryable Payload 배치 실패")
carryable_actor.set_actor_label(ACTOR_LABEL)
require(
    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH),
    "Training Map 저장 실패",
)

unreal.log(
    "DRONE_CARRYABLE_PAYLOAD|OK|"
    f"blueprint={BLUEPRINT_PATH}|mesh={CRATE_MESH_PATH}|"
    f"drop_pawn={DROP_PAWN_BLUEPRINT_PATH}|map={MAP_PATH}|"
    f"actor={ACTOR_LABEL}|location={ACTOR_LOCATION}"
)
