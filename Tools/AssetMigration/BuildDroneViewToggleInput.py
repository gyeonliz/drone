"""Create and validate the project-owned P-key Drone camera view toggle input."""

import unreal


ACTION_FOLDER = "/Game/Drone/Prototype/Input/Actions"
ACTION_NAME = "IA_DronePrototype_ToggleView"
ACTION_PATH = f"{ACTION_FOLDER}/{ACTION_NAME}"
CONTEXT_PATH = "/Game/Drone/Prototype/Input/IMC_DronePrototype"
PAWN_BLUEPRINT_PATH = "/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load_required(path):
    asset = unreal.load_asset(path)
    require(asset is not None, f"Required asset is missing: {path}")
    return asset


def make_key(name):
    key = unreal.Key()
    key.set_editor_property("key_name", name)
    return key


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.EditorAssetLibrary

toggle_action = unreal.load_asset(ACTION_PATH)
if toggle_action is None:
    toggle_action = asset_tools.create_asset(
        ACTION_NAME,
        ACTION_FOLDER,
        unreal.InputAction,
        unreal.InputAction_Factory(),
    )
    require(toggle_action is not None, f"Could not create {ACTION_PATH}")
    unreal.log(f"DRONE_VIEW_INPUT|CREATED|{ACTION_PATH}")

toggle_action.modify()
toggle_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
require(
    editor_assets.save_loaded_asset(toggle_action, only_if_is_dirty=False),
    f"Could not save {ACTION_PATH}",
)

mapping_context = load_required(CONTEXT_PATH)
mapping_context.modify()
mapping_context.unmap_all_keys_from_action(toggle_action)
p_key = make_key("P")
mapping_context.map_key(toggle_action, p_key)
require(
    editor_assets.save_loaded_asset(mapping_context, only_if_is_dirty=False),
    f"Could not save {CONTEXT_PATH}",
)

pawn_blueprint = load_required(PAWN_BLUEPRINT_PATH)
unreal.BlueprintEditorLibrary.compile_blueprint(pawn_blueprint)
require(
    pawn_blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
    f"Blueprint failed to compile before input assignment: {PAWN_BLUEPRINT_PATH}",
)
pawn_cdo = unreal.get_default_object(pawn_blueprint.generated_class())
require(pawn_cdo is not None, "FPV integration Pawn CDO is unavailable")
pawn_cdo.modify()
pawn_blueprint.modify()
pawn_cdo.set_editor_property("toggle_view_action", toggle_action)
require(
    editor_assets.save_loaded_asset(pawn_blueprint, only_if_is_dirty=False),
    f"Could not save {PAWN_BLUEPRINT_PATH}",
)

saved_mappings = mapping_context.get_editor_property("default_key_mappings").get_editor_property("mappings")
toggle_mappings = [
    mapping
    for mapping in saved_mappings
    if mapping.get_editor_property("action") == toggle_action
]
require(len(toggle_mappings) == 1, f"ToggleView has {len(toggle_mappings)} mappings; expected one")
require(
    str(toggle_mappings[0].get_editor_property("key").get_editor_property("key_name")) == "P",
    "ToggleView is not mapped to P",
)
require(
    pawn_cdo.get_editor_property("toggle_view_action") == toggle_action,
    "FPV integration Pawn did not retain ToggleViewAction",
)

unreal.log(
    "DRONE_VIEW_INPUT|OK|"
    f"action={toggle_action.get_path_name()}|key=P|"
    f"mappings={len(saved_mappings)}|"
    f"pawn={pawn_blueprint.get_path_name()}"
)
