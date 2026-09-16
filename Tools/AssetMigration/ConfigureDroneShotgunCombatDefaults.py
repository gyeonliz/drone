"""Apply or validate visible production Shotgun defaults without touching Training.

Set DRONE_SHOTGUN_COMBAT_APPLY=1 to modify only the Hostile Shotgun Blueprint
and the single tagged shooter in the isolated Shotgun Systems TestMap.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_SHOTGUN_COMBAT"
SHOTGUN_BLUEPRINT = "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Shotgun"
TEST_MAP = "/Game/Drone/Maps/TestMap/Lvl_DroneShotgunSystemsTest"
SHOOTER_TAG = unreal.Name("DroneShotgunSystemsTest.Shooter")
SPREAD_HALF_ANGLE_DEGREES = 12.0


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def configure_weapon(weapon: unreal.Object, apply: bool) -> None:
    require(weapon is not None, "Shotgun Weapon Component is missing")
    if apply:
        weapon.modify()
        weapon.set_editor_property("shotgun_spread_half_angle_degrees", SPREAD_HALF_ANGLE_DEGREES)
        weapon.set_shotgun_debug_trace_enabled(False)
    require(
        abs(weapon.get_shotgun_spread_half_angle_degrees() - SPREAD_HALF_ANGLE_DEGREES) < 0.001,
        "Shotgun spread must be a 12-degree half-angle",
    )
    require(not weapon.is_shotgun_debug_trace_enabled(), "Cyan debug rays must be disabled")


def main() -> None:
    apply = os.environ.get("DRONE_SHOTGUN_COMBAT_APPLY") == "1"
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(assets is not None and level_editor is not None and actors is not None, "Editor subsystems are missing")

    blueprint = assets.load_asset(SHOTGUN_BLUEPRINT)
    require(isinstance(blueprint, unreal.Blueprint), "Production Shotgun Blueprint is missing")
    shotgun_class = blueprint.generated_class()
    require(shotgun_class is not None, "Shotgun generated Class is missing")
    defaults = unreal.get_default_object(shotgun_class)
    require(defaults is not None, "Shotgun CDO is missing")
    if apply:
        blueprint.modify()
        defaults.modify()
    configure_weapon(defaults.get_npc_weapon_component(), apply)
    if apply:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR, "Shotgun compile failed")
        require(assets.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save Shotgun Blueprint")

    require(level_editor.load_level(TEST_MAP), "Could not load isolated Shotgun TestMap")
    shooters = [
        actor for actor in actors.get_all_level_actors()
        if SHOOTER_TAG in actor.get_editor_property("tags")
    ]
    require(len(shooters) == 1, f"Expected one tagged TestMap shooter, got {len(shooters)}")
    shooter = shooters[0]
    if apply:
        shooter.modify()
    configure_weapon(shooter.get_npc_weapon_component(), apply)
    if apply:
        require(level_editor.save_current_level(), "Could not save isolated Shotgun TestMap")

    unreal.log(f"{PREFIX}|VALIDATION_OK|apply={int(apply)}|spread_half_angle=12|cyan_debug=0")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
