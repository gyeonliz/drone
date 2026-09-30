"""Validate the migrated OilRig Preview environment in staging or Drone."""

from __future__ import annotations

import json
import os
from collections import deque

import unreal


MAP_PACKAGE = "/Game/Drone/Maps/Lvl_OilRigPreview"
ASSET_ROOT = "/Game/Drone/ThirdParty/OilRigPreview"


def dependency_options():
    return unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False,
    )


registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(
    ["/Game/Drone", "/Game/Liope_Tr", "/Game/FirstPerson"], force_rescan=True
)
registry.wait_for_completion()

registry_only = os.environ.get("DRONE_OILRIG_PREVIEW_REGISTRY_ONLY", "0") == "1"
world = None
if not registry_only:
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PACKAGE)
    if world is None:
        raise RuntimeError(f"Migrated OilRig Preview map does not open: {MAP_PACKAGE}")

options = dependency_options()
visited = set()
queue = deque([MAP_PACKAGE])
while queue:
    package = queue.popleft()
    if package in visited:
        continue
    visited.add(package)
    for dependency in registry.get_dependencies(package, options) or []:
        dependency_name = str(dependency)
        if dependency_name.startswith("/Game/") and dependency_name not in visited:
            queue.append(dependency_name)

external_game_dependencies = sorted(
    package
    for package in visited
    if package.startswith("/Game/") and not package.startswith("/Game/Drone/")
)
missing_game_dependencies = sorted(
    package
    for package in visited
    if package.startswith("/Game/") and not registry.get_assets_by_package_name(package, True)
)
assets = registry.get_assets_by_path(ASSET_ROOT, recursive=True)
world_settings = world.get_world_settings() if world else None
if world:
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
result = {
    "map": MAP_PACKAGE,
    "map_loaded": world is not None,
    "registry_only": registry_only,
    "asset_root": ASSET_ROOT,
    "asset_count": len(assets),
    "dependency_closure_count": len(visited),
    "external_game_dependencies": external_game_dependencies,
    "missing_game_dependencies": missing_game_dependencies,
    "default_game_mode": (
        world_settings.get_editor_property("default_game_mode").get_path_name()
        if world_settings and world_settings.get_editor_property("default_game_mode")
        else None
    ),
    "map_check_executed": world is not None,
}

report_dir = os.path.join(unreal.Paths.project_saved_dir(), "AssetMigration")
os.makedirs(report_dir, exist_ok=True)
report_path = os.path.join(report_dir, "oilrig_preview_migration_audit.json")
with open(report_path, "w", encoding="utf-8") as report_file:
    json.dump(result, report_file, ensure_ascii=False, indent=2)

unreal.log("DRONE_OILRIG_PREVIEW_AUDIT_BEGIN")
unreal.log(json.dumps(result, ensure_ascii=False, indent=2))
unreal.log("DRONE_OILRIG_PREVIEW_AUDIT_END")

if not assets:
    raise RuntimeError("OilRig Preview target root has no assets")
if external_game_dependencies or missing_game_dependencies:
    raise RuntimeError("OilRig Preview has external or missing /Game dependencies")
if not registry_only and result["default_game_mode"] is not None:
    raise RuntimeError("OilRig Preview still overrides the project GameMode")
