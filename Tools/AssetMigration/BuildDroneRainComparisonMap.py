"""Copy the OilRig preview for rain A/B tests; never save the imported source map."""
import traceback
import os
import shutil
import unreal

PREFIX = "DRONE_RAIN_COMPARISON"
SOURCE = "/Game/Drone/Maps/Lvl_OilRigPreview"
DESTINATION = "/Game/Drone/Maps/TestMap/Lvl_OilRigRainComparisonTest"
PROBE_LABEL = "RainComparison_FixedCameraAndControls"

def main():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not assets.does_asset_exist(DESTINATION):
        source_file = os.path.abspath(os.path.join(unreal.Paths.project_content_dir(), "Drone/Maps/Lvl_OilRigPreview.umap"))
        backup = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "Automation/GameReadiness/OilRigSourceBeforeSaveAs.umap"))
        os.makedirs(os.path.dirname(backup), exist_ok=True)
        shutil.copy2(source_file, backup)
        try:
            if not levels.load_level(SOURCE) or not unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(), DESTINATION):
                raise RuntimeError("Cannot create independent comparison map")
        finally:
            # UE SaveMap의 Save As가 원본을 먼저 재저장할 수 있으므로 원본 파일 바이트를 보존한다.
            shutil.copy2(backup, source_file)
    # SaveMap은 파일만 복사하며 현재 편집 World를 바꾸지 않는다. 반드시 목적지를 다시 연다.
    if not levels.load_level(DESTINATION):
        raise RuntimeError("Cannot load comparison map")
    probe_class = unreal.load_class(None, "/Script/Drone.DroneRainPerformanceProbe")
    probe = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == PROBE_LABEL), None)
    if probe is None:
        probe = actors.spawn_actor_from_class(probe_class, unreal.Vector(7600, -3000, 1900), unreal.Rotator(pitch=-12, yaw=-145, roll=0))
        probe.set_actor_label(PROBE_LABEL)
        probe.set_actor_location(unreal.Vector(7600, -3000, 1900), False, False)
        probe.set_actor_rotation(unreal.Rotator(pitch=-12, yaw=-145, roll=0), False)
        probe.set_editor_property("tags", [unreal.Name("DroneRainComparison.Owned")])
        if not levels.save_current_level():
            raise RuntimeError("Cannot save comparison map")
    if os.environ.get("DRONE_TEST_FIX_ROTATION") == "1":
        probe.set_actor_rotation(unreal.Rotator(pitch=-12, yaw=-145, roll=0), False)
        if not levels.save_current_level():
            raise RuntimeError("Cannot save corrected comparison camera")
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world.get_path_name().split(".")[0] != DESTINATION:
        raise RuntimeError("Refusing to save or validate a source World")
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    unreal.log(f"{PREFIX}|SUCCESS|{DESTINATION}|R=Original/Off/NearOriginal/ProjectRain")

if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error(traceback.format_exc())
        raise
