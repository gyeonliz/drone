"""Build the project-owned Blueprint toolkit used by Story Mission maps.

The script is idempotent and writes only below /Game/Drone/Mission.
It never opens or modifies the team's production Training map.
"""

import traceback

import unreal


PREFIX = "DRONE_MISSION_FRAMEWORK"
ROOT = "/Game/Drone/Mission"
MANAGERS = f"{ROOT}/Blueprints/Managers"
TRIGGERS = f"{ROOT}/Blueprints/Triggers"
TARGETS = f"{ROOT}/Blueprints/Targets"
TUTORIAL = f"{ROOT}/Blueprints/Tutorial"

SPECS = (
    ("BP_DroneMissionManager", MANAGERS, "/Script/Drone.DroneMissionDirector"),
    ("BP_DroneMissionPlayerController", MANAGERS, "/Script/Drone.DroneMissionPlayerController"),
    ("BP_DroneMissionGameMode", MANAGERS, "/Script/Drone.DroneMissionGameMode"),
    ("BP_MissionObjectiveTrigger", TRIGGERS, "/Script/Drone.DroneMissionTrigger"),
    ("BP_MissionFailureTrigger", TRIGGERS, "/Script/Drone.DroneMissionTrigger"),
    ("BP_MissionReturnZone", TRIGGERS, "/Script/Drone.DroneMissionReturnZone"),
    ("BP_MissionDamageTarget", TARGETS, "/Script/Drone.DroneMissionDamageTarget"),
    ("BP_TutorialHoverZone", TUTORIAL, "/Script/Drone.DroneTutorialHoverZone"),
    ("BP_TutorialHeadingZone", TUTORIAL, "/Script/Drone.DroneTutorialHeadingZone"),
)


def log(message):
    unreal.log(f"{PREFIX}|{message}")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def asset_path(name, folder):
    return f"{folder}/{name}"


def compile_and_save(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(
        blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
        f"Blueprint compile failed: {blueprint.get_path_name()}",
    )
    require(
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False),
        f"Blueprint save failed: {blueprint.get_path_name()}",
    )


def ensure_blueprint(name, folder, parent_path):
    path = asset_path(name, folder)
    parent = unreal.load_class(None, parent_path)
    require(parent is not None, f"Native parent class unavailable: {parent_path}")
    unreal.EditorAssetLibrary.make_directory(folder)
    blueprint = unreal.EditorAssetLibrary.load_asset(path)
    if blueprint is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name,
            folder,
            unreal.Blueprint.static_class(),
            factory,
            overwrite_existing=False,
        )
        log(f"CREATED|{path}")
    else:
        log(f"REUSED|{path}")
    require(isinstance(blueprint, unreal.Blueprint), f"Asset is not a Blueprint: {path}")
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == parent,
        f"Blueprint parent mismatch: {path}",
    )
    compile_and_save(blueprint)
    return blueprint


def configure_assets(assets):
    manager = assets["BP_DroneMissionManager"]
    controller = assets["BP_DroneMissionPlayerController"]
    game_mode = assets["BP_DroneMissionGameMode"]
    objective_trigger = assets["BP_MissionObjectiveTrigger"]
    failure_trigger = assets["BP_MissionFailureTrigger"]
    damage_target = assets["BP_MissionDamageTarget"]
    hover_zone = assets["BP_TutorialHoverZone"]
    heading_zone = assets["BP_TutorialHeadingZone"]

    controller_cdo = unreal.get_default_object(controller.generated_class())
    controller_cdo.set_editor_property("mission_director_class", manager.generated_class())
    game_mode_cdo = unreal.get_default_object(game_mode.generated_class())
    game_mode_cdo.set_editor_property("player_controller_class", controller.generated_class())

    objective_cdo = unreal.get_default_object(objective_trigger.generated_class())
    objective_cdo.set_editor_property(
        "trigger_action", unreal.DroneMissionTriggerAction.REPORT_OBJECTIVE_EVENT
    )
    objective_cdo.set_editor_property(
        "objective_event", unreal.DroneMissionObjectiveEvent.RETURN_TO_BASE
    )
    objective_cdo.set_editor_property(
        "actor_policy", unreal.DroneMissionTriggerActorPolicy.ACTIVE_PLAYER_DRONE
    )
    objective_cdo.set_editor_property("trigger_once", True)
    objective_cdo.get_trigger_box().set_box_extent(unreal.Vector(300.0, 300.0, 150.0), True)

    failure_cdo = unreal.get_default_object(failure_trigger.generated_class())
    failure_cdo.set_editor_property(
        "trigger_action", unreal.DroneMissionTriggerAction.FAIL_MISSION
    )
    failure_cdo.set_editor_property(
        "actor_policy", unreal.DroneMissionTriggerActorPolicy.ACTOR_WITH_TAG
    )
    failure_cdo.set_editor_property("trigger_once", True)
    failure_cdo.get_trigger_box().set_box_extent(unreal.Vector(500.0, 500.0, 250.0), True)

    damage_cdo = unreal.get_default_object(damage_target.generated_class())
    damage_cdo.set_editor_property("maximum_health", 100.0)
    damage_cdo.set_editor_property("disable_collision_when_destroyed", True)
    damage_cdo.set_editor_property("hide_visual_when_destroyed", False)

    hover_cdo = unreal.get_default_object(hover_zone.generated_class())
    hover_cdo.set_editor_property("required_hold_seconds", 3.0)
    hover_cdo.set_editor_property("maximum_speed_centimeters_per_second", 75.0)
    hover_cdo.set_editor_property("maximum_vertical_speed_centimeters_per_second", 40.0)
    hover_cdo.set_editor_property("maximum_tilt_degrees", 15.0)
    hover_cdo.set_editor_property("evaluation_interval_seconds", 0.10)
    hover_cdo.set_editor_property("reset_progress_when_unstable", True)
    hover_cdo.get_hover_box().set_box_extent(unreal.Vector(350.0, 350.0, 175.0), True)

    heading_cdo = unreal.get_default_object(heading_zone.generated_class())
    heading_cdo.set_editor_property("target_heading_degrees", 90.0)
    heading_cdo.set_editor_property("heading_tolerance_degrees", 8.0)
    heading_cdo.set_editor_property("required_hold_seconds", 1.0)
    heading_cdo.set_editor_property("evaluation_interval_seconds", 0.10)
    heading_cdo.set_editor_property("reset_progress_when_misaligned", True)
    heading_cdo.get_heading_box().set_box_extent(unreal.Vector(350.0, 350.0, 200.0), True)

    for blueprint in (
        controller,
        game_mode,
        objective_trigger,
        failure_trigger,
        damage_target,
        hover_zone,
        heading_zone,
    ):
        blueprint.modify()
        compile_and_save(blueprint)


def validate_assets(assets):
    manager_class = assets["BP_DroneMissionManager"].generated_class()
    controller_class = assets["BP_DroneMissionPlayerController"].generated_class()
    controller_cdo = unreal.get_default_object(controller_class)
    game_mode_cdo = unreal.get_default_object(assets["BP_DroneMissionGameMode"].generated_class())
    objective_cdo = unreal.get_default_object(assets["BP_MissionObjectiveTrigger"].generated_class())
    failure_cdo = unreal.get_default_object(assets["BP_MissionFailureTrigger"].generated_class())
    target_cdo = unreal.get_default_object(assets["BP_MissionDamageTarget"].generated_class())
    hover_cdo = unreal.get_default_object(assets["BP_TutorialHoverZone"].generated_class())
    heading_cdo = unreal.get_default_object(assets["BP_TutorialHeadingZone"].generated_class())

    require(controller_cdo.get_mission_director_class() == manager_class, "Controller Manager class mismatch")
    require(game_mode_cdo.get_editor_property("player_controller_class") == controller_class, "GameMode Controller mismatch")
    require(
        objective_cdo.get_trigger_action()
        == unreal.DroneMissionTriggerAction.REPORT_OBJECTIVE_EVENT,
        "Objective Trigger action mismatch",
    )
    require(
        failure_cdo.get_trigger_action() == unreal.DroneMissionTriggerAction.FAIL_MISSION,
        "Failure Trigger action mismatch",
    )
    require(target_cdo.get_health_component() is not None, "Damage Target Health is missing")
    require(target_cdo.get_target_collision() is not None, "Damage Target collision is missing")
    require(target_cdo.get_target_visual() is not None, "Damage Target visual is missing")
    require(hover_cdo.get_hover_box() is not None, "Tutorial Hover Box is missing")
    require(abs(hover_cdo.get_required_hold_seconds() - 3.0) < 0.01, "Tutorial Hover duration mismatch")
    require(heading_cdo.get_heading_box() is not None, "Tutorial Heading Box is missing")
    require(abs(heading_cdo.get_required_hold_seconds() - 1.0) < 0.01, "Tutorial Heading duration mismatch")
    require(abs(heading_cdo.get_target_heading_degrees() - 90.0) < 0.01, "Tutorial Heading target mismatch")

    for name, folder, _ in SPECS:
        require(
            unreal.EditorAssetLibrary.does_asset_exist(asset_path(name, folder)),
            f"Saved Blueprint is missing: {name}",
        )
    log("VALIDATION_OK|managers=3|triggers=3|targets=1|tutorial=2")


def main():
    assets = {
        name: ensure_blueprint(name, folder, parent)
        for name, folder, parent in SPECS
    }
    configure_assets(assets)
    validate_assets(assets)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
