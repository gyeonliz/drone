"""Switch the project Training map to spline-owned automatic Ring Gates.

The script is intentionally idempotent. It changes only the single Training Course
instance and removes only top-level manually placed DroneTrainingGate actors. Generated
Child Actors owned by the Course are never deleted directly.

Set DRONE_TRAINING_GATE_VALIDATE_ONLY=1 for a read-only validation pass.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_TRAINING_AUTO_GATE"
MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"
GATE_BLUEPRINT_PATH = "/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingGate"
GATE_COUNT = 4
START_DISTANCE_CM = 200.0
END_PADDING_CM = 200.0
FALLBACK_SPACING_CM = 1200.0


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def is_training_course(actor: unreal.Actor) -> bool:
    return hasattr(actor, "get_course_spline") and hasattr(actor, "rebuild_automatic_gates")


def is_training_gate(actor: unreal.Actor) -> bool:
    return hasattr(actor, "get_gate_index") and hasattr(actor, "get_gate_trigger")


def main() -> None:
    validate_only = os.environ.get("DRONE_TRAINING_GATE_VALIDATE_ONLY") == "1"
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(level_editor is not None and actors is not None, "Editor level subsystems are unavailable")
    require(level_editor.load_level(MAP_PATH), f"Could not load map: {MAP_PATH}")

    level_actors = actors.get_all_level_actors()
    courses = [actor for actor in level_actors if is_training_course(actor)]
    require(len(courses) == 1, f"Expected exactly one Training Course, found {len(courses)}")
    course = courses[0]

    gate_blueprint = unreal.EditorAssetLibrary.load_asset(GATE_BLUEPRINT_PATH)
    require(isinstance(gate_blueprint, unreal.Blueprint), f"Missing Gate Blueprint: {GATE_BLUEPRINT_PATH}")
    gate_class = gate_blueprint.generated_class()
    require(gate_class is not None, "Gate Blueprint generated Class is unavailable")

    # Capture only placed level Gates before enabling automatic Child Actors.
    manual_gates = [
        actor
        for actor in level_actors
        if is_training_gate(actor) and actor.get_owner() != course
    ]

    expected_gate_count = (
        int(course.get_editor_property("automatic_gate_count"))
        if validate_only
        else GATE_COUNT
    )

    if not validate_only:
        course.set_editor_property("automatic_gate_class", gate_class)
        course.set_editor_property("automatic_gate_distance_offset_centimeters", 0.0)
        course.set_editor_property("automatic_gate_distance_offsets_centimeters", [])
        course.set_editor_property("automatic_gate_spline_distances_centimeters", [])
        course.set_editor_property("automatic_gate_local_offset", unreal.Vector(0.0, 0.0, 0.0))
        course.set_editor_property("automatic_gate_local_offsets", [])
        course.set_editor_property("automatic_gate_rotation_offset", unreal.Rotator(0.0, 0.0, 0.0))
        course.set_editor_property("automatic_gate_scale", unreal.Vector(1.0, 1.0, 1.0))
        course.configure_automatic_gate_layout(
            True,
            GATE_COUNT,
            True,
            START_DISTANCE_CM,
            FALLBACK_SPACING_CM,
            END_PADDING_CM,
        )

        for gate in manual_gates:
            label = gate.get_actor_label()
            require(actors.destroy_actor(gate), f"Could not remove manual Gate: {label}")
            log(f"REMOVED_MANUAL_GATE|{label}")

        require(level_editor.save_current_level(), f"Could not save map: {MAP_PATH}")
        log(f"SAVED_MAP|{MAP_PATH}")

    require(course.is_using_automatic_spline_gates(), "Automatic Spline Gate mode is disabled")
    require(
        course.get_generated_automatic_gate_count() == expected_gate_count,
        f"Generated Gate count mismatch: {course.get_generated_automatic_gate_count()}",
    )
    sequence = course.get_gate_sequence_component()
    require(sequence is not None, "Gate Sequence Component is unavailable")
    require(sequence.is_configuration_valid(), "Generated Gate Sequence is invalid")
    require(sequence.get_configured_gate_count() == expected_gate_count, "Gate Sequence count mismatch")

    remaining_manual_gates = [
        actor
        for actor in actors.get_all_level_actors()
        if is_training_gate(actor) and actor.get_owner() != course
    ]
    require(not remaining_manual_gates, f"Manual Gate actors remain: {len(remaining_manual_gates)}")
    unreal.SystemLibrary.execute_console_command(course, "MAP CHECK")
    log(f"MAP_CHECK_EXECUTED|{MAP_PATH}")
    log(
        "VALIDATION_OK|automatic=true|count={}|manual=0|class={}".format(
            expected_gate_count,
            gate_class.get_path_name(),
        )
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
