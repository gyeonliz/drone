"""Create a project-owned Bangkok City map copy in disposable staging."""

import json
import os

import unreal


SOURCE_MAP = "/Game/BangkokCity/Maps/BangkokCity"
TARGET_MAP = "/Game/Drone/Maps/Lvl_BangkokCity"


if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP):
    raise RuntimeError(f"Missing Bangkok source map: {SOURCE_MAP}")

unreal.EditorAssetLibrary.make_directory("/Game/Drone/Maps")
if unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP):
    city_map = unreal.EditorAssetLibrary.load_asset(TARGET_MAP)
else:
    city_map = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, TARGET_MAP)
if city_map is None:
    raise RuntimeError(f"Failed to prepare Bangkok map: {TARGET_MAP}")

# The environment must not replace Drone's GameMode/Controller/Pawn/UI stack.
city_map.get_world_settings().set_editor_property("default_game_mode", None)

if not unreal.EditorAssetLibrary.save_asset(TARGET_MAP, only_if_is_dirty=False):
    raise RuntimeError(f"Failed to save Bangkok target map: {TARGET_MAP}")

result = {
    "source_map": SOURCE_MAP,
    "target_map": TARGET_MAP,
    "default_game_mode_cleared": True,
}
report_dir = os.path.join(unreal.Paths.project_saved_dir(), "AssetMigration")
os.makedirs(report_dir, exist_ok=True)
report_path = os.path.join(report_dir, "bangkok_prepare_report.json")
with open(report_path, "w", encoding="utf-8") as report_file:
    json.dump(result, report_file, ensure_ascii=False, indent=2)

unreal.log(f"DRONE_BANGKOK_PREPARE_REPORT={report_path}")
unreal.log(json.dumps(result, ensure_ascii=False, indent=2))
