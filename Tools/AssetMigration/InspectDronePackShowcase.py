"""Read-only StaticMesh actor inventory for the project-owned Drone Pack showcase."""

import unreal


MAP_PATH = "/Game/Drone/Maps/Lvl_DronePackShowcase"
ROLE_MARKERS = ("Baba", "Delivery", "Drone01", "POLICE", "DroneT", "Quad", "Sting", "FPV", "Rotor")


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if not world:
    raise RuntimeError(f"Drone Pack Showcase를 불러오지 못했습니다: {MAP_PATH}")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in sorted(actor_subsystem.get_all_level_actors(), key=lambda item: item.get_actor_label()):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh = component.get_editor_property("static_mesh") if component else None
    mesh_path = mesh.get_path_name() if mesh else "None"
    if mesh and any(marker.lower() in mesh_path.lower() for marker in ROLE_MARKERS):
        unreal.log(
            "DRONE_SHOWCASE_MESH|label={} | mesh={} | location={} | rotation={} | scale={}".format(
                actor.get_actor_label(),
                mesh_path,
                actor.get_actor_location(),
                actor.get_actor_rotation(),
                actor.get_actor_scale3d(),
            )
        )
