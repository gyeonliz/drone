"""기존 Tutorial Training Data Asset의 한 Lap 목표만 새 Rule 형식으로 이행한다.

Production Training 맵이나 다른 Mission/Drone Asset은 열어서 저장하지 않는다.
이미 같은 Rule이 들어 있으면 아무 자산도 다시 저장하지 않는다.
"""

import unreal


MISSION_PATH = "/Game/Drone/Data/Missions/DA_Mission_Tutorial_Training"
MISSION_ID = "Mission.Tutorial.Training"
OBJECTIVE_ID = "Objective.TrainingLap"


def main():
    mission = unreal.EditorAssetLibrary.load_asset(MISSION_PATH)
    if not isinstance(mission, unreal.DroneMissionDefinition):
        raise RuntimeError(f"Mission Definition을 불러오지 못했습니다: {MISSION_PATH}")
    if str(mission.get_editor_property("mission_id")) != MISSION_ID:
        raise RuntimeError("예상한 Tutorial Training Mission ID가 아닙니다. Asset을 수정하지 않습니다.")

    initial = mission.get_editor_property("initial_objectives")
    if len(initial) != 1 or not str(initial[0]).strip():
        raise RuntimeError("기존 Training 목표가 정확히 하나가 아닙니다. 자동 이행을 중단합니다.")

    rules = mission.get_editor_property("objective_rules")
    if rules:
        if (
            len(rules) == 1
            and str(rules[0].get_editor_property("objective_id")) == OBJECTIVE_ID
            and rules[0].get_editor_property("event") == unreal.DroneMissionObjectiveEvent.TRAINING_LAP
            and rules[0].get_editor_property("required_progress") == 1
        ):
            unreal.log("DRONE_MISSION_RULE|ALREADY_CONFIGURED|Tutorial Training Lap")
            return
        raise RuntimeError("이미 다른 Objective Rule이 설정되어 있습니다. 자동 덮어쓰기를 중단합니다.")

    rule = unreal.DroneMissionObjectiveRule(
        objective_id=unreal.Name(OBJECTIVE_ID),
        description=initial[0],
        event=unreal.DroneMissionObjectiveEvent.TRAINING_LAP,
        required_progress=1,
        time_limit_seconds=0.0,
    )
    mission.set_editor_property("objective_rules", [rule])
    if not mission.is_definition_valid():
        raise RuntimeError("새 Training Lap Rule 적용 후 Mission Definition 검증 실패")
    if not unreal.EditorAssetLibrary.save_loaded_asset(mission, only_if_is_dirty=False):
        raise RuntimeError("Mission Definition 저장 실패")
    unreal.log("DRONE_MISSION_RULE|SAVED|Tutorial Training Lap")


main()
