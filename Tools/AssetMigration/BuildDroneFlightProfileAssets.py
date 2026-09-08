import unreal


DRONE_FOLDER = "/Game/Drone/Data/Drones"
SCOUT_PATH = f"{DRONE_FOLDER}/DA_Drone_Scout_Greybox"
LEGACY_AGILE_PATH = f"{DRONE_FOLDER}/DA_Drone_Agile_Greybox"
LEGACY_STABLE_PATH = f"{DRONE_FOLDER}/DA_Drone_Stable_Greybox"
FPV_STRIKE_PATH = f"{DRONE_FOLDER}/DA_Drone_FPVStrike_Greybox"
DROP_PATH = f"{DRONE_FOLDER}/DA_Drone_Drop_Greybox"
MISSION_PATH = "/Game/Drone/Data/Missions/DA_Mission_Tutorial_Training"


def require_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"필수 Asset을 불러오지 못했습니다: {path}")
    return asset


def rename_legacy_asset(old_path, new_path):
    """이번 작업에서 만든 임시 핸들링 프리셋 Asset을 기획 역할 Asset으로 이름만 바로잡는다."""
    if unreal.EditorAssetLibrary.does_asset_exist(old_path):
        if unreal.EditorAssetLibrary.does_asset_exist(new_path):
            raise RuntimeError(f"이전/새 Asset이 동시에 존재합니다. 수동 확인 필요: {old_path}, {new_path}")
        if not unreal.EditorAssetLibrary.rename_asset(old_path, new_path):
            raise RuntimeError(f"Asset 이름 변경 실패: {old_path} -> {new_path}")


def duplicate_if_missing(destination_path):
    if not unreal.EditorAssetLibrary.does_asset_exist(destination_path):
        if not unreal.EditorAssetLibrary.duplicate_asset(SCOUT_PATH, destination_path):
            raise RuntimeError(f"Drone Definition 복제에 실패했습니다: {destination_path}")
    return require_asset(destination_path)


def make_profile(
    default_control_mode,
    default_handling_preset,
    max_speed,
    acceleration,
    deceleration,
    turning_boost,
    yaw_rate,
    bank_roll,
    tilt_pitch,
    start_in_first_person,
    highlights,
):
    # EditDefaultsOnly UStruct 필드는 생성자 인수로 완성한 뒤 Data Asset에 통째로 대입한다.
    return unreal.DroneFlightProfile(
        default_control_mode=default_control_mode,
        default_handling_preset=default_handling_preset,
        max_speed_centimeters_per_second=max_speed,
        acceleration_centimeters_per_second_squared=acceleration,
        deceleration_centimeters_per_second_squared=deceleration,
        turning_boost=turning_boost,
        yaw_rate_degrees_per_second=yaw_rate,
        maximum_visual_bank_roll_degrees=bank_roll,
        maximum_visual_tilt_pitch_degrees=tilt_pitch,
        start_in_first_person_view=start_in_first_person,
        max_health=100.0,
        feature_highlights=highlights,
    )


def configure_definition(
    asset,
    drone_id,
    display_name,
    description,
    mission_role,
    planned_capabilities,
    implemented_capabilities,
    profile,
):
    asset.set_editor_property("drone_id", unreal.Name(drone_id))
    asset.set_editor_property("display_name", display_name)
    asset.set_editor_property("description", description)
    asset.set_editor_property("mission_role", mission_role)
    asset.set_editor_property("player_controllable_in_current_build", True)
    asset.set_editor_property("planned_capabilities", planned_capabilities)
    asset.set_editor_property("implemented_capabilities", implemented_capabilities)
    asset.set_editor_property("flight_profile", profile)
    asset.set_editor_property("locked", False)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


# 기존의 Agile/Stable은 기체 종류처럼 잘못 분리했던 이번 작업의 임시 Asset이다.
# 이름을 FPV/Drop 역할로 바꾸고 안정/균형/고기동은 FlightProfile 기본 프리셋으로 이동한다.
rename_legacy_asset(LEGACY_AGILE_PATH, FPV_STRIKE_PATH)
rename_legacy_asset(LEGACY_STABLE_PATH, DROP_PATH)

scout = require_asset(SCOUT_PATH)
fpv_strike = duplicate_if_missing(FPV_STRIKE_PATH)
drop = duplicate_if_missing(DROP_PATH)

configure_definition(
    scout,
    "Drone.Scout.Greybox",
    "정찰 드론 (그레이박스)",
    "공통 비행과 거리/화각/시야 유지형 정찰 스캔을 사용하는 시험 기체입니다.",
    unreal.DroneMissionRole.RECONNAISSANCE,
    [unreal.DroneGameplayCapability.RECON_SCAN],
    [unreal.DroneGameplayCapability.RECON_SCAN],
    make_profile(
        unreal.DroneControlMode.ASSISTED_EASY,
        unreal.DroneHandlingPreset.BALANCED,
        1200.0,
        2400.0,
        3000.0,
        8.0,
        90.0,
        18.0,
        14.0,
        False,
        ["쉬운/실제 조작형 전환 가능", "안정/균형/고기동 전환 가능", "거리/화각/시야 유지형 스캔"],
    ),
)

configure_definition(
    fpv_strike,
    "Drone.FPVStrike.Greybox",
    "FPV 자폭 드론 (그레이박스)",
    "공통 비행과 명시적 Arm 뒤 속도 조건을 검사하는 1회 충돌 자폭 시험 기체입니다.",
    unreal.DroneMissionRole.FPV_STRIKE,
    [unreal.DroneGameplayCapability.IMPACT_DETONATION],
    [unreal.DroneGameplayCapability.IMPACT_DETONATION],
    make_profile(
        unreal.DroneControlMode.ASSISTED_EASY,
        unreal.DroneHandlingPreset.AGILE,
        1350.0,
        3000.0,
        3200.0,
        9.0,
        105.0,
        24.0,
        20.0,
        True,
        ["FPV 기본 시점", "고기동 기본 프리셋", "Arm 뒤 속도 조건 충돌 자폭"],
    ),
)

configure_definition(
    drop,
    "Drone.Drop.Greybox",
    "드랍 드론 (그레이박스)",
    "공통 비행과 탑뷰, 1회 Payload 투하 및 목표 접촉 판정을 사용하는 시험 기체입니다.",
    unreal.DroneMissionRole.DROP_DELIVERY,
    [unreal.DroneGameplayCapability.PAYLOAD_DROP],
    [unreal.DroneGameplayCapability.PAYLOAD_DROP],
    make_profile(
        unreal.DroneControlMode.ASSISTED_EASY,
        unreal.DroneHandlingPreset.STABLE,
        1000.0,
        1800.0,
        2500.0,
        6.0,
        72.0,
        14.0,
        11.0,
        False,
        ["3인칭 이륙 검증", "안정 기본 프리셋", "탑뷰/1회 Payload/목표 접촉"],
    ),
)

mission = require_asset(MISSION_PATH)
mission.set_editor_property(
    "allowed_drone_ids",
    [
        unreal.Name("Drone.Scout.Greybox"),
        unreal.Name("Drone.FPVStrike.Greybox"),
        unreal.Name("Drone.Drop.Greybox"),
    ],
)
mission.set_editor_property("default_drone_id", unreal.Name("Drone.Scout.Greybox"))
unreal.EditorAssetLibrary.save_loaded_asset(mission, only_if_is_dirty=False)

unreal.log("[BuildDroneFlightProfileAssets] Figma 역할 3종과 독립 조작/핸들링 기본값 저장 완료")
