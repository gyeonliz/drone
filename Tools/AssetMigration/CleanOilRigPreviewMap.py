"""Remove empty and exact duplicate StaticMeshActors from OilRig Preview."""

from __future__ import annotations

import json
import os

import unreal


MAP = "/Game/Drone/Maps/Lvl_OilRigPreview"
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
if world is None:
    raise RuntimeError(f"Failed to load {MAP}")


def rounded(value: float) -> float:
    return round(float(value), 4)


actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
null_mesh_actors = []
duplicate_actors = []
seen = {}
for actor in actor_subsystem.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    component = actor.get_editor_property("static_mesh_component")
    mesh = component.get_editor_property("static_mesh")
    if mesh is None:
        null_mesh_actors.append(actor)
        continue

    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    key = (
        mesh.get_path_name(),
        rounded(location.x),
        rounded(location.y),
        rounded(location.z),
        rounded(rotation.pitch),
        rounded(rotation.yaw),
        rounded(rotation.roll),
        rounded(scale.x),
        rounded(scale.y),
        rounded(scale.z),
    )
    if key in seen:
        duplicate_actors.append(actor)
    else:
        seen[key] = actor

removed_null = [actor.get_actor_label() for actor in null_mesh_actors]
removed_duplicates = [actor.get_actor_label() for actor in duplicate_actors]
actors_to_remove = null_mesh_actors + duplicate_actors
if actors_to_remove and not actor_subsystem.destroy_actors(actors_to_remove):
    raise RuntimeError("Failed to remove invalid OilRig Preview actors")

if not unreal.EditorAssetLibrary.save_asset(MAP, only_if_is_dirty=False):
    raise RuntimeError(f"Failed to save cleaned map: {MAP}")

unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
result = {
    "map": MAP,
    "removed_null_mesh_actor_count": len(removed_null),
    "removed_null_mesh_actors": removed_null,
    "removed_exact_duplicate_count": len(removed_duplicates),
    "removed_exact_duplicates": removed_duplicates,
}
report_dir = os.path.join(unreal.Paths.project_saved_dir(), "AssetMigration")
os.makedirs(report_dir, exist_ok=True)
report_path = os.path.join(report_dir, "oilrig_preview_cleanup.json")
with open(report_path, "w", encoding="utf-8") as report_file:
    json.dump(result, report_file, ensure_ascii=False, indent=2)
unreal.log(json.dumps(result, ensure_ascii=False, indent=2))
