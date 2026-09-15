"""FPV Definition 하나만 Rate/Acro 검증값으로 갱신한다.

전체 역할/미션 Asset 빌더를 다시 실행하지 않으므로 현재 미션 편집 상태를 덮어쓰지 않는다.
"""

import unreal


FPV_DEFINITION_PATH = "/Game/Drone/Data/Drones/DA_Drone_FPVStrike_Greybox"


asset = unreal.EditorAssetLibrary.load_asset(FPV_DEFINITION_PATH)
if not asset:
    raise RuntimeError(f"FPV Drone Definition을 불러오지 못했습니다: {FPV_DEFINITION_PATH}")

profile = asset.get_editor_property("flight_profile")
profile.set_editor_property(
    "default_control_mode",
    unreal.DroneControlMode.ACRO_RATE_REALISTIC_GREYBOX,
)
profile.set_editor_property("default_handling_preset", unreal.DroneHandlingPreset.AGILE)

# Agile 배율 1.25 적용 뒤 2700 cm/s = 27 m/s가 되도록 원본값을 2160 cm/s로 둔다.
profile.set_editor_property("max_speed_centimeters_per_second", 2160.0)
profile.set_editor_property("acceleration_centimeters_per_second_squared", 4200.0)
profile.set_editor_property("deceleration_centimeters_per_second_squared", 1750.0)
profile.set_editor_property(
    "acro_rate_settings",
    unreal.DroneAcroRateSettings(
        pitch_roll_center_sensitivity_degrees_per_second=180.0,
        maximum_pitch_rate_degrees_per_second=650.0,
        maximum_roll_rate_degrees_per_second=650.0,
        yaw_center_sensitivity_degrees_per_second=140.0,
        maximum_yaw_rate_degrees_per_second=400.0,
        pitch_roll_expo=0.30,
        yaw_expo=0.20,
        maximum_world_vertical_speed_centimeters_per_second=900.0,
    ),
)
profile.set_editor_property(
    "feature_highlights",
    [
        "FPV Rate/Acro 기본 조작",
        "고기동 27 m/s 기준 속도",
        "Arm 뒤 속도 조건 충돌 자폭",
    ],
)

asset.set_editor_property("flight_profile", profile)
unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

unreal.log(
    "FPV_ACRO_PROFILE_OK|default=RateAcro|handling=Agile|"
    "runtime_speed=2700cm/s|pitch_roll=650dps|yaw=400dps|vertical=900cm/s"
)
