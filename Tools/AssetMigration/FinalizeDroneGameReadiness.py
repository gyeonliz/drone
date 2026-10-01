"""Finish only the new comparison map and changed title WBP; no production maps saved."""
import importlib.util
import os
import unreal

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "BuildDroneRainComparisonMap.py")
spec = importlib.util.spec_from_file_location("rain_comparison", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.main()
lesson_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "BuildDroneIsolatedMissionMaps.py")
lesson_spec = importlib.util.spec_from_file_location("isolated_lessons", lesson_path)
lessons = importlib.util.module_from_spec(lesson_spec)
lesson_spec.loader.exec_module(lessons)
lessons.main()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for suffix, map_name in (("GoldenTime", "Lvl_DroneStory01_GoldenTimeTest"), ("Intercept", "Lvl_DroneStory02_InterceptTest"),
                        ("VeilBreaker", "Lvl_DroneStory03_VeilBreakerTest"), ("Endgame", "Lvl_DroneStory04_EndgameTest"),
                        ("Racing", "Lvl_DroneRacingTest")):
    path = f"/Game/Drone/Maps/TestMap/{map_name}"
    if not levels.load_level(path):
        raise RuntimeError(f"Cannot load {path}")
    asset_name = "DA_Mission_Racing_Circuit_Test" if suffix == "Racing" else f"DA_Mission_Story_{suffix}_Test"
    mission = unreal.load_asset(f"/Game/Drone/Data/Missions/{asset_name}")
    entry = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == "MissionTest_DefaultEntry"), None)
    if entry is None:
        entry = actors.spawn_actor_from_class(unreal.load_class(None, "/Script/Drone.DroneMissionTestEntry"), unreal.Vector())
        entry.set_actor_label("MissionTest_DefaultEntry")
        entry.set_editor_property("tags", [unreal.Name("DroneIsolatedMissionTest.Owned")])
    entry.set_editor_property("default_test_mission", mission)
    if not levels.save_current_level():
        raise RuntimeError(f"Cannot save Entry in {path}")
    unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(), "MAP CHECK")
    unreal.log(f"DRONE_GAME_READINESS|DEFAULT_ENTRY|{path}|{asset_name}")
widget = unreal.load_asset("/Game/Drone/FrontEnd/UI/WBP_DroneFrontEndRoot")
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
if widget.get_editor_property("status") == unreal.BlueprintStatus.BS_ERROR:
    raise RuntimeError("Title WBP compilation failed")
if not unreal.EditorAssetLibrary.save_loaded_asset(widget, only_if_is_dirty=False):
    raise RuntimeError("Title WBP save failed")
unreal.log("DRONE_GAME_READINESS|SETUP_SUCCESS")
