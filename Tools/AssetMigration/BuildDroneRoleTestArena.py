"""Place one explicit target for each Drone role in the project-owned Training map."""

import unreal


MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"
ROLE_TARGET_FOLDER = "/Game/Drone/Abilities/RoleTargets"
TARGETS = [
    ("RoleTest_ReconTarget", "BP_RoleTest_ReconTarget", "/Script/Drone.DroneReconRoleTestTarget", unreal.Vector(900, 900, 300), unreal.Rotator(pitch=0, yaw=-90, roll=0)),
    ("RoleTest_ImpactTarget", "BP_RoleTest_ImpactTarget", "/Script/Drone.DroneImpactRoleTestTarget", unreal.Vector(1700, 900, 300), unreal.Rotator(pitch=0, yaw=-90, roll=0)),
    ("RoleTest_PayloadTarget", "BP_RoleTest_PayloadTarget", "/Script/Drone.DronePayloadRoleTestTarget", unreal.Vector(1000, -900, 15), unreal.Rotator(pitch=0, yaw=90, roll=0)),
]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load_or_create_target_blueprint(asset_name, parent_class):
    asset_path = f"{ROLE_TARGET_FOLDER}/{asset_name}"
    blueprint = unreal.load_asset(asset_path)
    if blueprint is None:
        unreal.EditorAssetLibrary.make_directory(ROLE_TARGET_FOLDER)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            ROLE_TARGET_FOLDER,
            unreal.Blueprint,
            factory,
        )
    require(blueprint is not None, f"역할 표적 Blueprint 생성 실패: {asset_path}")
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == parent_class,
        f"역할 표적 Blueprint Parent 불일치: {asset_path}",
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(
        blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
        f"역할 표적 Blueprint Compile 실패: {asset_path}",
    )
    require(
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False),
        f"역할 표적 Blueprint 저장 실패: {asset_path}",
    )
    return blueprint


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if not world:
    raise RuntimeError(f"Training Map을 불러오지 못했습니다: {MAP_PATH}")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
target_labels = {entry[0] for entry in TARGETS}
for actor in actor_subsystem.get_all_level_actors():
    if actor.get_actor_label() in target_labels:
        actor_subsystem.destroy_actor(actor)

for label, asset_name, parent_class_path, location, rotation in TARGETS:
    parent_class = unreal.load_class(None, parent_class_path)
    require(parent_class is not None, f"역할 표적 Native Parent를 찾지 못했습니다: {parent_class_path}")
    blueprint = load_or_create_target_blueprint(asset_name, parent_class)
    actor = actor_subsystem.spawn_actor_from_class(blueprint.generated_class(), location, rotation)
    if not actor:
        raise RuntimeError(f"역할 시험 표적 Spawn 실패: {label}")
    actor.set_actor_label(label)
    unreal.log(
        f"DRONE_ROLE_TARGET|label={label}|blueprint={blueprint.get_path_name()}|"
        f"class={actor.get_class().get_path_name()}|location={location}"
    )

if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    raise RuntimeError("Training Map 저장 실패")
