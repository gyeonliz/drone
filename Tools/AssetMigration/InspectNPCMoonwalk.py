"""Read-only armed NPC locomotion diagnostics for the moonwalk regression."""

import unreal


NPC_BLUEPRINTS = (
    "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Rifle",
    "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Shotgun",
    "/Game/Drone/AI/Blueprints/BP_NPC_Friendly_Base",
)
BLEND_SPACES = (
    "/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run",
    "/Game/Drone/AI/Animation/BS_NPC_Rifle_Locomotion",
)
ANIM_BLUEPRINTS = (
    "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed",
    "/Game/Drone/AI/Animation/ABP_NPC_Rifle_Greybox",
)


def load_required(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing asset: {path}")
    return asset


def log_npc(path):
    blueprint = load_required(path)
    cdo = unreal.get_default_object(blueprint.generated_class())
    mesh = cdo.get_editor_property("mesh")
    skeletal_mesh = mesh.get_editor_property("skeletal_mesh_asset")
    anim_class = mesh.get_editor_property("anim_class")
    unreal.log(
        "[DEBUG-MOONWALK] NPC "
        f"path={path} mesh={skeletal_mesh.get_path_name() if skeletal_mesh else 'None'} "
        f"anim={anim_class.get_path_name() if anim_class else 'None'} "
        f"relative_location={mesh.get_editor_property('relative_location')} "
        f"relative_rotation={mesh.get_editor_property('relative_rotation')} "
        f"relative_scale={mesh.get_editor_property('relative_scale3d')}"
    )


def log_blend_space(path):
    blend_space = load_required(path)
    unreal.log(f"[DEBUG-MOONWALK] BLEND path={path}")
    for index, sample in enumerate(blend_space.get_editor_property("sample_data")):
        animation = sample.get_editor_property("animation")
        value = sample.get_editor_property("sample_value")
        unreal.log(
            f"[DEBUG-MOONWALK] SAMPLE index={index} value=({value.x:.1f},{value.y:.1f}) "
            f"animation={animation.get_path_name() if animation else 'None'}"
        )


def log_anim_blueprint(path):
    blueprint = load_required(path)
    generated_class = blueprint.generated_class()
    anim_cdo = unreal.get_default_object(generated_class)
    matching_names = [
        name for name in dir(anim_cdo)
        if any(token in name.lower() for token in ("speed", "direction", "velocity", "move"))
    ]
    unreal.log(
        f"[DEBUG-MOONWALK] ANIM_BP path={path} class={generated_class.get_path_name()} "
        f"matching_names={matching_names}"
    )
    for node in blueprint.get_nodes_of_class(unreal.AnimGraphNode_BlendSpacePlayer):
        node_data = node.get_editor_property("node")
        blend_space = node_data.get_editor_property("blend_space")
        unreal.log(
            f"[DEBUG-MOONWALK] BLEND_NODE path={node.get_path_name()} "
            f"blend_space={blend_space.get_path_name() if blend_space else 'None'}"
        )


for npc_path in NPC_BLUEPRINTS:
    log_npc(npc_path)
for blend_space_path in BLEND_SPACES:
    log_blend_space(blend_space_path)
for anim_blueprint_path in ANIM_BLUEPRINTS:
    log_anim_blueprint(anim_blueprint_path)

unreal.log("[DEBUG-MOONWALK] DONE")
