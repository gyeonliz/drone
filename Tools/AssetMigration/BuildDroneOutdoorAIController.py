"""Create the outdoor hostile AI Controller Blueprint and assign it to hostile NPC Blueprints.

This does not open or save any map. Values remain editable in the Controller Blueprint Class Defaults.
"""

from __future__ import annotations

import traceback
import unreal


PREFIX = "DRONE_OUTDOOR_AI_CONTROLLER"
FOLDER = "/Game/Drone/AI/Blueprints"
NAME = "BP_DroneNPCAIController_Outdoor"
PATH = f"{FOLDER}/{NAME}"
PARENT_PATH = "/Script/Drone.DroneNPCAIController"
HOSTILE_BLUEPRINTS = (
    "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Rifle",
    "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Shotgun",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def compile_and_save(blueprint: unreal.Blueprint) -> None:
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
            f"Blueprint compile failed: {blueprint.get_path_name()}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False),
            f"Could not save: {blueprint.get_path_name()}")


def main() -> None:
    parent = unreal.load_class(None, PARENT_PATH)
    require(parent is not None, f"Native class unavailable: {PARENT_PATH}")
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    blueprint = unreal.EditorAssetLibrary.load_asset(PATH)
    if blueprint is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, FOLDER, unreal.Blueprint.static_class(), factory, overwrite_existing=False)
        log(f"CREATED|{PATH}")
    require(isinstance(blueprint, unreal.Blueprint), f"Not a Blueprint: {PATH}")
    require(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == parent,
            f"Parent mismatch: {PATH}")
    compile_and_save(blueprint)

    controller_class = blueprint.generated_class()
    cdo = unreal.get_default_object(controller_class)
    cdo.set_editor_property("drone_sight_radius", 6000.0)
    cdo.set_editor_property("drone_lose_sight_radius", 7000.0)
    cdo.set_editor_property("patrol_smart_object_search_radius", 8000.0)
    cdo.set_editor_property("patrol_smart_object_search_half_height", 1000.0)
    cdo.set_editor_property("patrol_repeat_avoidance_radius", 1500.0)
    compile_and_save(blueprint)

    for path in HOSTILE_BLUEPRINTS:
        npc_blueprint = unreal.EditorAssetLibrary.load_asset(path)
        require(isinstance(npc_blueprint, unreal.Blueprint), f"Missing NPC Blueprint: {path}")
        npc_cdo = unreal.get_default_object(npc_blueprint.generated_class())
        npc_cdo.set_editor_property("ai_controller_class", controller_class)
        compile_and_save(npc_blueprint)
        log(f"ASSIGNED|{path}|{PATH}_C")

    log("VALIDATION_OK|sight=6000|lose=7000|search=8000|repeat_avoid=1500|maps_saved=0")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
