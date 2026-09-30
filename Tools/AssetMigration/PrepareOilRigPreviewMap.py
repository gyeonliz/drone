"""Create a cleaned, project-owned copy of the OilRig Preview map in staging."""

from __future__ import annotations

import json
import os

import unreal


SOURCE_MAP = "/Game/Liope_Tr/Maps/Preview"
TARGET_MAP = "/Game/Drone/Maps/Lvl_OilRigPreview"


if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP):
    raise RuntimeError(f"Missing OilRig Preview source map: {SOURCE_MAP}")

unreal.EditorAssetLibrary.make_directory("/Game/Drone/Maps")
if unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP):
    preview_map = unreal.EditorAssetLibrary.load_asset(TARGET_MAP)
else:
    preview_map = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, TARGET_MAP)
if preview_map is None:
    raise RuntimeError(f"Failed to duplicate OilRig Preview map: {SOURCE_MAP}")

if not unreal.EditorAssetLibrary.save_asset(TARGET_MAP, only_if_is_dirty=False):
    raise RuntimeError(f"Failed to save OilRig Preview map copy: {TARGET_MAP}")

preview_map = None
unreal.SystemLibrary.collect_garbage()
world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
if world is None:
    raise RuntimeError(f"Failed to load OilRig Preview map copy: {TARGET_MAP}")

# Keep the location only. Drone's own GameMode must continue to own pawn,
# controller, input, and UI behavior.
world.get_world_settings().set_editor_property("default_game_mode", None)

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sample_actors = []
door_actors = []
for actor in actor_subsystem.get_all_level_actors():
    class_path = actor.get_class().get_path_name()
    is_vendor_demo = "/Demo/" in class_path
    is_first_person_sample = "/FirstPerson/" in class_path
    if is_vendor_demo or is_first_person_sample:
        sample_actors.append(actor)
    elif "BP_Simple_Door" in class_path:
        door_actors.append(actor)

# The sample doors cast to the vendor FirstPersonCharacter and pull a second
# pawn/input/arms stack into Drone.  Preserve every visible door and frame mesh
# as a plain StaticMeshActor, then remove only the gameplay Blueprint wrapper.
flattened_door_meshes = []
for door_actor in door_actors:
    door_label = door_actor.get_actor_label()
    for component in door_actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
        if mesh is None or not component.get_editor_property("visible"):
            continue
        transform = component.get_world_transform()
        static_actor = actor_subsystem.spawn_actor_from_class(
            unreal.StaticMeshActor, transform.translation, unreal.Rotator()
        )
        if static_actor is None:
            raise RuntimeError(f"Failed to flatten door component: {door_label}")
        static_actor.set_actor_transform(transform, sweep=False, teleport=True)
        static_actor.set_actor_label(
            f"{door_label}_{component.get_name()}_Static", mark_dirty=False
        )
        static_component = static_actor.get_editor_property("static_mesh_component")
        static_component.set_static_mesh(mesh)
        for material_index in range(component.get_num_materials()):
            material = component.get_material(material_index)
            if material is not None:
                static_component.set_material(material_index, material)
        flattened_door_meshes.append(
            {
                "source_actor": door_label,
                "source_component": component.get_name(),
                "mesh": mesh.get_path_name(),
                "actor": static_actor.get_actor_label(),
            }
        )

removed_actor_labels = [actor.get_actor_label() for actor in sample_actors]
removed_actor_classes = [actor.get_class().get_path_name() for actor in sample_actors]
actors_to_remove = sample_actors + door_actors
if actors_to_remove and not actor_subsystem.destroy_actors(actors_to_remove):
    raise RuntimeError("Failed to remove vendor gameplay actors from OilRig Preview")

if not unreal.EditorAssetLibrary.save_asset(TARGET_MAP, only_if_is_dirty=False):
    raise RuntimeError(f"Failed to save cleaned OilRig Preview map: {TARGET_MAP}")

result = {
    "source_map": SOURCE_MAP,
    "target_map": TARGET_MAP,
    "default_game_mode_cleared": True,
    "removed_sample_actor_count": len(sample_actors),
    "removed_sample_actor_labels": removed_actor_labels,
    "removed_sample_actor_classes": removed_actor_classes,
    "flattened_door_actor_count": len(door_actors),
    "flattened_door_mesh_count": len(flattened_door_meshes),
    "flattened_door_meshes": flattened_door_meshes,
}
report_dir = os.path.join(unreal.Paths.project_saved_dir(), "AssetMigration")
os.makedirs(report_dir, exist_ok=True)
report_path = os.path.join(report_dir, "oilrig_preview_prepare_report.json")
with open(report_path, "w", encoding="utf-8") as report_file:
    json.dump(result, report_file, ensure_ascii=False, indent=2)

unreal.log(f"DRONE_OILRIG_PREVIEW_PREPARE={report_path}")
unreal.log(json.dumps(result, ensure_ascii=False, indent=2))
