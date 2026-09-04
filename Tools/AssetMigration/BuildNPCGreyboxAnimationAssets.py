"""Build project-owned armed NPC animation assets from the UE mannequin samples.

This script is intentionally idempotent. It keeps the engine/sample assets untouched,
updates only /Game/Drone/AI/Animation, and assigns the armed AnimBP only to hostile
rifle/shotgun NPC Blueprints.
"""

import unreal


SOURCE_BLEND_SPACE = "/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run"
SOURCE_ANIM_BLUEPRINT = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"

TARGET_BLEND_SPACE = "/Game/Drone/AI/Animation/BS_NPC_Rifle_Locomotion"
TARGET_ANIM_BLUEPRINT = "/Game/Drone/AI/Animation/ABP_NPC_Rifle_Greybox"

RIFLE_IDLE = "/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"
ARMED_NPC_BLUEPRINTS = (
    "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Rifle",
    "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Shotgun",
)
FRIENDLY_NPC_BLUEPRINT = "/Game/Drone/AI/Blueprints/BP_NPC_Friendly_Base"
UNARMED_ANIM_BLUEPRINT = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"


def load_required(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        raise RuntimeError(f"Required asset is missing: {asset_path}")
    return asset


def ensure_duplicate(source_path, target_path):
    existing = unreal.EditorAssetLibrary.load_asset(target_path)
    if existing:
        return existing

    duplicated = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
    if not duplicated:
        raise RuntimeError(f"Could not duplicate {source_path} to {target_path}")
    return duplicated


def rifle_variant_path(source_animation, speed):
    if speed <= 1.0:
        return RIFLE_IDLE

    source_path = source_animation.get_path_name().split(".", 1)[0]
    return source_path.replace("/Unarmed/", "/Rifle/").replace("_Unarmed_", "_Rifle_")


def update_rifle_blend_space():
    blend_space = ensure_duplicate(SOURCE_BLEND_SPACE, TARGET_BLEND_SPACE)
    samples = blend_space.get_editor_property("sample_data")
    updated_samples = []

    for sample in samples:
        sample_value = sample.get_editor_property("sample_value")
        source_animation = sample.get_editor_property("animation")
        target_path = rifle_variant_path(source_animation, sample_value.y)
        target_animation = load_required(target_path)
        # Unreal의 배열 안 UStruct 래퍼는 값 복사다. 원소를 제자리에서 바꾸면 저장 시
        # 사라질 수 있으므로 구조체를 복제한 새 배열을 만들어 Asset에 다시 지정한다.
        updated_sample = sample.copy()
        updated_sample.set_editor_property("animation", target_animation)
        updated_samples.append(updated_sample)

    blend_space.modify()
    blend_space.set_editor_property("sample_data", updated_samples)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blend_space, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {TARGET_BLEND_SPACE}")
    return blend_space


def update_rifle_anim_blueprint(blend_space):
    anim_blueprint = ensure_duplicate(SOURCE_ANIM_BLUEPRINT, TARGET_ANIM_BLUEPRINT)

    blend_nodes = anim_blueprint.get_nodes_of_class(unreal.AnimGraphNode_BlendSpacePlayer)
    if len(blend_nodes) != 1:
        raise RuntimeError(f"Expected one locomotion BlendSpace node, found {len(blend_nodes)}")

    blend_node = blend_nodes[0]
    blend_node_struct = blend_node.get_editor_property("node")
    blend_node_struct.set_editor_property("blend_space", blend_space)
    blend_node.modify()
    blend_node.set_editor_property("node", blend_node_struct)

    idle_animation = load_required(RIFLE_IDLE)
    jump_replacements = {
        "MM_Jump": "/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Start",
        "MM_Fall_Loop": "/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Loop",
        "MM_Land": "/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Land",
    }

    for sequence_node in anim_blueprint.get_nodes_of_class(unreal.AnimGraphNode_SequencePlayer):
        node_struct = sequence_node.get_editor_property("node")
        current_sequence = node_struct.get_editor_property("sequence")
        if not current_sequence:
            continue

        state_path = sequence_node.get_path_name()
        replacement = None
        if ".Idle." in state_path:
            replacement = idle_animation
        else:
            replacement_path = jump_replacements.get(current_sequence.get_name())
            if replacement_path:
                replacement = load_required(replacement_path)

        if replacement:
            node_struct.set_editor_property("sequence", replacement)
            sequence_node.modify()
            sequence_node.set_editor_property("node", node_struct)

    if not unreal.DroneNPCAnimationAuthoringLibrary.upgrade_rifle_anim_blueprint_for_drone_gaze(
        TARGET_ANIM_BLUEPRINT
    ):
        raise RuntimeError("Could not add the project-owned Drone gaze graph")

    unreal.BlueprintEditorLibrary.compile_blueprint(anim_blueprint)
    if anim_blueprint.status == unreal.BlueprintStatus.BS_ERROR:
        raise RuntimeError(f"Compile failed for {TARGET_ANIM_BLUEPRINT}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(anim_blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {TARGET_ANIM_BLUEPRINT}")
    return anim_blueprint


def assign_anim_blueprint(character_blueprint_path, anim_blueprint):
    character_blueprint = load_required(character_blueprint_path)
    character_class = character_blueprint.generated_class()
    character_cdo = unreal.get_default_object(character_class)
    mesh = character_cdo.get_editor_property("mesh")
    if not mesh:
        raise RuntimeError(f"Mesh component is missing on {character_blueprint_path}")

    mesh.modify()
    mesh.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    mesh.set_editor_property("anim_class", anim_blueprint.generated_class())
    character_cdo.modify()
    character_blueprint.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(character_blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {character_blueprint_path}")


def verify_assets(blend_space, anim_blueprint):
    samples = blend_space.get_editor_property("sample_data")
    if len(samples) != 27:
        raise RuntimeError(f"Expected 27 locomotion samples, found {len(samples)}")
    for sample in samples:
        animation = sample.get_editor_property("animation")
        if not animation or "/Anims/Rifle/" not in animation.get_path_name():
            raise RuntimeError(f"Non-rifle locomotion sample remains: {animation}")

    blend_nodes = anim_blueprint.get_nodes_of_class(unreal.AnimGraphNode_BlendSpacePlayer)
    if len(blend_nodes) != 1:
        raise RuntimeError("Armed AnimBP locomotion node count changed")
    active_blend_space = blend_nodes[0].get_editor_property("node").get_editor_property("blend_space")
    if active_blend_space != blend_space:
        raise RuntimeError("Armed AnimBP does not reference the project Rifle BlendSpace")

    if not unreal.DroneNPCAnimationAuthoringLibrary.validate_rifle_anim_blueprint_drone_gaze(
        TARGET_ANIM_BLUEPRINT
    ):
        raise RuntimeError("Armed AnimBP Drone gaze graph is missing or invalid")

    armed_anim_class = anim_blueprint.generated_class()
    for character_blueprint_path in ARMED_NPC_BLUEPRINTS:
        character_blueprint = load_required(character_blueprint_path)
        character_cdo = unreal.get_default_object(character_blueprint.generated_class())
        if character_cdo.get_editor_property("mesh").get_editor_property("anim_class") != armed_anim_class:
            raise RuntimeError(f"Armed AnimBP assignment was not saved on {character_blueprint_path}")

    friendly_blueprint = load_required(FRIENDLY_NPC_BLUEPRINT)
    friendly_cdo = unreal.get_default_object(friendly_blueprint.generated_class())
    unarmed_anim_class = load_required(UNARMED_ANIM_BLUEPRINT).generated_class()
    if friendly_cdo.get_editor_property("mesh").get_editor_property("anim_class") != unarmed_anim_class:
        raise RuntimeError("Friendly NPC must keep the Unarmed AnimBP")


def main():
    blend_space = update_rifle_blend_space()
    anim_blueprint = update_rifle_anim_blueprint(blend_space)
    for character_blueprint_path in ARMED_NPC_BLUEPRINTS:
        assign_anim_blueprint(character_blueprint_path, anim_blueprint)

    verify_assets(blend_space, anim_blueprint)

    unreal.log(
        "NPC_GREYBOX_ANIM success: rifle locomotion and smoothed spine/neck/head Drone gaze assigned"
    )


main()
