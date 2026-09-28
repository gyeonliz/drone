"""Create the shared Tutorial Mission test map and eight focused Mission Definitions.

The tool owns only `/Game/Drone/Maps/TestMap/Lvl_DroneTutorialMissionTest` and the
eight `DA_Mission_Tutorial_*` lessons declared below. It never opens or saves the
team's production `Lvl_DroneTraining` map.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_TUTORIAL_MISSION_TEST"
MAP_FOLDER = "/Game/Drone/Maps/TestMap"
MAP_PATH = f"{MAP_FOLDER}/Lvl_DroneTutorialMissionTest"
TRAINING_MAP_PATH = "/Game/Drone/Maps/Lvl_DroneTraining"
MISSION_FOLDER = "/Game/Drone/Data/Missions"
SOURCE_MISSION_PATH = f"{MISSION_FOLDER}/DA_Mission_Tutorial_Training"

GAME_MODE_PATH = "/Game/Drone/Mission/Blueprints/Managers/BP_DroneMissionGameMode"
HOVER_ZONE_PATH = "/Game/Drone/Mission/Blueprints/Tutorial/BP_TutorialHoverZone"
HEADING_ZONE_PATH = "/Game/Drone/Mission/Blueprints/Tutorial/BP_TutorialHeadingZone"
OBJECTIVE_TRIGGER_PATH = "/Game/Drone/Mission/Blueprints/Triggers/BP_MissionObjectiveTrigger"
PAYLOAD_TARGET_PATH = "/Game/Drone/Abilities/RoleTargets/BP_RoleTest_PayloadTarget"
CARRYABLE_PATH = "/Game/Drone/Abilities/Payload/BP_DroneCarryablePayload"
DAMAGE_TARGET_PATH = "/Game/Drone/Mission/Blueprints/Targets/BP_MissionDamageTarget"
RETURN_ZONE_PATH = "/Game/Drone/Mission/Blueprints/Triggers/BP_MissionReturnZone"
COURSE_PATH = "/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingCourse"
GATE_PATH = "/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingGate"
HOSTILE_NPC_PATH = "/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Rifle"
CUBE_PATH = "/Engine/BasicShapes/Cube"

OWNED_TAG = unreal.Name("DroneTutorialMissionTest.Owned")
HOVER_TAG = unreal.Name("Tutorial.Hover.Zone")
FORWARD_TAG = unreal.Name("Tutorial.Forward.Goal")
HEADING_TAG = unreal.Name("Tutorial.Heading.Zone")
PAYLOAD_TAG = unreal.Name("Tutorial.Payload.Target")
FPV_TAG = unreal.Name("Tutorial.FPV.Target")
UGV_NPC_TAG = unreal.Name("Tutorial.UGV.NPC")
UGV_TURRET_TAG = unreal.Name("Tutorial.UGV.Turret")
RETURN_TAG = unreal.Name("Tutorial.Return.Zone")

FLOOR_LABEL = "TutorialMissionTest_Floor"
PLAYER_START_LABEL = "TutorialMissionTest_PlayerStart"
KEY_LIGHT_LABEL = "TutorialMissionTest_KeyLight"
SKY_LIGHT_LABEL = "TutorialMissionTest_SkyLight"
HOVER_ZONE_LABEL = "TutorialMissionTest_HoverZone"
HOVER_PAD_LABEL = "TutorialMissionTest_HoverPad"
FORWARD_TRIGGER_LABEL = "TutorialMissionTest_ForwardGoal"
FORWARD_PAD_LABEL = "TutorialMissionTest_ForwardPad"
HEADING_ZONE_LABEL = "TutorialMissionTest_HeadingZone"
HEADING_PAD_LABEL = "TutorialMissionTest_HeadingPad"
COURSE_LABEL = "TutorialMissionTest_GateCourse"
PAYLOAD_TARGET_LABEL = "TutorialMissionTest_PayloadTarget"
CARRYABLE_LABEL = "TutorialMissionTest_SpareCarryable"
FPV_TARGET_LABEL = "TutorialMissionTest_FPVTarget"
UGV_NPC_LABEL = "TutorialMissionTest_UGVEnemyNPC"
UGV_TURRET_LABEL = "TutorialMissionTest_UGVFixedTurretTarget"
RETURN_ZONE_LABEL = "TutorialMissionTest_ReturnZone"
RETURN_PAD_LABEL = "TutorialMissionTest_ReturnPad"

COURSE_POINTS = (
    (0.0, 0.0, 300.0),
    (900.0, 0.0, 400.0),
    (1800.0, 450.0, 525.0),
    (2700.0, 0.0, 450.0),
    (3600.0, -450.0, 350.0),
)
GATE_COUNT = 4

MISSION_SPECS = (
    {
        "asset": "DA_Mission_Tutorial_Hover",
        "id": "Mission.Tutorial.Hover",
        "name": "튜토리얼 1-1 - 호버링",
        "description": "정찰 드론을 지정 영역 안에서 3초간 안정적으로 호버링한 뒤 출발 지점으로 귀환합니다.",
        "drone": "Drone.Scout.Greybox",
        "rules": (
            ("Objective.Tutorial.Hover", "호버링 영역에서 3초간 자세를 유지하세요.", "hover", 90.0, HOVER_TAG),
            ("Objective.Tutorial.Hover.Return", "출발 지점으로 귀환하세요.", "return", 60.0, RETURN_TAG),
        ),
    },
    {
        "asset": "DA_Mission_Tutorial_Forward",
        "id": "Mission.Tutorial.Forward",
        "name": "튜토리얼 1-2 - 전진",
        "description": "정찰 드론을 전방 목표 구역까지 이동시킨 뒤 출발 지점으로 귀환합니다.",
        "drone": "Drone.Scout.Greybox",
        "rules": (
            ("Objective.Tutorial.Forward", "전방 목표 구역을 통과하세요.", "manual", 90.0, FORWARD_TAG),
            ("Objective.Tutorial.Forward.Return", "출발 지점으로 귀환하세요.", "return", 60.0, RETURN_TAG),
        ),
    },
    {
        "asset": "DA_Mission_Tutorial_Heading",
        "id": "Mission.Tutorial.Heading",
        "name": "튜토리얼 1-3 - 회전",
        "description": "표시 구역 안에서 기체를 동쪽 90도로 회전해 1초간 유지한 뒤 귀환합니다.",
        "drone": "Drone.Scout.Greybox",
        "rules": (
            ("Objective.Tutorial.Heading", "화살표 방향으로 회전해 1초간 유지하세요.", "heading", 90.0, HEADING_TAG),
            ("Objective.Tutorial.Heading.Return", "출발 지점으로 귀환하세요.", "return", 60.0, RETURN_TAG),
        ),
    },
    {
        "asset": "DA_Mission_Tutorial_GateFlight",
        "id": "Mission.Tutorial.GateFlight",
        "name": "튜토리얼 1-4 - 게이트 자유비행",
        "description": "정찰 드론으로 네 개의 게이트를 순서와 정방향에 맞게 통과합니다.",
        "drone": "Drone.Scout.Greybox",
        "rules": (
            ("Objective.Tutorial.GateFlight", "게이트 코스를 1회 완주하세요.", "lap", 180.0, unreal.Name()),
        ),
    },
    {
        "asset": "DA_Mission_Tutorial_Payload",
        "id": "Mission.Tutorial.Payload",
        "name": "튜토리얼 3 - 물자 전달",
        "description": "드랍 드론에 실린 물자를 지정 표적까지 운반해 정확히 투하한 뒤 귀환합니다.",
        "drone": "Drone.Drop.Greybox",
        "rules": (
            ("Objective.Tutorial.Payload.Deliver", "물자를 지정 표적에 투하하세요.", "payload", 120.0, PAYLOAD_TAG),
            ("Objective.Tutorial.Payload.Return", "출발 지점으로 귀환하세요.", "return", 60.0, RETURN_TAG),
        ),
    },
    {
        "asset": "DA_Mission_Tutorial_FPV",
        "id": "Mission.Tutorial.FPV",
        "name": "튜토리얼 2 - FPV 표적 타격",
        "description": "FPV 자폭 드론을 Arm한 뒤 충분한 속도로 표적에 충돌해 파괴합니다.",
        "drone": "Drone.FPVStrike.Greybox",
        "rules": (
            ("Objective.Tutorial.FPV.Destroy", "FPV 드론으로 표적을 파괴하세요.", "destroy", 90.0, FPV_TAG),
        ),
    },
    {
        "asset": "DA_Mission_Tutorial_UGV_NPC",
        "id": "Mission.Tutorial.UGV.NPC",
        "name": "튜토리얼 4-1 - UGV 적 NPC 처치",
        "description": "UGV 상부를 조준하고 Primary 총으로 체력 100의 시험용 적 NPC를 처치합니다.",
        "drone": "Drone.GroundUGV.Greybox",
        "rules": (
            ("Objective.Tutorial.UGV.NPC", "Primary 총으로 표식된 적 NPC를 처치하세요.", "destroy", 120.0, UGV_NPC_TAG),
        ),
    },
    {
        "asset": "DA_Mission_Tutorial_UGV_Turret",
        "id": "Mission.Tutorial.UGV.Turret",
        "name": "튜토리얼 4-2 - 고정형 포탑 파괴",
        "description": "UGV 총 또는 Secondary 유탄으로 체력 100의 고정형 포탑 표적을 파괴합니다.",
        "drone": "Drone.GroundUGV.Greybox",
        "rules": (
            ("Objective.Tutorial.UGV.Turret", "총 또는 유탄으로 고정형 포탑 표적을 파괴하세요.", "destroy", 120.0, UGV_TURRET_TAG),
        ),
    },
)


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load_asset(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    require(asset is not None, f"Missing asset: {path}")
    return asset


def load_blueprint_class(path: str) -> unreal.Class:
    blueprint = load_asset(path)
    require(isinstance(blueprint, unreal.Blueprint), f"Asset is not a Blueprint: {path}")
    generated_class = blueprint.generated_class()
    require(generated_class is not None, f"Blueprint Class unavailable: {path}")
    return generated_class


def actor_by_label(actors: unreal.EditorActorSubsystem, label: str) -> unreal.Actor | None:
    matches = [actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == label]
    require(len(matches) <= 1, f"Duplicate actor label: {label}")
    return matches[0] if matches else None


def spawn_actor(
    actors: unreal.EditorActorSubsystem,
    actor_class: unreal.Class,
    label: str,
    location: tuple[float, float, float],
    tags: tuple[unreal.Name, ...] = (),
    yaw: float = 0.0,
    pitch: float = 0.0,
) -> unreal.Actor:
    actor = actors.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0),
    )
    require(actor is not None, f"Could not spawn actor: {label}")
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [OWNED_TAG, *tags])
    return actor


def spawn_cube(
    actors: unreal.EditorActorSubsystem,
    cube: unreal.StaticMesh,
    label: str,
    location: tuple[float, float, float],
    scale: tuple[float, float, float],
    collision: unreal.CollisionEnabled,
) -> unreal.StaticMeshActor:
    actor = spawn_actor(actors, unreal.StaticMeshActor, label, location)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    require(component is not None and component.set_static_mesh(cube), f"Could not configure marker: {label}")
    component.set_collision_enabled(collision)
    return actor


def configure_course(course: unreal.Actor, gate_class: unreal.Class) -> None:
    spline = course.get_course_spline()
    require(spline is not None, "Gate Course spline unavailable")
    spline.modify()
    spline.clear_spline_points(False)
    for point_index, coordinates in enumerate(COURSE_POINTS):
        spline.add_spline_point(unreal.Vector(*coordinates), unreal.SplineCoordinateSpace.LOCAL, False)
        spline.set_spline_point_type(point_index, unreal.SplinePointType.CURVE, False)
    spline.set_closed_loop(False, False)
    spline.update_spline()
    course.set_editor_property("automatic_gate_class", gate_class)
    course.set_editor_property("course_line_segment_length_centimeters", 100.0)
    course.set_editor_property("automatic_gate_scale", unreal.Vector(1.0, 1.0, 1.0))
    course.configure_automatic_gate_layout(True, GATE_COUNT, True, 300.0, 900.0, 300.0)
    course.initialize_automatic_gate_spline_handles_from_current_layout()


def clear_owned_actors(actors: unreal.EditorActorSubsystem) -> None:
    for actor in list(actors.get_all_level_actors()):
        if OWNED_TAG in actor.get_editor_property("tags"):
            label = actor.get_actor_label()
            require(actors.destroy_actor(actor), f"Could not remove owned actor: {label}")
            log(f"REMOVED_OWNED_ACTOR|{label}")


def create_or_rebuild_map(
    level_editor: unreal.LevelEditorSubsystem,
    actors: unreal.EditorActorSubsystem,
    rebuild: bool,
) -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem unavailable")
    map_exists = editor_assets.does_asset_exist(MAP_PATH)
    if map_exists:
        require(level_editor.load_level(MAP_PATH), f"Could not load map: {MAP_PATH}")
        require(rebuild, "Map already exists; use validation or explicit Rebuild mode")
        clear_owned_actors(actors)
    else:
        unreal.EditorAssetLibrary.make_directory(MAP_FOLDER)
        require(level_editor.new_level(MAP_PATH), f"Could not create map: {MAP_PATH}")
        log(f"CREATED_MAP|{MAP_PATH}")

    cube = load_asset(CUBE_PATH)
    require(isinstance(cube, unreal.StaticMesh), f"Engine cube is not StaticMesh: {CUBE_PATH}")
    spawn_cube(
        actors, cube, FLOOR_LABEL, (500.0, 0.0, -25.0), (130.0, 90.0, 0.25),
        unreal.CollisionEnabled.QUERY_AND_PHYSICS,
    )
    spawn_actor(actors, unreal.PlayerStart, PLAYER_START_LABEL, (-3000.0, 0.0, 325.0))
    spawn_actor(actors, unreal.DirectionalLight, KEY_LIGHT_LABEL, (0.0, 0.0, 2200.0), yaw=-35.0, pitch=-45.0)
    spawn_actor(actors, unreal.SkyLight, SKY_LIGHT_LABEL, (0.0, 0.0, 1800.0))

    hover_zone = spawn_actor(
        actors,
        load_blueprint_class(HOVER_ZONE_PATH),
        HOVER_ZONE_LABEL,
        (-500.0, 0.0, 500.0),
        (HOVER_TAG,),
    )
    hover_zone.get_hover_box().set_box_extent(unreal.Vector(350.0, 350.0, 175.0), True)
    spawn_cube(
        actors, cube, HOVER_PAD_LABEL, (-500.0, 0.0, 5.0), (4.0, 4.0, 0.10),
        unreal.CollisionEnabled.NO_COLLISION,
    )

    forward_trigger = spawn_actor(
        actors,
        load_blueprint_class(OBJECTIVE_TRIGGER_PATH),
        FORWARD_TRIGGER_LABEL,
        (1600.0, 0.0, 350.0),
        (FORWARD_TAG,),
    )
    forward_trigger.configure_mission_trigger(
        unreal.DroneMissionTriggerAction.REPORT_OBJECTIVE_EVENT,
        unreal.DroneMissionObjectiveEvent.MANUAL,
        unreal.DroneMissionTriggerActorPolicy.ACTIVE_PLAYER_DRONE,
        unreal.Name(),
        False,
    )
    forward_trigger.get_trigger_box().set_box_extent(unreal.Vector(300.0, 450.0, 250.0), True)
    spawn_cube(
        actors, cube, FORWARD_PAD_LABEL, (1600.0, 0.0, 5.0), (4.0, 5.0, 0.10),
        unreal.CollisionEnabled.NO_COLLISION,
    )

    heading_zone = spawn_actor(
        actors,
        load_blueprint_class(HEADING_ZONE_PATH),
        HEADING_ZONE_LABEL,
        (3300.0, 0.0, 400.0),
        (HEADING_TAG,),
    )
    heading_zone.set_editor_property("target_heading_degrees", 90.0)
    heading_zone.set_editor_property("heading_tolerance_degrees", 8.0)
    heading_zone.set_editor_property("required_hold_seconds", 1.0)
    heading_zone.get_heading_box().set_box_extent(unreal.Vector(350.0, 350.0, 220.0), True)
    spawn_cube(
        actors, cube, HEADING_PAD_LABEL, (3300.0, 0.0, 5.0), (4.0, 4.0, 0.10),
        unreal.CollisionEnabled.NO_COLLISION,
    )

    course = spawn_actor(
        actors,
        load_blueprint_class(COURSE_PATH),
        COURSE_LABEL,
        (-1000.0, 2600.0, 0.0),
    )
    configure_course(course, load_blueprint_class(GATE_PATH))

    spawn_actor(
        actors,
        load_blueprint_class(PAYLOAD_TARGET_PATH),
        PAYLOAD_TARGET_LABEL,
        (2500.0, -1900.0, 40.0),
        (PAYLOAD_TAG,),
    )
    spawn_actor(
        actors,
        load_blueprint_class(CARRYABLE_PATH),
        CARRYABLE_LABEL,
        (-800.0, -1900.0, 70.0),
    )

    fpv_target = spawn_actor(
        actors,
        load_blueprint_class(DAMAGE_TARGET_PATH),
        FPV_TARGET_LABEL,
        (3000.0, 1900.0, 250.0),
        (FPV_TAG,),
    )
    fpv_target.set_actor_scale3d(unreal.Vector(1.5, 1.5, 1.5))

    ugv_npc = spawn_actor(
        actors,
        load_blueprint_class(HOSTILE_NPC_PATH),
        UGV_NPC_LABEL,
        (2300.0, -3200.0, 110.0),
        (UGV_NPC_TAG,),
        yaw=180.0,
    )
    ugv_npc.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.DISABLED)
    npc_weapon_visual = ugv_npc.get_weapon_visual_component()
    require(npc_weapon_visual is not None, "UGV NPC Weapon Visual missing")
    require(npc_weapon_visual.set_static_mesh(cube), "Could not assign UGV NPC test weapon mesh")
    npc_weapon_visual.set_relative_scale3d(unreal.Vector(0.55, 0.06, 0.06))
    npc_weapon_visual.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

    ugv_turret = spawn_actor(
        actors,
        load_blueprint_class(DAMAGE_TARGET_PATH),
        UGV_TURRET_LABEL,
        (5000.0, -3200.0, 125.0),
        (UGV_TURRET_TAG,),
        yaw=180.0,
    )
    ugv_turret.set_actor_scale3d(unreal.Vector(1.25, 1.25, 1.25))

    return_zone = spawn_actor(
        actors,
        load_blueprint_class(RETURN_ZONE_PATH),
        RETURN_ZONE_LABEL,
        (-3500.0, 0.0, 250.0),
        (RETURN_TAG,),
    )
    return_zone.get_return_trigger().set_box_extent(unreal.Vector(400.0, 400.0, 250.0), True)
    spawn_cube(
        actors, cube, RETURN_PAD_LABEL, (-3500.0, 0.0, 5.0), (5.0, 5.0, 0.10),
        unreal.CollisionEnabled.NO_COLLISION,
    )

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World unavailable")
    game_mode_class = load_blueprint_class(GAME_MODE_PATH)
    world.get_world_settings().set_editor_property("default_game_mode", game_mode_class)
    require(level_editor.save_current_level(), f"Could not save map: {MAP_PATH}")
    log(f"SAVED_MAP|{MAP_PATH}")


def make_rule(rule_id: str, description: str, event_name: str, time_limit: float, target_tag: unreal.Name):
    events = {
        "manual": unreal.DroneMissionObjectiveEvent.MANUAL,
        "hover": unreal.DroneMissionObjectiveEvent.HOVER_MAINTAINED,
        "heading": unreal.DroneMissionObjectiveEvent.HEADING_ALIGNED,
        "lap": unreal.DroneMissionObjectiveEvent.TRAINING_LAP,
        "payload": unreal.DroneMissionObjectiveEvent.PAYLOAD_DELIVERED,
        "destroy": unreal.DroneMissionObjectiveEvent.TARGET_DESTROYED,
        "return": unreal.DroneMissionObjectiveEvent.RETURN_TO_BASE,
    }
    return unreal.DroneMissionObjectiveRule(
        objective_id=unreal.Name(rule_id),
        description=description,
        event=events[event_name],
        required_progress=1,
        time_limit_seconds=time_limit,
        target_id=target_tag,
    )


def configure_missions() -> None:
    source = load_asset(SOURCE_MISSION_PATH)
    mission_map = load_asset(MAP_PATH)
    require(isinstance(source, unreal.DroneMissionDefinition), "Source Mission Definition type mismatch")
    unreal.EditorAssetLibrary.make_directory(MISSION_FOLDER)

    for spec in MISSION_SPECS:
        path = f"{MISSION_FOLDER}/{spec['asset']}"
        mission = unreal.EditorAssetLibrary.load_asset(path)
        if mission is None:
            require(unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MISSION_PATH, path), f"Could not create {path}")
            mission = load_asset(path)
            log(f"CREATED_MISSION|{path}")
        require(isinstance(mission, unreal.DroneMissionDefinition), f"Mission type mismatch: {path}")

        mission.set_editor_property("mission_id", unreal.Name(spec["id"]))
        mission.set_editor_property("display_name", spec["name"])
        mission.set_editor_property("lobby_description", spec["description"])
        mission.set_editor_property("region_text", "튜토리얼 시험장")
        mission.set_editor_property("difficulty_text", "기초")
        mission.set_editor_property("thumbnail", None)
        mission.set_editor_property("briefing_asset", unreal.SoftObjectPath())
        mission.set_editor_property("mission_map", mission_map)
        mission.set_editor_property("allowed_drone_ids", [unreal.Name(spec["drone"])])
        mission.set_editor_property("default_drone_id", unreal.Name(spec["drone"]))
        mission.set_editor_property("initial_objectives", [])
        mission.set_editor_property(
            "objective_rules",
            [make_rule(*rule_spec) for rule_spec in spec["rules"]],
        )
        mission.set_editor_property("story_facts_granted_on_success", [])
        mission.set_editor_property("story_facts_removed_on_success", [])
        mission.set_editor_property("success_rule_id", unreal.Name("Rule.Tutorial.StandardSuccess"))
        mission.set_editor_property("failure_rule_id", unreal.Name("Rule.Tutorial.TimeLimitFailure"))
        require(mission.is_definition_valid(), f"Mission Definition validation failed: {path}")
        require(unreal.EditorAssetLibrary.save_loaded_asset(mission, only_if_is_dirty=False), f"Could not save {path}")
        log(f"SAVED_MISSION|{spec['id']}|rules={len(spec['rules'])}|drone={spec['drone']}")


def validate(level_editor: unreal.LevelEditorSubsystem, actors: unreal.EditorActorSubsystem) -> None:
    require(MAP_PATH != TRAINING_MAP_PATH, "Tutorial test path must not target Production Training")
    require(level_editor.load_level(MAP_PATH), f"Could not load map for validation: {MAP_PATH}")
    expected_labels = {
        FLOOR_LABEL,
        PLAYER_START_LABEL,
        KEY_LIGHT_LABEL,
        SKY_LIGHT_LABEL,
        HOVER_ZONE_LABEL,
        HOVER_PAD_LABEL,
        FORWARD_TRIGGER_LABEL,
        FORWARD_PAD_LABEL,
        HEADING_ZONE_LABEL,
        HEADING_PAD_LABEL,
        COURSE_LABEL,
        PAYLOAD_TARGET_LABEL,
        CARRYABLE_LABEL,
        FPV_TARGET_LABEL,
        UGV_NPC_LABEL,
        UGV_TURRET_LABEL,
        RETURN_ZONE_LABEL,
        RETURN_PAD_LABEL,
    }
    for label in expected_labels:
        require(actor_by_label(actors, label) is not None, f"Missing test actor: {label}")

    require(HOVER_TAG in actor_by_label(actors, HOVER_ZONE_LABEL).get_editor_property("tags"), "Hover Tag mismatch")
    require(FORWARD_TAG in actor_by_label(actors, FORWARD_TRIGGER_LABEL).get_editor_property("tags"), "Forward Tag mismatch")
    require(HEADING_TAG in actor_by_label(actors, HEADING_ZONE_LABEL).get_editor_property("tags"), "Heading Tag mismatch")
    require(PAYLOAD_TAG in actor_by_label(actors, PAYLOAD_TARGET_LABEL).get_editor_property("tags"), "Payload Tag mismatch")
    require(FPV_TAG in actor_by_label(actors, FPV_TARGET_LABEL).get_editor_property("tags"), "FPV Tag mismatch")
    require(UGV_NPC_TAG in actor_by_label(actors, UGV_NPC_LABEL).get_editor_property("tags"), "UGV NPC Tag mismatch")
    require(UGV_TURRET_TAG in actor_by_label(actors, UGV_TURRET_LABEL).get_editor_property("tags"), "UGV Turret Tag mismatch")
    require(RETURN_TAG in actor_by_label(actors, RETURN_ZONE_LABEL).get_editor_property("tags"), "Return Tag mismatch")

    forward_trigger = actor_by_label(actors, FORWARD_TRIGGER_LABEL)
    require(
        forward_trigger.get_objective_event() == unreal.DroneMissionObjectiveEvent.MANUAL,
        "Forward Trigger Event mismatch",
    )
    heading_zone = actor_by_label(actors, HEADING_ZONE_LABEL)
    require(abs(heading_zone.get_target_heading_degrees() - 90.0) < 0.01, "Heading target mismatch")
    require(abs(heading_zone.get_heading_tolerance_degrees() - 8.0) < 0.01, "Heading tolerance mismatch")
    course = actor_by_label(actors, COURSE_LABEL)
    require(course.is_using_automatic_spline_gates(), "Gate Course automatic layout is disabled")
    require(course.get_resolved_automatic_gate_count() == GATE_COUNT, "Gate Course count mismatch")
    require(course.get_gate_sequence_component().is_configuration_valid(), "Gate Course sequence invalid")
    ugv_npc = actor_by_label(actors, UGV_NPC_LABEL)
    require(ugv_npc.get_health_component() is not None, "UGV NPC Health missing")
    require(ugv_npc.get_weapon_visual_component().get_editor_property("static_mesh") is not None, "UGV NPC test weapon mesh missing")
    ugv_turret = actor_by_label(actors, UGV_TURRET_LABEL)
    require(ugv_turret.get_health_component() is not None, "UGV turret target Health missing")

    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world is not None, "Editor World unavailable during validation")
    expected_game_mode = load_blueprint_class(GAME_MODE_PATH)
    require(world.get_world_settings().get_editor_property("default_game_mode") == expected_game_mode, "Mission GameMode mismatch")

    for spec in MISSION_SPECS:
        mission = load_asset(f"{MISSION_FOLDER}/{spec['asset']}")
        require(isinstance(mission, unreal.DroneMissionDefinition), f"Mission missing: {spec['asset']}")
        require(str(mission.get_editor_property("mission_id")) == spec["id"], f"Mission ID mismatch: {spec['asset']}")
        require(len(mission.get_editor_property("objective_rules")) == len(spec["rules"]), f"Rule count mismatch: {spec['asset']}")
        require(mission.is_definition_valid(), f"Mission invalid: {spec['asset']}")

    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    log("MAP_CHECK_EXECUTED")
    log(
        "VALIDATION_OK|map=1|missions=8|hover=1|forward=1|heading=1|course=1|"
        "payload=1|fpv=1|ugv_npc=1|ugv_turret=1|return=1"
    )


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(editor_assets is not None and level_editor is not None and actors is not None, "Editor subsystems unavailable")

    map_exists = editor_assets.does_asset_exist(MAP_PATH)
    rebuild = os.environ.get("DRONE_TUTORIAL_MISSION_REBUILD") == "1"
    if not map_exists or rebuild:
        create_or_rebuild_map(level_editor, actors, rebuild)
    configure_missions()
    validate(level_editor, actors)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
