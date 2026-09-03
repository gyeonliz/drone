"""Read-only validation for the saved project-owned armed NPC animation setup."""

import unreal


TARGET_BLEND_SPACE = "/Game/Drone/AI/Animation/BS_NPC_Rifle_Locomotion"
TARGET_ANIM_BLUEPRINT = "/Game/Drone/AI/Animation/ABP_NPC_Rifle_Greybox"
RIFLE_IDLE = "/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"
UNARMED_ANIM_BLUEPRINT = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"
ARMED_NPCS = (
    "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Rifle",
    "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Shotgun",
)
FRIENDLY_NPC = "/Game/Drone/AI/Blueprints/BP_NPC_Friendly_Base"


def load_required(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        raise RuntimeError(f"Missing asset: {asset_path}")
    return asset


blend_space = load_required(TARGET_BLEND_SPACE)
samples = blend_space.get_editor_property("sample_data")
if len(samples) != 27:
    raise RuntimeError(f"Expected 27 Rifle BlendSpace samples, found {len(samples)}")
if any(
    "/Anims/Rifle/" not in sample.get_editor_property("animation").get_path_name()
    for sample in samples
):
    raise RuntimeError("Saved Rifle BlendSpace still contains an Unarmed locomotion sample")

anim_blueprint = load_required(TARGET_ANIM_BLUEPRINT)
if anim_blueprint.status == unreal.BlueprintStatus.BS_ERROR:
    raise RuntimeError("Saved armed AnimBP has a compile error")

blend_nodes = anim_blueprint.get_nodes_of_class(unreal.AnimGraphNode_BlendSpacePlayer)
if len(blend_nodes) != 1:
    raise RuntimeError(f"Expected one armed locomotion node, found {len(blend_nodes)}")
saved_blend_space = blend_nodes[0].get_editor_property("node").get_editor_property("blend_space")
if saved_blend_space != blend_space:
    raise RuntimeError("Saved armed AnimBP does not use the Rifle BlendSpace")

idle_nodes = [
    node
    for node in anim_blueprint.get_nodes_of_class(unreal.AnimGraphNode_SequencePlayer)
    if ".Idle." in node.get_path_name()
]
if len(idle_nodes) != 1:
    raise RuntimeError(f"Expected one armed Idle node, found {len(idle_nodes)}")
idle_sequence = idle_nodes[0].get_editor_property("node").get_editor_property("sequence")
if idle_sequence.get_path_name().split(".", 1)[0] != RIFLE_IDLE:
    raise RuntimeError(f"Saved armed Idle node uses the wrong sequence: {idle_sequence}")

armed_class = anim_blueprint.generated_class()
for npc_path in ARMED_NPCS:
    npc_blueprint = load_required(npc_path)
    npc_cdo = unreal.get_default_object(npc_blueprint.generated_class())
    if npc_cdo.get_editor_property("mesh").get_editor_property("anim_class") != armed_class:
        raise RuntimeError(f"Saved hostile NPC does not use armed AnimBP: {npc_path}")

friendly_blueprint = load_required(FRIENDLY_NPC)
friendly_cdo = unreal.get_default_object(friendly_blueprint.generated_class())
unarmed_class = load_required(UNARMED_ANIM_BLUEPRINT).generated_class()
if friendly_cdo.get_editor_property("mesh").get_editor_property("anim_class") != unarmed_class:
    raise RuntimeError("Friendly NPC no longer uses the Unarmed AnimBP")

unreal.log(
    "NPC_GREYBOX_ANIM_VERIFY success: saved Rifle idle + 27 Rifle locomotion samples + hostile/friendly role split"
)
