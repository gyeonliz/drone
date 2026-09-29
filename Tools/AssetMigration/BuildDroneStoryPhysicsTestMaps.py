"""Build one Physics Sandbox and four isolated Story Mission test maps.

Owned maps live only under /Game/Drone/Maps/TestMap. The script never opens or
saves the team's production Lvl_DroneTraining map.
"""

from __future__ import annotations

import math
import os
import traceback

import unreal


PREFIX = "DRONE_STORY_PHYSICS_TEST"
MAP_FOLDER = "/Game/Drone/Maps/TestMap"
MISSION_FOLDER = "/Game/Drone/Data/Missions"
SOURCE_MISSION = f"{MISSION_FOLDER}/DA_Mission_Tutorial_Training"
MISSION_GAME_MODE = "/Game/Drone/Mission/Blueprints/Managers/BP_DroneMissionGameMode"
PAYLOAD_TARGET = "/Game/Drone/Abilities/RoleTargets/BP_RoleTest_PayloadTarget"
CARRYABLE = "/Game/Drone/Abilities/Payload/BP_DroneCarryablePayload"
DAMAGE_TARGET = "/Game/Drone/Mission/Blueprints/Targets/BP_MissionDamageTarget"
RETURN_ZONE = "/Game/Drone/Mission/Blueprints/Triggers/BP_MissionReturnZone"
MISSION_TRIGGER = "/Game/Drone/Mission/Blueprints/Triggers/BP_MissionObjectiveTrigger"
VEHICLE = "/Game/Drone/Vehicles/Blueprints/BP_GroundConformingVehicle_Greybox"
VEHICLE_ROUTE = "/Game/Drone/Vehicles/Blueprints/BP_DroneVehicleSplineRoute"
CUBE_PATH = "/Engine/BasicShapes/Cube"

PHYSICS_MAP = f"{MAP_FOLDER}/Lvl_DronePhysicsSandbox"
PHYSICS_BP_FOLDER = "/Game/Drone/Physics/Blueprints"
PHYSICS_PAWN_BP = f"{PHYSICS_BP_FOLDER}/BP_DronePhysicsCollisionTest"
PHYSICS_GAME_MODE_BP = f"{PHYSICS_BP_FOLDER}/BP_DronePhysicsTestGameMode"
NET_BP = f"{PHYSICS_BP_FOLDER}/BP_DroneNetPlacementRig"
BREAKABLE_WALL_BP = f"{PHYSICS_BP_FOLDER}/BP_DroneBreakableWallPanel"

OWNED_TAG = unreal.Name("DroneStoryPhysicsTest.Owned")
WALL_TAG = unreal.Name("PhysicsSandbox.Wall")
NET_TAG = unreal.Name("PhysicsSandbox.NetRig")
BREAKABLE_WALL_TAG = unreal.Name("PhysicsSandbox.BreakableWall")

STORY_SPECS = (
    {
        "index": 1,
        "key": "GoldenTime",
        "map": f"{MAP_FOLDER}/Lvl_DroneStory01_GoldenTimeTest",
        "asset": "DA_Mission_Story_GoldenTime_Test",
        "id": "Mission.Story.GoldenTime.Test",
        "name": "황금 시간 - 물자 전달 시험",
        "description": "드랍 드론으로 구호 물자를 지정 지점에 전달하고 출발 지점으로 귀환합니다.",
        "drone": "Drone.Drop.Greybox",
        "region": "재난 구역 Greybox",
        "difficulty": "스토리 1",
        "rules": (
            ("Objective.Story.M1.Deliver", "구호 물자를 표식 지점에 투하하세요.", "payload", 180.0, "Story.M1.Payload.Target", 1),
            ("Objective.Story.M1.Return", "출발 지점으로 귀환하세요.", "return", 90.0, "Story.M1.Return", 1),
        ),
        "facts": ("Story.M1.GoldenTime.Completed",),
    },
    {
        "index": 2,
        "key": "Intercept",
        "map": f"{MAP_FOLDER}/Lvl_DroneStory02_InterceptTest",
        "asset": "DA_Mission_Story_Intercept_Test",
        "id": "Mission.Story.Intercept.Test",
        "name": "차단 - 이동 차량 요격 시험",
        "description": "목적지에 도착하기 전에 이동 차량의 핵심 표적을 FPV 드론으로 파괴합니다.",
        "drone": "Drone.FPVStrike.Greybox",
        "region": "야외 수송로 Greybox",
        "difficulty": "스토리 2",
        "rules": (
            ("Objective.Story.M2.Intercept", "차량이 목적지에 도착하기 전에 핵심 표적을 파괴하세요.", "destroy", 150.0, "Story.M2.Convoy.Target", 1),
        ),
        "facts": ("Story.M2.Intercept.Completed",),
    },
    {
        "index": 3,
        "key": "VeilBreaker",
        "map": f"{MAP_FOLDER}/Lvl_DroneStory03_VeilBreakerTest",
        "asset": "DA_Mission_Story_VeilBreaker_Test",
        "id": "Mission.Story.VeilBreaker.Test",
        "name": "베일 브레이커 - 재밍 돌파 시험",
        "description": "광섬유 드론으로 재밍 구역을 통과한 뒤 출발 지점으로 귀환합니다.",
        "drone": "Drone.FiberOptic.Greybox",
        "region": "재밍 구역 Greybox",
        "difficulty": "스토리 3",
        "rules": (
            ("Objective.Story.M3.ExitJamming", "광섬유 연결을 유지하며 재밍 구역을 완전히 통과하세요.", "jamming_exit", 180.0, "Story.M3.Jammer", 1),
            ("Objective.Story.M3.Return", "출발 지점으로 귀환하세요.", "return", 120.0, "Story.M3.Return", 1),
        ),
        "facts": ("Story.M3.VeilBreaker.Completed",),
    },
    {
        "index": 4,
        "key": "Endgame",
        "map": f"{MAP_FOLDER}/Lvl_DroneStory04_EndgameTest",
        "asset": "DA_Mission_Story_Endgame_Test",
        "id": "Mission.Story.Endgame.Test",
        "name": "엔드게임 - 지상 거점 제압 시험",
        "description": "지상 UGV로 세 개의 지휘 표적을 파괴하고 이탈 지점으로 복귀합니다.",
        "drone": "Drone.GroundUGV.Greybox",
        "region": "지휘 거점 Greybox",
        "difficulty": "스토리 4",
        "rules": (
            ("Objective.Story.M4.Destroy", "지휘 표적 세 개를 모두 파괴하세요.", "destroy", 240.0, "Story.M4.Command.Target", 3),
            ("Objective.Story.M4.Return", "이탈 지점으로 복귀하세요.", "return", 120.0, "Story.M4.Return", 1),
        ),
        "facts": ("Story.M4.Endgame.Completed",),
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
    asset = load_asset(path)
    require(isinstance(asset, unreal.Blueprint), f"Asset is not Blueprint: {path}")
    generated = asset.generated_class()
    require(generated is not None, f"Blueprint Class unavailable: {path}")
    return generated


def load_native_class(path: str) -> unreal.Class:
    result = unreal.load_class(None, path)
    require(result is not None, f"Native Class unavailable: {path}")
    return result


def compile_blueprint(blueprint: unreal.Blueprint) -> None:
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(
        blueprint.get_editor_property("status") == unreal.BlueprintStatus.BS_UP_TO_DATE,
        f"Blueprint compile failed: {blueprint.get_path_name()}",
    )


def create_or_load_blueprint(path: str, parent_class: unreal.Class) -> unreal.Blueprint:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem unavailable")
    asset = editor_assets.load_asset(path) if editor_assets.does_asset_exist(path) else None
    if asset is None:
        folder, name = path.rsplit("/", 1)
        unreal.EditorAssetLibrary.make_directory(folder)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, folder, unreal.Blueprint.static_class(), factory, overwrite_existing=False
        )
        log(f"CREATED_BLUEPRINT|{path}")
    require(isinstance(asset, unreal.Blueprint), f"Blueprint unavailable: {path}")
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset) == parent_class,
        f"Blueprint parent mismatch: {path}",
    )
    compile_blueprint(asset)
    return asset


def configure_physics_blueprints() -> tuple[unreal.Blueprint, unreal.Blueprint, unreal.Blueprint, unreal.Blueprint]:
    scout_parent = load_native_class(
        "/Game/Drone/Integrations/RoleDrones/BP_DroneScoutIntegration.BP_DroneScoutIntegration_C"
    )
    pawn = create_or_load_blueprint(PHYSICS_PAWN_BP, scout_parent)
    pawn_cdo = unreal.get_default_object(pawn.generated_class())
    response = pawn_cdo.get_collision_response_component()
    require(response is not None, "Physics Pawn collision response component missing")
    response.configure_collision_response(True, 0.62, 80.0, 160.0, 1200.0)

    game_mode = create_or_load_blueprint(PHYSICS_GAME_MODE_BP, load_native_class("/Script/Engine.GameModeBase"))
    game_mode_cdo = unreal.get_default_object(game_mode.generated_class())
    game_mode_cdo.set_editor_property("default_pawn_class", pawn.generated_class())

    net = create_or_load_blueprint(NET_BP, load_native_class("/Script/Drone.DroneNetPlacementRig"))
    breakable_wall = create_or_load_blueprint(
        BREAKABLE_WALL_BP, load_native_class("/Script/Drone.DroneBreakableWallPanel")
    )
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for blueprint in (pawn, game_mode, net, breakable_wall):
        require(editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False), f"Could not save {blueprint.get_path_name()}")
        compile_blueprint(blueprint)
    return pawn, game_mode, net, breakable_wall


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
    tags: tuple[unreal.Name, ...] = (),
    yaw: float = 0.0,
    pitch: float = 0.0,
) -> unreal.StaticMeshActor:
    actor = spawn_actor(actors, unreal.StaticMeshActor, label, location, tags, yaw, pitch)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    require(component is not None and component.set_static_mesh(cube), f"Could not set Cube: {label}")
    component.set_collision_enabled(collision)
    return actor


def clear_owned(actors: unreal.EditorActorSubsystem) -> None:
    for actor in list(actors.get_all_level_actors()):
        if OWNED_TAG in actor.get_editor_property("tags"):
            require(actors.destroy_actor(actor), f"Could not remove owned actor: {actor.get_actor_label()}")


def begin_map(
    map_path: str,
    level_editor: unreal.LevelEditorSubsystem,
    actors: unreal.EditorActorSubsystem,
    rebuild: bool,
) -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    exists = editor_assets.does_asset_exist(map_path)
    if exists:
        require(level_editor.load_level(map_path), f"Could not load map: {map_path}")
        require(rebuild, f"Map exists; use validate mode or explicit rebuild: {map_path}")
        clear_owned(actors)
    else:
        unreal.EditorAssetLibrary.make_directory(MAP_FOLDER)
        require(level_editor.new_level(map_path), f"Could not create map: {map_path}")
        log(f"CREATED_MAP|{map_path}")


def add_common_map_content(
    actors: unreal.EditorActorSubsystem,
    cube: unreal.StaticMesh,
    prefix: str,
    floor_center: tuple[float, float, float] = (1500.0, 0.0, -25.0),
    player_start: tuple[float, float, float] = (-3000.0, 0.0, 325.0),
) -> None:
    spawn_cube(
        actors, cube, f"{prefix}_Floor", floor_center, (105.0, 65.0, 0.25),
        unreal.CollisionEnabled.QUERY_AND_PHYSICS,
    )
    spawn_actor(actors, unreal.PlayerStart, f"{prefix}_PlayerStart", player_start)
    spawn_actor(actors, unreal.DirectionalLight, f"{prefix}_KeyLight", (0.0, 0.0, 2400.0), yaw=-35.0, pitch=-45.0)
    spawn_actor(actors, unreal.SkyLight, f"{prefix}_SkyLight", (0.0, 0.0, 1800.0))


def build_physics_map(
    level_editor: unreal.LevelEditorSubsystem,
    actors: unreal.EditorActorSubsystem,
    rebuild: bool,
    game_mode: unreal.Blueprint,
    net_blueprint: unreal.Blueprint,
    breakable_wall_blueprint: unreal.Blueprint,
) -> None:
    begin_map(PHYSICS_MAP, level_editor, actors, rebuild)
    cube = load_asset(CUBE_PATH)
    add_common_map_content(actors, cube, "PhysicsSandbox", floor_center=(900.0, 0.0, -25.0), player_start=(-2200.0, 0.0, 220.0))
    spawn_cube(
        actors, cube, "PhysicsSandbox_Wall_A", (-300.0, -600.0, 250.0), (0.2, 4.0, 2.5),
        unreal.CollisionEnabled.QUERY_AND_PHYSICS, (WALL_TAG,), yaw=0.0,
    )
    spawn_cube(
        actors, cube, "PhysicsSandbox_Wall_B", (900.0, 650.0, 275.0), (0.2, 3.5, 2.75),
        unreal.CollisionEnabled.QUERY_AND_PHYSICS, (WALL_TAG,), yaw=28.0,
    )
    net = spawn_actor(
        actors, net_blueprint.generated_class(), "PhysicsSandbox_NetRig", (2200.0, 0.0, 0.0), (NET_TAG,)
    )
    net.rebuild_net()
    require(net.get_anchor_count() == 4, "Physics net must expose four anchors")
    require(net.get_strand_instance_count() >= 12, "Physics net did not build its strand grid")
    breakable_wall = spawn_actor(
        actors,
        breakable_wall_blueprint.generated_class(),
        "PhysicsSandbox_BreakableWall",
        (3400.0, 0.0, 0.0),
        (BREAKABLE_WALL_TAG,),
    )
    breakable_wall.rebuild_wall()
    require(breakable_wall.get_intact_piece_count() >= 12, "Breakable wall did not build its piece grid")
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", game_mode.generated_class())
    require(level_editor.save_current_level(), f"Could not save {PHYSICS_MAP}")
    log(f"SAVED_MAP|{PHYSICS_MAP}|walls=2|net=1|breakable_wall=1")


def configure_route(route: unreal.Actor) -> None:
    spline = route.get_route_spline()
    require(spline is not None, "Vehicle route spline missing")
    spline.modify()
    spline.clear_spline_points(False)
    for index, point in enumerate(((0.0, 0.0, 0.0), (1800.0, 450.0, 0.0), (3600.0, -500.0, 0.0), (6000.0, 0.0, 0.0))):
        spline.add_spline_point(unreal.Vector(*point), unreal.SplineCoordinateSpace.LOCAL, False)
        spline.set_spline_point_type(index, unreal.SplinePointType.CURVE, False)
    spline.set_closed_loop(False, False)
    spline.update_spline()


def add_return_zone(
    actors: unreal.EditorActorSubsystem,
    label: str,
    location: tuple[float, float, float],
    tag: str,
) -> unreal.Actor:
    actor = spawn_actor(actors, load_blueprint_class(RETURN_ZONE), label, location, (unreal.Name(tag),))
    actor.get_return_trigger().set_box_extent(unreal.Vector(450.0, 450.0, 300.0), True)
    return actor


def build_story_map(
    spec: dict[str, object],
    level_editor: unreal.LevelEditorSubsystem,
    actors: unreal.EditorActorSubsystem,
    rebuild: bool,
) -> None:
    map_path = str(spec["map"])
    begin_map(map_path, level_editor, actors, rebuild)
    cube = load_asset(CUBE_PATH)
    prefix = f"Story0{spec['index']}_{spec['key']}"
    add_common_map_content(actors, cube, prefix)

    index = int(spec["index"])
    if index == 1:
        spawn_actor(
            actors, load_blueprint_class(CARRYABLE), f"{prefix}_Carryable", (-2200.0, 0.0, 80.0)
        )
        spawn_actor(
            actors, load_blueprint_class(PAYLOAD_TARGET), f"{prefix}_PayloadTarget", (2200.0, 0.0, 60.0),
            (unreal.Name("Story.M1.Payload.Target"),),
        )
        add_return_zone(actors, f"{prefix}_Return", (-3300.0, 0.0, 250.0), "Story.M1.Return")
        spawn_cube(actors, cube, f"{prefix}_Ruins_A", (600.0, -1300.0, 300.0), (6.0, 2.0, 3.0), unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        spawn_cube(actors, cube, f"{prefix}_Ruins_B", (1200.0, 1300.0, 450.0), (3.0, 4.0, 4.5), unreal.CollisionEnabled.QUERY_AND_PHYSICS)

    elif index == 2:
        route = spawn_actor(actors, load_blueprint_class(VEHICLE_ROUTE), f"{prefix}_Route", (-2400.0, 0.0, 0.0))
        configure_route(route)
        vehicle = spawn_actor(
            actors, load_blueprint_class(VEHICLE), f"{prefix}_ConvoyVehicle", (-2400.0, 0.0, 90.0),
            (unreal.Name("Story.M2.Convoy.Vehicle"),),
        )
        vehicle.set_spline_route(route, True)
        target = spawn_actor(
            actors, load_blueprint_class(DAMAGE_TARGET), f"{prefix}_ConvoyTarget", (-2400.0, 0.0, 220.0),
            (unreal.Name("Story.M2.Convoy.Target"),),
        )
        target.attach_to_actor(
            vehicle,
            unreal.Name(),
            unreal.AttachmentRule.KEEP_WORLD,
            unreal.AttachmentRule.KEEP_WORLD,
            unreal.AttachmentRule.KEEP_WORLD,
        )
        destination = spawn_actor(
            actors, load_blueprint_class(MISSION_TRIGGER), f"{prefix}_DestinationFailure", (3600.0, 0.0, 250.0)
        )
        destination.configure_mission_trigger(
            unreal.DroneMissionTriggerAction.FAIL_MISSION,
            unreal.DroneMissionObjectiveEvent.MANUAL,
            unreal.DroneMissionTriggerActorPolicy.ACTOR_WITH_TAG,
            unreal.Name("Story.M2.Convoy.Vehicle"),
            True,
        )
        destination.get_trigger_box().set_box_extent(unreal.Vector(500.0, 900.0, 400.0), True)

    elif index == 3:
        jammer = spawn_actor(
            actors, load_native_class("/Script/Drone.DroneJammingVolume"), f"{prefix}_Jammer", (500.0, 0.0, 450.0),
            (unreal.Name("Story.M3.Jammer"),),
        )
        jammer.set_normalized_jamming_strength(0.90)
        jammer.get_jamming_bounds().set_box_extent(unreal.Vector(1250.0, 1800.0, 700.0), True)
        add_return_zone(actors, f"{prefix}_Return", (-3300.0, 0.0, 250.0), "Story.M3.Return")
        for marker_index, x in enumerate((-750.0, 500.0, 1750.0)):
            spawn_cube(
                actors, cube, f"{prefix}_JammingMarker_{marker_index + 1}", (x, -1900.0, 140.0),
                (0.15, 0.15, 1.4), unreal.CollisionEnabled.NO_COLLISION,
            )

    elif index == 4:
        target_positions = ((1400.0, -1200.0, 160.0), (2600.0, 0.0, 160.0), (1400.0, 1200.0, 160.0))
        for target_index, location in enumerate(target_positions):
            target = spawn_actor(
                actors, load_blueprint_class(DAMAGE_TARGET), f"{prefix}_CommandTarget_{target_index + 1}", location,
                (unreal.Name("Story.M4.Command.Target"),),
            )
            target.set_actor_scale3d(unreal.Vector(1.4, 1.4, 1.8))
        add_return_zone(actors, f"{prefix}_Return", (-3300.0, 0.0, 250.0), "Story.M4.Return")
        for cover_index, y in enumerate((-800.0, 0.0, 800.0)):
            spawn_cube(
                actors, cube, f"{prefix}_Cover_{cover_index + 1}", (500.0, y, 125.0),
                (1.0, 2.5, 1.25), unreal.CollisionEnabled.QUERY_AND_PHYSICS,
            )

    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", load_blueprint_class(MISSION_GAME_MODE))
    require(level_editor.save_current_level(), f"Could not save {map_path}")
    log(f"SAVED_MAP|{map_path}")


def make_rule(rule_spec: tuple[object, ...]) -> unreal.DroneMissionObjectiveRule:
    objective_id, description, event_key, time_limit, target_id, required_progress = rule_spec
    events = {
        "payload": unreal.DroneMissionObjectiveEvent.PAYLOAD_DELIVERED,
        "destroy": unreal.DroneMissionObjectiveEvent.TARGET_DESTROYED,
        "jamming_exit": unreal.DroneMissionObjectiveEvent.JAMMING_EXITED,
        "return": unreal.DroneMissionObjectiveEvent.RETURN_TO_BASE,
    }
    return unreal.DroneMissionObjectiveRule(
        objective_id=unreal.Name(str(objective_id)),
        description=str(description),
        event=events[str(event_key)],
        required_progress=int(required_progress),
        time_limit_seconds=float(time_limit),
        target_id=unreal.Name(str(target_id)),
    )


def configure_story_missions() -> None:
    source = load_asset(SOURCE_MISSION)
    require(isinstance(source, unreal.DroneMissionDefinition), "Source Mission type mismatch")
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for spec in STORY_SPECS:
        path = f"{MISSION_FOLDER}/{spec['asset']}"
        mission = editor_assets.load_asset(path) if editor_assets.does_asset_exist(path) else None
        if mission is None:
            require(unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MISSION, path), f"Could not create {path}")
            mission = load_asset(path)
            log(f"CREATED_MISSION|{path}")
        require(isinstance(mission, unreal.DroneMissionDefinition), f"Mission type mismatch: {path}")
        mission.set_editor_property("mission_id", unreal.Name(str(spec["id"])))
        mission.set_editor_property("display_name", str(spec["name"]))
        mission.set_editor_property("lobby_description", str(spec["description"]))
        mission.set_editor_property("region_text", str(spec["region"]))
        mission.set_editor_property("difficulty_text", str(spec["difficulty"]))
        mission.set_editor_property("thumbnail", None)
        mission.set_editor_property("briefing_asset", unreal.SoftObjectPath())
        mission.set_editor_property("mission_map", load_asset(str(spec["map"])))
        mission.set_editor_property("allowed_drone_ids", [unreal.Name(str(spec["drone"]))])
        mission.set_editor_property("default_drone_id", unreal.Name(str(spec["drone"])))
        mission.set_editor_property("initial_objectives", [])
        mission.set_editor_property("objective_rules", [make_rule(rule) for rule in spec["rules"]])
        mission.set_editor_property("story_facts_granted_on_success", [unreal.Name(value) for value in spec["facts"]])
        mission.set_editor_property("story_facts_removed_on_success", [])
        mission.set_editor_property("success_rule_id", unreal.Name(f"Rule.Story.{spec['key']}.Success"))
        mission.set_editor_property("failure_rule_id", unreal.Name(f"Rule.Story.{spec['key']}.Failure"))
        require(mission.is_definition_valid(), f"Mission invalid after configuration: {path}")
        require(editor_assets.save_loaded_asset(mission, only_if_is_dirty=False), f"Could not save {path}")
        log(f"SAVED_MISSION|{path}")


def actor_count_with_tag(actors: unreal.EditorActorSubsystem, tag: unreal.Name) -> int:
    return sum(1 for actor in actors.get_all_level_actors() if tag in actor.get_editor_property("tags"))


def validate_all(
    level_editor: unreal.LevelEditorSubsystem,
    actors: unreal.EditorActorSubsystem,
    game_mode: unreal.Blueprint,
    net_blueprint: unreal.Blueprint,
    breakable_wall_blueprint: unreal.Blueprint,
) -> None:
    require(level_editor.load_level(PHYSICS_MAP), f"Could not validate {PHYSICS_MAP}")
    require(actor_count_with_tag(actors, WALL_TAG) >= 2, "Physics walls missing")
    nets = [actor for actor in actors.get_all_level_actors() if actor.get_class() == net_blueprint.generated_class()]
    require(len(nets) == 1, "Physics Sandbox must contain one Net Rig")
    require(nets[0].get_anchor_count() == 4 and nets[0].get_strand_instance_count() >= 12, "Net Rig contract invalid")
    breakable_walls = [
        actor for actor in actors.get_all_level_actors()
        if actor.get_class() == breakable_wall_blueprint.generated_class()
    ]
    require(len(breakable_walls) == 1, "Physics Sandbox must contain one Breakable Wall")
    require(breakable_walls[0].get_intact_piece_count() >= 12, "Breakable Wall contract invalid")
    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world.get_world_settings().get_editor_property("default_game_mode") == game_mode.generated_class(), "Physics GameMode mismatch")
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    log("VALIDATED_MAP|physics|walls=2|net=1|anchors=4|breakable_wall=1")

    expected_game_mode = load_blueprint_class(MISSION_GAME_MODE)
    for spec in STORY_SPECS:
        require(level_editor.load_level(str(spec["map"])), f"Could not validate {spec['map']}")
        world = unreal.EditorLevelLibrary.get_editor_world()
        require(world.get_world_settings().get_editor_property("default_game_mode") == expected_game_mode, f"GameMode mismatch: {spec['map']}")
        mission = load_asset(f"{MISSION_FOLDER}/{spec['asset']}")
        require(mission.is_definition_valid(), f"Mission invalid: {spec['asset']}")
        require(len(mission.get_editor_property("objective_rules")) == len(spec["rules"]), f"Rule count mismatch: {spec['asset']}")
        unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
        log(f"VALIDATED_MAP|story={spec['index']}|{spec['map']}")
    log("VALIDATION_OK|physics=1|story_maps=4|story_missions=4")


def main() -> None:
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(level_editor is not None and actors is not None and editor_assets is not None, "Editor subsystems unavailable")

    _pawn, game_mode, net, breakable_wall = configure_physics_blueprints()
    validate_only = os.environ.get("DRONE_STORY_PHYSICS_VALIDATE_ONLY") == "1"
    rebuild = os.environ.get("DRONE_STORY_PHYSICS_REBUILD") == "1"
    all_maps = [PHYSICS_MAP, *(str(spec["map"]) for spec in STORY_SPECS)]
    missing = [path for path in all_maps if not editor_assets.does_asset_exist(path)]
    if not validate_only and (missing or rebuild):
        build_physics_map(level_editor, actors, rebuild, game_mode, net, breakable_wall)
        for spec in STORY_SPECS:
            build_story_map(spec, level_editor, actors, rebuild)
        configure_story_missions()
    elif missing:
        raise RuntimeError(f"Validation requested but maps are missing: {missing}")
    validate_all(level_editor, actors, game_mode, net, breakable_wall)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
