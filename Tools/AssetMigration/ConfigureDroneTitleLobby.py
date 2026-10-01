"""Import supplied title artwork, configure WBP slots, upgrade Orbit, add isolated Racing test.

No production map is opened. Existing manual Tutorial actors are preserved. Existing Racing
maps are never rebuilt. Run only with Editor closed, via UnrealEditor-Cmd ExecutePythonScript.
"""
from __future__ import annotations

import os
import sys
import traceback
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import BuildDroneTutorialMissionTest as tutorial

PREFIX = "DRONE_TITLE_LOBBY"
SOURCE = Path(os.environ.get("DRONE_TITLE_ASSET_DIR",
    r"C:\Users\jkw11\Downloads\학교관련 문서\새 폴더\Title_Asset"))
TEXTURES = "/Game/Drone/FrontEnd/Textures/Title"
WIDGET = "/Game/Drone/FrontEnd/UI/WBP_DroneFrontEndRoot"
RACING_MAP = "/Game/Drone/Maps/TestMap/Lvl_DroneRacingTest"
RACING_MISSION = "/Game/Drone/Data/Missions/DA_Mission_Racing_Circuit_Test"
RACING_TAG = unreal.Name("Racing.Circuit.Course")
ARTWORK = {
    "title_background_texture": ("Background_1.png", "T_Title_Background"),
    "title_overlay_texture": ("Background_2.png", "T_Title_Overlay"),
    "title_logo_texture": ("Main_LOGO.png", "T_Title_Logo"),
    "button_normal_texture": ("Unselect.png", "T_Title_ButtonNormal"),
    "button_hovered_texture": ("Select.png", "T_Title_ButtonHovered"),
    "button_pressed_texture": ("Click.png", "T_Title_ButtonPressed"),
}


def import_artwork():
    assets = {}
    unreal.EditorAssetLibrary.make_directory(TEXTURES)
    for property_name, (filename, name) in ARTWORK.items():
        path = f"{TEXTURES}/{name}"
        texture = unreal.EditorAssetLibrary.load_asset(path)
        if texture is None:
            tutorial.require((SOURCE / filename).is_file(), f"Missing title PNG: {filename}")
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(SOURCE / filename))
            task.set_editor_property("destination_path", TEXTURES)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", True)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            texture = tutorial.load_asset(path)
        tutorial.require(isinstance(texture, unreal.Texture2D), f"Not a Texture2D: {path}")
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("srgb", True)
        tutorial.require(unreal.EditorAssetLibrary.save_loaded_asset(texture), f"Cannot save {path}")
        assets[property_name] = texture
    blueprint = tutorial.load_asset(WIDGET)
    defaults = unreal.get_default_object(blueprint.generated_class())
    for name, texture in assets.items():
        defaults.set_editor_property(name, texture)
    defaults.set_editor_property("use_provided_button_atlas_regions", True)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    tutorial.require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), "Cannot save title WBP")
    unreal.log(f"{PREFIX}|ARTWORK_OK|textures={len(assets)}|widget={WIDGET}")
    return assets


def configure_racing(levels, actors, textures):
    if not unreal.EditorAssetLibrary.does_asset_exist(RACING_MAP):
        tutorial.require(levels.new_level(RACING_MAP), "Cannot create Racing test map")
        cube = tutorial.load_asset(tutorial.CUBE_PATH)
        tutorial.spawn_cube(actors, cube, "RacingTest_Floor", (0, 0, -25), (90, 90, .25),
                            unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        tutorial.spawn_actor(actors, unreal.PlayerStart, "RacingTest_PlayerStart", (1800, -900, 500), yaw=90)
        tutorial.spawn_actor(actors, unreal.DirectionalLight, "RacingTest_KeyLight", (0, 0, 2200), yaw=-35, pitch=-45)
        tutorial.spawn_actor(actors, unreal.SkyLight, "RacingTest_SkyLight", (0, 0, 1800))
        course = tutorial.spawn_actor(actors, tutorial.load_blueprint_class(tutorial.COURSE_PATH),
                                     "RacingTest_Circuit", (0, 0, 0), (RACING_TAG,))
        tutorial.configure_orbit_course(course, tutorial.load_blueprint_class(tutorial.GATE_PATH), radius=1800.0)
        course.set_editor_property("course_id", unreal.Name("Course.Racing.Circuit.Test"))
        world = unreal.EditorLevelLibrary.get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", tutorial.load_blueprint_class(tutorial.GAME_MODE_PATH))
        tutorial.require(levels.save_current_level(), "Cannot save Racing test map")
    else:
        tutorial.require(levels.load_level(RACING_MAP), "Cannot load Racing test map")
    course = tutorial.actor_by_label(actors, "RacingTest_Circuit")
    tutorial.require(course is not None and RACING_TAG in course.get_editor_property("tags"), "Racing Course missing")
    tutorial.require(course.get_course_spline().is_closed_loop(), "Racing course must be closed")
    tutorial.require(course.get_gate_sequence_component().is_configuration_valid(), "Racing sequence invalid")
    course.rebuild_automatic_gates()
    tutorial.require(levels.save_current_level(), "Cannot save Racing gate components")
    mission = unreal.EditorAssetLibrary.load_asset(RACING_MISSION)
    if mission is None:
        tutorial.require(unreal.EditorAssetLibrary.duplicate_asset(
            f"{tutorial.MISSION_FOLDER}/DA_Mission_Tutorial_GateFlight", RACING_MISSION), "Cannot create Racing DA")
        mission = tutorial.load_asset(RACING_MISSION)
    mission.set_editor_property("mission_id", unreal.Name("Mission.Racing.Circuit.Test"))
    mission.set_editor_property("lobby_category", unreal.DroneMissionCategory.RACING)
    mission.set_editor_property("display_name", "레이싱 - 원형 코스 기록 시험")
    mission.set_editor_property("lobby_description", "시작, 7개 체크포인트, 결승을 순서대로 통과합니다. 완주 시간·거리·평균 속도를 기록합니다. 정식 레이싱 규칙과 Best Lap 영구 저장은 미확정/미구현입니다.")
    mission.set_editor_property("region_text", "독립 레이싱 시험맵")
    mission.set_editor_property("difficulty_text", "시험")
    mission.set_editor_property("thumbnail", textures["title_background_texture"])
    mission.set_editor_property("mission_map", tutorial.load_asset(RACING_MAP))
    mission.set_editor_property("allowed_drone_ids", [unreal.Name("Drone.Scout.Greybox"), unreal.Name("Drone.FPVStrike.Greybox")])
    mission.set_editor_property("default_drone_id", unreal.Name("Drone.Scout.Greybox"))
    mission.set_editor_property("objective_rules", [tutorial.make_rule(
        "Objective.Racing.Circuit", "원형 코스를 정방향으로 한 바퀴 완주하세요.", "lap", 180.0, RACING_TAG)])
    tutorial.require(mission.is_definition_valid(), "Racing DA invalid")
    tutorial.require(unreal.EditorAssetLibrary.save_loaded_asset(mission, only_if_is_dirty=False), "Cannot save Racing DA")
    unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(), "MAP CHECK")
    unreal.log(f"{PREFIX}|RACING_OK|map={RACING_MAP}|mission=Mission.Racing.Circuit.Test")


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    textures = import_artwork()
    tutorial.require(levels.load_level(tutorial.MAP_PATH), "Cannot load Tutorial test map")
    tutorial.ensure_orbit_station(actors)
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.DroneTrainingCourse) and actor.is_using_automatic_spline_gates():
            actor.rebuild_automatic_gates()
    tutorial.require(levels.save_current_level(), "Cannot save Orbit upgrade")
    tutorial.configure_missions({"DA_Mission_Tutorial_Heading", "DA_Mission_Tutorial_GateFlight"})
    tutorial.validate(levels, actors)
    configure_racing(levels, actors, textures)
    unreal.log(f"{PREFIX}|VALIDATION_OK|title=1|tabs=3|orbit=1|racing=1")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"{PREFIX}|FAILED|{error}\n{traceback.format_exc()}")
        raise
