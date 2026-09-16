#pragma once

#include "CoreMinimal.h"
#include "DroneAITypes.generated.h"

/**
 * Drone 프로젝트 NPC의 최소 진영 구분이다.
 *
 * 실제 국가·군 설정을 뜻하지 않는다. Smart Object 검색과 드론 감지 반응을
 * Friendly/Hostile로 분리하기 위한 프로젝트 내부 역할 값이다.
 */
UENUM(BlueprintType)
enum class EDroneNPCFaction : uint8
{
	Neutral UMETA(DisplayName="Neutral"),
	Friendly UMETA(DisplayName="Friendly"),
	Hostile UMETA(DisplayName="Hostile")
};

/**
 * 첫 Enemy AI MVP에서 분기할 휴대 무기 종류다.
 * Damage, 사거리, 발사 간격, Pellet 수치는 별도 Weapon Profile에서 확정한다.
 */
UENUM(BlueprintType)
enum class EDroneNPCWeaponType : uint8
{
	Unarmed UMETA(DisplayName="Unarmed"),
	Rifle UMETA(DisplayName="Rifle"),
	Shotgun UMETA(DisplayName="Shotgun")
};

/**
 * Map에 배치한 Smart Object Host가 어떤 용도인지 사람이 확인하기 위한 값이다.
 * 실제 검색 필터는 Smart Object Definition의 Activity Tag가 단일 기준이다.
 */
UENUM(BlueprintType)
enum class EDroneSmartObjectActivity : uint8
{
	EnemyPatrol UMETA(DisplayName="Enemy Patrol"),
	FriendlyBasePatrol UMETA(DisplayName="Friendly Base Patrol"),
	Ambient UMETA(DisplayName="Ambient"),
	Guard UMETA(DisplayName="Guard"),
	Cover UMETA(DisplayName="Cover"),
	MGTurret UMETA(DisplayName="MG Turret")
};

/**
 * Spawn Point와 NPC Blueprint가 공유하는 최소 역할 데이터다.
 * 외형, Animation, Weapon Asset은 이 데이터에 넣지 않고 Blueprint에서 조립한다.
 */
USTRUCT(BlueprintType)
struct FDroneNPCProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
	EDroneNPCFaction Faction = EDroneNPCFaction::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
	EDroneNPCWeaponType WeaponType = EDroneNPCWeaponType::Unarmed;

	/** Hostile NPC가 DroneDetected 뒤 MG Turret 후보를 검색할 수 있는지 여부다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
	bool bCanUseMGTurret = false;

	/** 개인화기 교전 중 정면의 작은 표적 이동을 몸/고개가 왕복 추종하지 않게 하는 Yaw 허용각이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Gaze", meta=(ClampMin="0.0", ClampMax="30.0", ForceUnits="deg"))
	float PersonalWeaponFacingDeadZoneDegrees = 3.0f;

	/** 데드존 경계에서 몸 회전이 On/Off를 반복하지 않게 회전 시작각에 더하는 여유각이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Gaze", meta=(ClampMin="0.0", ClampMax="30.0", ForceUnits="deg"))
	float PersonalWeaponFacingHysteresisDegrees = 3.0f;

	/** 개인화기 교전 중 몸이 표적 Yaw를 따라가는 최대 회전속도다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Gaze", meta=(ClampMin="0.0", ForceUnits="deg/s"))
	float PersonalWeaponFacingTurnSpeedDegreesPerSecond = 180.0f;
};
