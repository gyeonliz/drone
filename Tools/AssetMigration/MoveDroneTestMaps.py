"""Move project-owned test maps under /Game/Drone/Maps/TestMap.

The move uses Unreal AssetTools so hard object references can be updated by the
Editor.  The script is idempotent: when the destination already exists and the
source no longer contains a World asset it performs validation only.
"""

from __future__ import annotations

import traceback

import unreal


PREFIX = "DRONE_TESTMAP_MOVE"
TEST_MAP_FOLDER = "/Game/Drone/Maps/TestMap"
MAP_MOVES = (
    (
        "/Game/Drone/Maps/Lvl_NPCSmartObjectGreybox",
        f"{TEST_MAP_FOLDER}/Lvl_NPCSmartObjectGreybox",
    ),
)


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def has_world_asset(registry: unreal.AssetRegistry, path: str) -> bool:
    assets = registry.get_assets_by_package_name(path, True)
    return any(str(asset.asset_class_path.asset_name) == "World" for asset in assets)


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem is unavailable")

    unreal.EditorAssetLibrary.make_directory(TEST_MAP_FOLDER)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()

    for source_path, target_path in MAP_MOVES:
        source_exists = has_world_asset(registry, source_path)
        target_exists = has_world_asset(registry, target_path)

        if target_exists:
            require(not source_exists, f"Both source and target World assets exist: {source_path}, {target_path}")
            log(f"ALREADY_MOVED|{target_path}")
        else:
            require(source_exists, f"Source test map is missing: {source_path}")
            source_world = editor_assets.load_asset(source_path)
            require(isinstance(source_world, unreal.World), f"Source World could not be loaded: {source_path}")
            target_folder, _, target_name = target_path.rpartition("/")
            rename = unreal.AssetRenameData(source_world, target_folder, target_name)
            require(asset_tools.rename_assets([rename]), f"AssetTools could not move: {source_path}")
            del rename
            del source_world
            registry.scan_paths_synchronous([TEST_MAP_FOLDER], force_rescan=True)
            registry.wait_for_completion()
            require(has_world_asset(registry, target_path), f"Moved World is missing: {target_path}")
            log(f"MOVED|{source_path}|{target_path}")

        log(f"VALIDATED|{target_path}")

    log("VALIDATION_OK")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
