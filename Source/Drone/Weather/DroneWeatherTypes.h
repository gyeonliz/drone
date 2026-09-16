#pragma once

#include "CoreMinimal.h"
#include "DroneWeatherTypes.generated.h"

/** Mission이 선택하는 지속풍·돌풍·난류 입력값이다. 단위는 기획자가 읽기 쉬운 m/s와 초를 사용한다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneWindSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind", meta=(ClampMin="-360.0", ClampMax="360.0", ForceUnits="deg"))
	float DirectionYawDegrees = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.0", ClampMax="50.0", ForceUnits="m/s"))
	float BaseSpeedMetersPerSecond = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind|Gust", meta=(ClampMin="0.0", ClampMax="50.0", ForceUnits="m/s"))
	float GustAdditionalSpeedMetersPerSecond = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind|Gust", meta=(ClampMin="0.1", ForceUnits="s"))
	FVector2D GustIntervalSeconds = FVector2D(2.0f, 8.0f);

	/** 새 돌풍 세기가 커질 때 목표값을 따라가는 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind|Gust", meta=(ClampMin="0.01", ClampMax="10.0", ForceUnits="s"))
	float GustAttackSeconds = 0.65f;

	/** 돌풍 세기가 작아질 때 잦은 꺼짐처럼 보이지 않도록 천천히 빠지는 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind|Gust", meta=(ClampMin="0.01", ClampMax="10.0", ForceUnits="s"))
	float GustReleaseSeconds = 1.50f;

	/** 풍향 변화가 최단각으로 새 목표를 따라가는 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind|Gust", meta=(ClampMin="0.01", ClampMax="10.0", ForceUnits="s"))
	float DirectionResponseSeconds = 0.80f;

	/** 풍향을 좌우 최대 20도까지 흔들고 돌풍 변화를 빠르게 만드는 0~1 Greybox 값이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind|Gust", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Turbulence01 = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind|Gust", meta=(ClampMin="0.0", ClampMax="20.0", ForceUnits="m/s"))
	float VerticalGustMetersPerSecond = 0.0f;

	/** 모든 Drone에 전달하기 전 Profile 수준에서 적용하는 외력 배율이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.0", ClampMax="2.0"))
	float DroneWindResponseMultiplier = 1.0f;
};

/** 비 Niagara와 Material/Audio가 읽을 표현값이다. Gameplay 피해·신호 손실은 포함하지 않는다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneRainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Intensity01 = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Niagara", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SpawnScale01 = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Visibility", meta=(ClampMin="1.0", ForceUnits="m"))
	float VisibilityDistanceMeters = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Camera", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ScreenDropletIntensity01 = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Material", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SurfaceWetness01 = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Niagara", meta=(ClampMin="0.0", ClampMax="1.0"))
	float GroundSplashScale01 = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Indoor", meta=(ClampMin="0.0", ClampMax="1.0"))
	float IndoorRainAttenuation01 = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AudioVolume01 = 0.0f;
};

/** Gameplay 바람과 비 표현이 같은 시점 값을 읽게 하는 World 단위 사본이다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneWeatherSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather")
	FName WeatherId = NAME_None;

	/** 지속풍·돌풍·수직 기류를 합친 World 속도다. */
	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Wind", meta=(ForceUnits="cm/s"))
	FVector WindVelocityCentimetersPerSecond = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Wind", meta=(ClampMin="0.0", ClampMax="2.0"))
	float DroneWindResponseMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Rain", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RainIntensity01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Rain", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RainSpawnScale01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Rain", meta=(ForceUnits="cm"))
	float VisibilityDistanceCentimeters = 100000.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Rain")
	float ScreenDropletIntensity01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Rain")
	float SurfaceWetness01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Rain")
	float GroundSplashScale01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Rain")
	float IndoorRainAttenuation01 = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather|Rain")
	float RainAudioVolume01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Weather")
	float TransitionAlpha = 1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDroneWeatherSnapshotChangedSignature, FDroneWeatherSnapshot, Snapshot);
