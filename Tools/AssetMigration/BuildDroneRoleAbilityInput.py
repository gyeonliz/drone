"""Create the two temporary Enhanced Input actions used by all current Drone roles."""

import unreal


ACTION_FOLDER = "/Game/Drone/Prototype/Input/Actions"
CONTEXT_PATH = "/Game/Drone/Prototype/Input/IMC_DronePrototype"
PAWN_BLUEPRINT_PATH = "/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration"
ACTION_SPECS = (
    (
        "IA_DronePrototype_PrimaryAbility",
        ("LeftMouseButton", "Gamepad_RightShoulder"),
        "primary_role_ability_action",
    ),
    (
        "IA_DronePrototype_SecondaryAbility",
        ("RightMouseButton", "Gamepad_LeftShoulder"),
        "secondary_role_ability_action",
    ),
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load_required(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    require(asset is not None, f"필수 Asset을 불러오지 못했습니다: {path}")
    return asset


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mapping_context = load_required(CONTEXT_PATH)
pawn_blueprint = load_required(PAWN_BLUEPRINT_PATH)
unreal.BlueprintEditorLibrary.compile_blueprint(pawn_blueprint)
require(
    pawn_blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
    f"입력 연결 전 Blueprint Compile 실패: {PAWN_BLUEPRINT_PATH}",
)
pawn_cdo = unreal.get_default_object(pawn_blueprint.generated_class())
require(pawn_cdo is not None, "FPV Integration Pawn CDO를 찾지 못했습니다")

created_actions = []
for action_name, key_names, pawn_property_name in ACTION_SPECS:
    action_path = f"{ACTION_FOLDER}/{action_name}"
    action = None
    if unreal.EditorAssetLibrary.does_asset_exist(action_path):
        action = load_required(action_path)
    else:
        action = asset_tools.create_asset(
            action_name,
            ACTION_FOLDER,
            unreal.InputAction,
            unreal.InputAction_Factory(),
        )
    require(action is not None, f"Input Action 생성 실패: {action_path}")
    action.modify()
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    require(
        unreal.EditorAssetLibrary.save_loaded_asset(action, only_if_is_dirty=False),
        f"Input Action 저장 실패: {action_path}",
    )

    mapping_context.modify()
    mapping_context.unmap_all_keys_from_action(action)
    for key_name in key_names:
        mapping_context.map_key(action, make_key(key_name))
    pawn_blueprint.modify()
    pawn_cdo.modify()
    pawn_cdo.set_editor_property(pawn_property_name, action)
    created_actions.append((action, key_names, pawn_property_name))

require(
    unreal.EditorAssetLibrary.save_loaded_asset(mapping_context, only_if_is_dirty=False),
    f"Input Mapping Context 저장 실패: {CONTEXT_PATH}",
)
require(
    unreal.EditorAssetLibrary.save_loaded_asset(pawn_blueprint, only_if_is_dirty=False),
    f"Pawn Blueprint 저장 실패: {PAWN_BLUEPRINT_PATH}",
)

saved_mappings = mapping_context.get_editor_property("default_key_mappings").get_editor_property("mappings")
for action, key_names, pawn_property_name in created_actions:
    action_mappings = [
        mapping
        for mapping in saved_mappings
        if mapping.get_editor_property("action") == action
    ]
    require(
        len(action_mappings) == len(key_names),
        f"{action.get_name()} Mapping 수가 {len(key_names)}이 아닙니다",
    )
    saved_key_names = {
        str(mapping.get_editor_property("key").get_editor_property("key_name"))
        for mapping in action_mappings
    }
    require(
        saved_key_names == set(key_names),
        f"{action.get_name()} Key가 {key_names}와 일치하지 않습니다: {saved_key_names}",
    )
    require(
        pawn_cdo.get_editor_property(pawn_property_name) == action,
        f"Pawn CDO가 {pawn_property_name}을 보존하지 못했습니다",
    )

unreal.log(
    "DRONE_ROLE_INPUT|OK|"
    "primary=LeftMouseButton+Gamepad_RightShoulder|"
    "secondary=RightMouseButton+Gamepad_LeftShoulder|"
    f"mappings={len(saved_mappings)}"
)
