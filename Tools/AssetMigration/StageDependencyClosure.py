"""Move a vendor asset and its Unreal dependency closure into a project-owned root.

This script is intentionally run only in a disposable staging project.  The source
Marketplace files stay untouched.  Configure it with environment variables:

* DRONE_STAGE_SEEDS: JSON array of /Game package names.
* DRONE_STAGE_TARGET_ROOT: destination such as
  /Game/Drone/ThirdParty/ArmyVFX.
* DRONE_STAGE_SOURCE_ROOT: vendor mount root such as /Game/ArmyVFX.  This
  prefix is stripped so the target does not become ArmyVFX/ArmyVFX.
* DRONE_STAGE_KEEP_PREFIXES: optional JSON array of project-owned prefixes that
  must not be moved (for example /Game/Drone/Maps for a cleaned map copy).
* DRONE_STAGE_REPORT_NAME: optional report file name.
"""

from __future__ import annotations

import json
import os
from collections import deque

import unreal


def read_json_array(name: str, default: list[str] | None = None) -> list[str]:
    raw_value = os.environ.get(name)
    if not raw_value:
        return list(default or [])
    value = json.loads(raw_value)
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise RuntimeError(f"{name} must be a JSON array of strings")
    return value


def dependency_options() -> unreal.AssetRegistryDependencyOptions:
    return unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False,
    )


def dependency_closure(registry, seeds: list[str], options) -> set[str]:
    visited: set[str] = set()
    queue = deque(seeds)
    while queue:
        package_name = queue.popleft()
        if package_name in visited:
            continue
        visited.add(package_name)
        for dependency in registry.get_dependencies(package_name, options) or []:
            dependency_name = str(dependency)
            if dependency_name.startswith("/Game/") and dependency_name not in visited:
                queue.append(dependency_name)
    return visited


def is_under(package_name: str, prefix: str) -> bool:
    return package_name == prefix or package_name.startswith(prefix.rstrip("/") + "/")


def target_package_name(
    source_package: str, source_root: str, target_root: str
) -> str:
    if not is_under(source_package, source_root):
        raise RuntimeError(
            f"Package is outside DRONE_STAGE_SOURCE_ROOT: {source_package}"
        )
    relative_package = source_package[len(source_root) :].lstrip("/")
    if not relative_package:
        raise RuntimeError(f"Cannot move a mount root without an asset name: {source_package}")
    return target_root.rstrip("/") + "/" + relative_package


seeds = read_json_array("DRONE_STAGE_SEEDS")
if not seeds:
    raise RuntimeError("DRONE_STAGE_SEEDS is empty")

target_root = os.environ.get("DRONE_STAGE_TARGET_ROOT", "").rstrip("/")
if not target_root.startswith("/Game/Drone/ThirdParty/"):
    raise RuntimeError("DRONE_STAGE_TARGET_ROOT must be under /Game/Drone/ThirdParty")

source_root = os.environ.get("DRONE_STAGE_SOURCE_ROOT", "").rstrip("/")
if not source_root.startswith("/Game/"):
    raise RuntimeError("DRONE_STAGE_SOURCE_ROOT must be a vendor root under /Game")

keep_prefixes = read_json_array("DRONE_STAGE_KEEP_PREFIXES", ["/Game/Drone"])
report_name = os.environ.get("DRONE_STAGE_REPORT_NAME", "dependency_stage_report.json")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
options = dependency_options()

missing_seeds = [
    package_name
    for package_name in seeds
    if not registry.get_assets_by_package_name(package_name, True)
]
if missing_seeds:
    raise RuntimeError(f"Seed packages are missing: {missing_seeds}")

source_closure = dependency_closure(registry, seeds, options)
packages_to_move = sorted(
    package_name
    for package_name in source_closure
    if is_under(package_name, source_root)
    and not any(is_under(package_name, prefix) for prefix in keep_prefixes)
)

rename_data = []
package_map: dict[str, str] = {}
unavailable_packages = []
for package_name in packages_to_move:
    asset_data_list = registry.get_assets_by_package_name(package_name, True)
    if not asset_data_list:
        unavailable_packages.append(package_name)
        continue

    destination_package = target_package_name(package_name, source_root, target_root)
    destination_path, _, destination_leaf = destination_package.rpartition("/")
    package_map[package_name] = destination_package

    for asset_data in asset_data_list:
        asset = asset_data.get_asset()
        if asset is None:
            unavailable_packages.append(package_name)
            continue
        # Most Unreal packages contain one asset whose name matches the package leaf.
        # Preserve a differing asset name, while keeping the destination directory.
        new_name = str(asset_data.asset_name)
        if new_name == str(asset_data.package_name).rsplit("/", 1)[-1]:
            new_name = destination_leaf
        rename_data.append(unreal.AssetRenameData(asset, destination_path, new_name))

if unavailable_packages:
    raise RuntimeError(
        "Dependency packages could not be loaded in staging: "
        + json.dumps(sorted(set(unavailable_packages)), ensure_ascii=False)
    )

unreal.EditorAssetLibrary.make_directory(target_root)
if rename_data:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    if not asset_tools.rename_assets(rename_data):
        raise RuntimeError("Unreal failed to rename the dependency closure")

moved_seeds = [package_map.get(package_name, package_name) for package_name in seeds]

# Save only the migrated closure and the explicitly retained seed assets.  Saving
# all of /Game/Drone needlessly rebuilds unrelated staging packs and can exhaust
# the local DDC/memory on large environment migrations.
if not unreal.EditorAssetLibrary.save_directory(
    target_root, only_if_is_dirty=False, recursive=True
):
    raise RuntimeError("Failed to save staged dependency assets")
for seed_package in moved_seeds:
    if any(is_under(seed_package, prefix) for prefix in keep_prefixes):
        if not unreal.EditorAssetLibrary.save_asset(seed_package, only_if_is_dirty=False):
            raise RuntimeError(f"Failed to save retained staged seed: {seed_package}")

registry.scan_paths_synchronous(["/Game/Drone"], force_rescan=True)
registry.wait_for_completion()

final_closure = dependency_closure(registry, moved_seeds, options)
external_game_dependencies = sorted(
    package_name
    for package_name in final_closure
    if package_name.startswith("/Game/") and not is_under(package_name, "/Game/Drone")
)
missing_game_dependencies = sorted(
    package_name
    for package_name in final_closure
    if package_name.startswith("/Game/")
    and not registry.get_assets_by_package_name(package_name, True)
)

target_assets = registry.get_assets_by_path(target_root, recursive=True)
class_counts: dict[str, int] = {}
for asset_data in target_assets:
    class_name = str(asset_data.asset_class_path.asset_name)
    class_counts[class_name] = class_counts.get(class_name, 0) + 1

result = {
    "source_seeds": seeds,
    "source_root": source_root,
    "final_seeds": moved_seeds,
    "source_closure_count": len(source_closure),
    "moved_package_count": len(package_map),
    "target_asset_count": len(target_assets),
    "class_counts": dict(sorted(class_counts.items())),
    "external_game_dependencies": external_game_dependencies,
    "missing_game_dependencies": missing_game_dependencies,
    "package_map": package_map,
}

report_dir = os.path.join(unreal.Paths.project_saved_dir(), "AssetMigration")
os.makedirs(report_dir, exist_ok=True)
report_path = os.path.join(report_dir, report_name)
with open(report_path, "w", encoding="utf-8") as report_file:
    json.dump(result, report_file, ensure_ascii=False, indent=2)

unreal.log(f"DRONE_ASSET_STAGE_REPORT={report_path}")
unreal.log(json.dumps(result, ensure_ascii=False, indent=2))

if external_game_dependencies or missing_game_dependencies:
    raise RuntimeError(
        "Staged seed closure still has external or missing /Game dependencies"
    )
