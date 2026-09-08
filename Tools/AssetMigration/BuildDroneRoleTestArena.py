"""Place one explicit target for each Drone role in the project-owned Training map."""

import unreal


MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"
TARGETS = [
    ("RoleTest_ReconTarget", unreal.DroneReconRoleTestTarget, unreal.Vector(900, 900, 300), unreal.Rotator(pitch=0, yaw=-90, roll=0)),
    ("RoleTest_ImpactTarget", unreal.DroneImpactRoleTestTarget, unreal.Vector(1700, 900, 300), unreal.Rotator(pitch=0, yaw=-90, roll=0)),
    ("RoleTest_PayloadTarget", unreal.DronePayloadRoleTestTarget, unreal.Vector(1000, -900, 15), unreal.Rotator(pitch=0, yaw=90, roll=0)),
]


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if not world:
    raise RuntimeError(f"Training Map을 불러오지 못했습니다: {MAP_PATH}")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
target_labels = {entry[0] for entry in TARGETS}
for actor in actor_subsystem.get_all_level_actors():
    if actor.get_actor_label() in target_labels:
        actor_subsystem.destroy_actor(actor)

for label, actor_class, location, rotation in TARGETS:
    actor = actor_subsystem.spawn_actor_from_class(actor_class, location, rotation)
    if not actor:
        raise RuntimeError(f"역할 시험 표적 Spawn 실패: {label}")
    actor.set_actor_label(label)
    unreal.log(f"DRONE_ROLE_TARGET|label={label}|class={actor.get_class().get_path_name()}|location={location}")

if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    raise RuntimeError("Training Map 저장 실패")
