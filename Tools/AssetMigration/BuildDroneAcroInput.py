"""Create the dedicated keyboard/gamepad Rate/Acro Enhanced Input contract.

The shared Move/Altitude/Yaw actions remain untouched for Assisted and Manual modes.
Keyboard Acro actions stay semantic and intuitive. Gamepad vertical axes are raw
left/right-stick actions so C++ can apply real RC transmitter Mode 1 or Mode 2.
"""

import unreal


ACTION_FOLDER = "/Game/Drone/Prototype/Input/Actions"
CONTEXT_PATH = "/Game/Drone/Prototype/Input/IMC_DronePrototype"
PAWN_BLUEPRINT_PATH = "/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration"

ACTION_SPECS = (
    (
        "IA_DronePrototype_AcroPitch",
        "acro_pitch_action",
        (("W", False), ("S", True)),
    ),
    (
        "IA_DronePrototype_AcroRoll",
        "acro_roll_action",
        (("D", False), ("A", True), ("Gamepad_RightX", False)),
    ),
    (
        "IA_DronePrototype_AcroYaw",
        "acro_yaw_action",
        (("E", False), ("Q", True), ("Gamepad_LeftX", False)),
    ),
    (
        "IA_DronePrototype_AcroThrottle",
        "acro_throttle_action",
        (("SpaceBar", False), ("LeftControl", True)),
    ),
    (
        "IA_DronePrototype_AcroGamepadLeftVertical",
        "acro_gamepad_left_vertical_action",
        (("Gamepad_LeftY", False),),
    ),
    (
        "IA_DronePrototype_AcroGamepadRightVertical",
        "acro_gamepad_right_vertical_action",
        (("Gamepad_RightY", False),),
    ),
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", unreal.Name(key_name))
    require(unreal.InputLibrary.key_is_valid(key), f"Invalid Unreal input key: {key_name}")
    return key


def make_negate_x(outer):
    modifier = unreal.InputModifierNegate(outer=outer)
    modifier.set_editor_properties({"x": True, "y": False, "z": False})
    return modifier


def make_mapping(context, action, key_name, negate):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", make_key(key_name))
    mapping.set_editor_property("modifiers", [make_negate_x(context)] if negate else [])
    mapping.set_editor_property("triggers", [])
    return mapping


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
require(editor_assets is not None, "EditorAssetSubsystem is unavailable")

mapping_context = unreal.load_asset(CONTEXT_PATH)
require(mapping_context is not None, f"Required asset is missing: {CONTEXT_PATH}")

pawn_blueprint = unreal.load_asset(PAWN_BLUEPRINT_PATH)
require(pawn_blueprint is not None, f"Required asset is missing: {PAWN_BLUEPRINT_PATH}")
unreal.BlueprintEditorLibrary.compile_blueprint(pawn_blueprint)
require(
    pawn_blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
    f"Blueprint failed to compile before Acro input assignment: {PAWN_BLUEPRINT_PATH}",
)
pawn_cdo = unreal.get_default_object(pawn_blueprint.generated_class())
require(pawn_cdo is not None, "FPV integration Pawn CDO is unavailable")

created_actions = []
for action_name, pawn_property_name, key_specs in ACTION_SPECS:
    action_path = f"{ACTION_FOLDER}/{action_name}"
    action = unreal.load_asset(action_path)
    if action is None:
        action = asset_tools.create_asset(
            action_name,
            ACTION_FOLDER,
            unreal.InputAction,
            unreal.InputAction_Factory(),
        )
    require(action is not None, f"Could not create {action_path}")
    action.modify()
    action.set_editor_property("value_type", unreal.InputActionValueType.AXIS1D)
    require(
        editor_assets.save_loaded_asset(action, only_if_is_dirty=False),
        f"Could not save {action_path}",
    )
    created_actions.append((action, pawn_property_name, key_specs))

mapping_data = mapping_context.get_editor_property("default_key_mappings")
existing_mappings = list(mapping_data.get_editor_property("mappings"))
acro_actions = {action for action, _, _ in created_actions}
preserved_mappings = [
    mapping
    for mapping in existing_mappings
    if mapping.get_editor_property("action") not in acro_actions
]
for action, _, key_specs in created_actions:
    for key_name, negate in key_specs:
        preserved_mappings.append(make_mapping(mapping_context, action, key_name, negate))

new_mapping_data = unreal.InputMappingContextMappingData()
new_mapping_data.set_editor_property("mappings", preserved_mappings)
mapping_context.modify()
mapping_context.set_editor_property("default_key_mappings", new_mapping_data)
require(
    editor_assets.save_loaded_asset(mapping_context, only_if_is_dirty=False),
    f"Could not save {CONTEXT_PATH}",
)

pawn_blueprint.modify()
pawn_cdo.modify()
for action, pawn_property_name, _ in created_actions:
    pawn_cdo.set_editor_property(pawn_property_name, action)
unreal.BlueprintEditorLibrary.compile_blueprint(pawn_blueprint)
require(
    pawn_blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
    f"Blueprint failed to compile after Acro input assignment: {PAWN_BLUEPRINT_PATH}",
)
require(
    editor_assets.save_loaded_asset(pawn_blueprint, only_if_is_dirty=False),
    f"Could not save {PAWN_BLUEPRINT_PATH}",
)

saved_mapping_data = mapping_context.get_editor_property("default_key_mappings")
saved_mappings = list(saved_mapping_data.get_editor_property("mappings"))
for action, pawn_property_name, key_specs in created_actions:
    action_mappings = [
        mapping
        for mapping in saved_mappings
        if mapping.get_editor_property("action") == action
    ]
    require(
        len(action_mappings) == len(key_specs),
        f"{action.get_name()} has {len(action_mappings)} mappings; expected {len(key_specs)}",
    )
    require(
        pawn_cdo.get_editor_property(pawn_property_name) == action,
        f"Pawn CDO did not retain {pawn_property_name}",
    )

require(len(saved_mappings) == 33, f"Prototype IMC has {len(saved_mappings)} mappings; expected 33")
unreal.log(
    "DRONE_ACRO_INPUT|OK|"
    "keyboard=W/S Pitch,A/D Roll,Q/E Yaw,Space/Ctrl Throttle|"
    "gamepad=Mode1 LeftY Pitch/RightY Throttle,Mode2 LeftY Throttle/RightY Pitch,"
    "LeftX Yaw,RightX Roll|"
    f"mappings={len(saved_mappings)}"
)
