"""Read-only scene/effect audit. Does not save a map or a marketplace asset."""
import collections
import json
import os
import traceback
import unreal

PREFIX = "DRONE_GAME_AUDIT"
ROOT = os.path.join(unreal.Paths.project_saved_dir(), "Automation", "GameReadiness")

def value(obj, name):
    try:
        result = obj.get_editor_property(name)
        if isinstance(result, unreal.Object):
            return result.get_path_name()
        return str(result)
    except Exception:
        return "not exposed"

def audit_map(path):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.load_level(path):
        raise RuntimeError(f"Cannot load {path}")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    classes = collections.Counter(a.get_class().get_name() for a in actors)
    effects, materials, meshes = [], set(), collections.Counter()
    for actor in actors:
        for component in actor.get_components_by_class(unreal.ActorComponent):
            if isinstance(component, unreal.NiagaraComponent):
                system = component.get_asset()
                effects.append({"actor": actor.get_actor_label(), "component": component.get_name(),
                    "location": str(actor.get_actor_location()), "system": system.get_path_name() if system else None,
                    "active": component.is_active(), "auto_activate": value(component, "auto_activate"),
                    "cast_shadow": value(component, "cast_shadow"),
                    "bounds_scale": value(component, "bounds_scale"),
                    "max_draw_distance": value(component, "ld_max_draw_distance")})
            if isinstance(component, unreal.MeshComponent):
                for index in range(component.get_num_materials()):
                    material = component.get_material(index)
                    if material:
                        materials.add(material.get_path_name())
            if isinstance(component, unreal.StaticMeshComponent):
                mesh = component.get_editor_property("static_mesh")
                if mesh:
                    meshes[mesh.get_path_name()] += 1
    return {"map": path, "actors": len(actors), "classes": dict(classes), "effects": effects,
            "materials": sorted(materials), "mesh_components": sum(meshes.values()),
            "repeated_meshes": meshes.most_common(15)}

def export_system(path):
    asset = unreal.load_asset(path)
    if asset is None:
        return {"path": path, "missing": True}
    result = {"path": path, "properties": {name: value(asset, name) for name in
        ("fixed_bounds", "fixed_bounds_enabled", "effect_type", "override_scalability_settings",
         "scalability_overrides", "warmup_time", "warmup_tick_count", "determinism", "emitter_handles")}}
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(ROOT, asset.get_name() + ".copy")
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    result["exported"] = unreal.Exporter.run_asset_export_task(task)
    return result

def main():
    os.makedirs(ROOT, exist_ok=True)
    maps = [audit_map(p) for p in ("/Game/Drone/Maps/Lvl_OilRig", "/Game/Drone/Maps/Lvl_OilRigPreview")]
    systems = [export_system("/Game/Drone/ThirdParty/OilRigPreview/Rain/Niagara/" + name)
        for name in ("NS_sky_Rain", "NS_Rain_Slow", "NS_Rain_Fast")]
    material_results = []
    for path in sorted(set(p for m in maps for p in m["materials"])):
        if any(word in path.lower() for word in ("rain", "water", "wet", "puddle")):
            asset = unreal.load_asset(path)
            base = asset.get_base_material() if isinstance(asset, unreal.MaterialInterface) else asset
            material_results.append({"path": path, "base": base.get_path_name(), "blend": value(base, "blend_mode"),
                "two_sided": value(base, "two_sided"), "shading_model": value(base, "shading_model"),
                "parent": value(asset, "parent"), "scalar_parameters": value(asset, "scalar_parameter_values")})
    with open(os.path.join(ROOT, "oilrig_audit.json"), "w", encoding="utf-8") as handle:
        json.dump({"maps": maps, "systems": systems, "water_materials": material_results}, handle, ensure_ascii=False, indent=2)
    for m in maps:
        unreal.log(f"{PREFIX}|MAP|{m['map']}|actors={m['actors']}|meshes={m['mesh_components']}|niagara={len(m['effects'])}")
    unreal.log(f"{PREFIX}|SUCCESS|{ROOT}")

if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error(traceback.format_exc())
        raise
