"""Build eight independent test worlds using the shared, verified station builder.

Existing independent maps are validated, not overwritten. The source shared map
and team production maps are never saved. DA settings other than MissionMap stay intact.
"""
import importlib.util
import json
import os
import traceback
import unreal

PREFIX = "DRONE_ISOLATED_MISSIONS"
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("tutorial_builder", os.path.join(HERE, "BuildDroneTutorialMissionTest.py"))
tutorial = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tutorial)
FOLDER = "/Game/Drone/Maps/TestMap/Tutorial"
COMMON = {tutorial.FLOOR_LABEL, tutorial.KEY_LIGHT_LABEL, tutorial.SKY_LIGHT_LABEL, tutorial.PLAYER_START_LABEL}
RETURN = {tutorial.RETURN_ZONE_LABEL, tutorial.RETURN_PAD_LABEL}
LAYOUTS = {
    "Hover": ({tutorial.HOVER_ZONE_LABEL, tutorial.HOVER_PAD_LABEL, *tutorial.HOVER_MARKER_LABELS} | RETURN, (-500, -1100, 500), 90),
    "Forward": ({tutorial.FORWARD_TRIGGER_LABEL, tutorial.FORWARD_PAD_LABEL} | RETURN, (-1200, 0, 350), 0),
    "Heading": ({tutorial.ORBIT_COURSE_LABEL} | RETURN, (5100, 150, 500), 90),
    "GateFlight": ({tutorial.COURSE_LABEL}, (-1200, 2600, 300), 0),
    "FPV": ({tutorial.FPV_TARGET_LABEL}, (500, 1900, 500), 0),
    "Payload": ({tutorial.PAYLOAD_TARGET_LABEL, tutorial.CARRYABLE_LABEL} | RETURN, (-1100, -1900, 500), 0),
    "UGV_NPC": ({tutorial.UGV_NPC_LABEL}, (500, -3200, 110), 0),
    "UGV_Turret": ({tutorial.UGV_TURRET_LABEL}, (2500, -3200, 110), 0),
}

def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    report = []
    for mission_spec in tutorial.MISSION_SPECS:
        key = mission_spec["asset"].removeprefix("DA_Mission_Tutorial_")
        keep, start, yaw = LAYOUTS[key]
        path = f"{FOLDER}/Lvl_Tutorial_{key}_Test"
        created = not assets.does_asset_exist(path)
        if created:
            # 빈 독립 World를 생성하고 검증된 공통 배치 함수를 재사용한다.
            # 공유 맵의 수동 편집을 변경하거나 잘못된 World 복제로 GC 참조를 남기지 않는다.
            original_path = tutorial.MAP_PATH
            tutorial.MAP_PATH = path
            try:
                tutorial.create_or_rebuild_map(levels, actors, False)
            finally:
                tutorial.MAP_PATH = original_path
        else:
            tutorial.require(levels.load_level(path), f"Load failed: {path}")
        if created:
            # 새로 생성한 시험 World의 자동 생성 Actor만 필터링한다.
            # 엔진 WorldSettings를 보존하고 이미 존재하는 수업 맵은 재배치하지 않는다.
            for actor in list(actors.get_all_level_actors()):
                if actor.get_attach_parent_actor() is not None:
                    continue  # Course-owned child Gate actors are removed by their owner.
                if tutorial.OWNED_TAG in actor.get_editor_property("tags") and actor.get_actor_label() not in keep | COMMON:
                    tutorial.require(actors.destroy_actor(actor), f"Cannot remove copied station: {actor.get_actor_label()}")
            player_start = tutorial.actor_by_label(actors, tutorial.PLAYER_START_LABEL)
            tutorial.require(player_start is not None, f"Missing PlayerStart: {path}")
            player_start.set_actor_location(unreal.Vector(*start), False, False)
            player_start.set_actor_rotation(unreal.Rotator(pitch=0, yaw=yaw, roll=0), False)
            for actor in actors.get_all_level_actors():
                if isinstance(actor, unreal.DroneTrainingCourse):
                    actor.rebuild_automatic_gates()
            tutorial.require(levels.save_current_level(), f"Save failed: {path}")
        labels = {a.get_actor_label() for a in actors.get_all_level_actors()}
        tutorial.require(keep | COMMON <= labels, f"Missing required actors in {path}: {(keep | COMMON) - labels}")
        mission = tutorial.load_asset(f"{tutorial.MISSION_FOLDER}/{mission_spec['asset']}")
        if os.environ.get("DRONE_TEST_FIX_ROTATION") == "1":
            tutorial.actor_by_label(actors, tutorial.PLAYER_START_LABEL).set_actor_rotation(unreal.Rotator(pitch=0, yaw=yaw, roll=0), False)
        mission.set_editor_property("mission_map", tutorial.load_asset(path))
        entry = tutorial.actor_by_label(actors, "MissionTest_DefaultEntry")
        if entry is None:
            entry = actors.spawn_actor_from_class(unreal.load_class(None, "/Script/Drone.DroneMissionTestEntry"), unreal.Vector())
            entry.set_actor_label("MissionTest_DefaultEntry")
            entry.set_editor_property("tags", [unreal.Name("DroneIsolatedMissionTest.Owned")])
        entry.set_editor_property("default_test_mission", mission)
        tutorial.require(levels.save_current_level(), "Cannot save independent Mission Entry")
        tutorial.require(mission.is_definition_valid(), f"Invalid mission: {mission_spec['id']}")
        tutorial.require(assets.save_loaded_asset(mission, only_if_is_dirty=False), "Cannot save MissionMap")
        # Verify every objective's TargetId can be found in the chosen independent world.
        tags = {str(tag) for a in actors.get_all_level_actors() for tag in a.get_editor_property("tags")}
        for rule in mission.get_editor_property("objective_rules"):
            target = str(rule.get_editor_property("target_id"))
            tutorial.require(target in tags, f"Objective {target} has no placed actor in {path}")
        unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(), "MAP CHECK")
        report.append({"mission": mission_spec["id"], "map": path, "actors": len(actors.get_all_level_actors()), "new": created})
        unreal.log(f"{PREFIX}|MAP_OK|{mission_spec['id']}|{path}")
    root = os.path.join(unreal.Paths.project_saved_dir(), "Automation", "GameReadiness")
    os.makedirs(root, exist_ok=True)
    with open(os.path.join(root, "isolated_mission_maps.json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    unreal.log(f"{PREFIX}|SUCCESS|maps={len(report)}")

if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error(traceback.format_exc())
        raise
