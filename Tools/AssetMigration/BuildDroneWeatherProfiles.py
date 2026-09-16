"""프로젝트 소유 기상 Profile 3종을 생성하거나 동일 값으로 갱신한다.

Niagara/Material 자산을 생성하지 않는다. 이 파일은 Gameplay 바람과 향후 비 표현이
공유할 데이터 계약만 준비하며, 반복 실행해도 같은 Asset을 재사용한다.
"""

import unreal


WEATHER_FOLDER = "/Game/Drone/Data/Weather"
PROFILES = {
    "DA_Weather_Clear": {
        "weather_id": "Weather.Clear",
        "transition_seconds": 2.0,
        "random_seed": 1001,
        "wind": unreal.DroneWindSettings(
            direction_yaw_degrees=0.0,
            base_speed_meters_per_second=0.0,
            gust_additional_speed_meters_per_second=0.0,
            gust_interval_seconds=unreal.Vector2D(2.0, 8.0),
            gust_attack_seconds=0.65,
            gust_release_seconds=1.50,
            direction_response_seconds=0.80,
            turbulence01=0.0,
            vertical_gust_meters_per_second=0.0,
            drone_wind_response_multiplier=1.0,
        ),
        "rain": unreal.DroneRainSettings(
            intensity01=0.0,
            spawn_scale01=0.0,
            visibility_distance_meters=1000.0,
            screen_droplet_intensity01=0.0,
            surface_wetness01=0.0,
            ground_splash_scale01=0.0,
            indoor_rain_attenuation01=1.0,
            audio_volume01=0.0,
        ),
    },
    "DA_Weather_LightWind": {
        "weather_id": "Weather.LightWind",
        "transition_seconds": 3.0,
        "random_seed": 2001,
        "wind": unreal.DroneWindSettings(
            direction_yaw_degrees=35.0,
            base_speed_meters_per_second=4.0,
            gust_additional_speed_meters_per_second=2.0,
            gust_interval_seconds=unreal.Vector2D(3.0, 7.0),
            gust_attack_seconds=0.80,
            gust_release_seconds=1.80,
            direction_response_seconds=1.00,
            turbulence01=0.2,
            vertical_gust_meters_per_second=0.4,
            drone_wind_response_multiplier=1.0,
        ),
        "rain": unreal.DroneRainSettings(
            intensity01=0.0,
            spawn_scale01=0.0,
            visibility_distance_meters=1000.0,
            screen_droplet_intensity01=0.0,
            surface_wetness01=0.0,
            ground_splash_scale01=0.0,
            indoor_rain_attenuation01=1.0,
            audio_volume01=0.0,
        ),
    },
    "DA_Weather_RainStorm_Greybox": {
        "weather_id": "Weather.RainStorm.Greybox",
        "transition_seconds": 5.0,
        "random_seed": 3001,
        # 최대 수평풍 약 10.7 m/s는 현재 FPV 검증용 상한이며 최종 기체 보증 수치가 아니다.
        "wind": unreal.DroneWindSettings(
            direction_yaw_degrees=-25.0,
            base_speed_meters_per_second=8.0,
            gust_additional_speed_meters_per_second=2.7,
            gust_interval_seconds=unreal.Vector2D(2.0, 5.0),
            gust_attack_seconds=0.45,
            gust_release_seconds=1.20,
            direction_response_seconds=0.65,
            turbulence01=0.35,
            vertical_gust_meters_per_second=0.8,
            drone_wind_response_multiplier=1.0,
        ),
        "rain": unreal.DroneRainSettings(
            intensity01=0.8,
            spawn_scale01=0.75,
            visibility_distance_meters=180.0,
            screen_droplet_intensity01=0.55,
            surface_wetness01=0.85,
            ground_splash_scale01=0.65,
            indoor_rain_attenuation01=0.1,
            audio_volume01=0.8,
        ),
    },
}


def load_or_create_profile(asset_name):
    asset_path = f"{WEATHER_FOLDER}/{asset_name}"
    existing = (
        unreal.EditorAssetLibrary.load_asset(asset_path)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path)
        else None
    )
    if existing:
        if not isinstance(existing, unreal.DroneWeatherProfile):
            raise RuntimeError(f"다른 형식의 Asset이 같은 경로를 사용합니다: {asset_path}")
        return existing

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DroneWeatherProfile)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        WEATHER_FOLDER,
        unreal.DroneWeatherProfile,
        factory,
    )
    if not created:
        raise RuntimeError(f"Weather Profile 생성 실패: {asset_path}")
    return created


for name, values in PROFILES.items():
    profile = load_or_create_profile(name)
    profile.set_editor_property("weather_id", unreal.Name(values["weather_id"]))
    profile.set_editor_property("gameplay_update_hertz", 10.0)
    profile.set_editor_property("transition_seconds", values["transition_seconds"])
    profile.set_editor_property("random_seed", values["random_seed"])
    profile.set_editor_property("wind", values["wind"])
    profile.set_editor_property("rain", values["rain"])
    if not profile.is_profile_valid():
        raise RuntimeError(f"Weather Profile 검증 실패: {profile.get_path_name()}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False):
        raise RuntimeError(f"Weather Profile 저장 실패: {profile.get_path_name()}")


unreal.log(
    "DRONE_WEATHER_PROFILES_OK|clear=1|light_wind=1|rain_storm_greybox=1|"
    "runtime_hz=10|gust_attack_release=1|direction_response=1|rain_visual_assets=0"
)
