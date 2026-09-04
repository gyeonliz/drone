"""Read-only diagnostic for the project-owned FPV Drone component attachment hierarchy."""

import unreal


PAWN_CLASS_PATH = "/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration.BP_DroneFPVIntegration_C"


pawn_class = unreal.load_class(None, PAWN_CLASS_PATH)
if pawn_class is None:
    raise RuntimeError(f"Missing Drone integration Class: {PAWN_CLASS_PATH}")

cdo = unreal.get_default_object(pawn_class)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
spawned = actor_subsystem.spawn_actor_from_class(pawn_class, unreal.Vector(), unreal.Rotator())
if spawned is None:
    raise RuntimeError("Could not spawn FPV integration for hierarchy inspection")

components = spawned.get_components_by_class(unreal.StaticMeshComponent)
for component in components:
    parent = component.get_attach_parent()
    unreal.log(
        "DRONE_VISUAL_HIERARCHY|component={} | parent={} | mesh={}".format(
            component.get_name(),
            parent.get_name() if parent else "None",
            component.get_editor_property("static_mesh").get_path_name()
            if component.get_editor_property("static_mesh")
            else "None",
        )
    )

actor_subsystem.destroy_actor(spawned)
