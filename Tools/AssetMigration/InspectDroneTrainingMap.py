"""Read-only actor/transform inventory for the project-owned Training map."""

import unreal


MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if not world:
    raise RuntimeError(f"Training Map을 불러오지 못했습니다: {MAP_PATH}")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in sorted(actor_subsystem.get_all_level_actors(), key=lambda item: item.get_actor_label()):
    unreal.log(
        "DRONE_TRAINING_ACTOR|label={} | class={} | location={} | rotation={} | scale={}".format(
            actor.get_actor_label(),
            actor.get_class().get_path_name(),
            actor.get_actor_location(),
            actor.get_actor_rotation(),
            actor.get_actor_scale3d(),
        )
    )
