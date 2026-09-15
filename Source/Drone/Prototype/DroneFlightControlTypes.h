#pragma once

#include "CoreMinimal.h"
#include "DroneFlightControlTypes.generated.h"

/**
 * 조작 보조 수준이다. 기체 역할이나 속도 프리셋과 독립적으로 바꿀 수 있다.
 * ManualRealisticGreybox는 제한된 기울기와 관성을 사용하는 중간 단계다.
 * AcroRateRealisticGreybox는 스틱을 각속도 명령으로 해석하고 자동 수평 복귀를 끄는 FPV 검증 모드다.
 * 두 실제 조작형 모두 모터별 추력/PID/공기역학을 1:1 시뮬레이션하지는 않는다.
 */
UENUM(BlueprintType)
enum class EDroneControlMode : uint8
{
	AssistedEasy UMETA(DisplayName="쉬운 조작"),
	ManualRealisticGreybox UMETA(DisplayName="실제 조작형 (그레이박스)"),
	AcroRateRealisticGreybox UMETA(DisplayName="FPV Rate/Acro (그레이박스)")
};

/**
 * Betaflight Actual Rates와 같은 의미의 FPV 각속도 설정이다.
 * CenterSensitivity는 스틱 중앙 부근 감도, MaximumRate는 끝까지 밀었을 때의 최대 각속도다.
 */
USTRUCT(BlueprintType)
struct DRONE_API FDroneAcroRateSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Pitch Roll", meta=(ClampMin="1.0", ForceUnits="deg/s"))
	float PitchRollCenterSensitivityDegreesPerSecond = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Pitch Roll", meta=(ClampMin="1.0", ForceUnits="deg/s"))
	float MaximumPitchRateDegreesPerSecond = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Pitch Roll", meta=(ClampMin="1.0", ForceUnits="deg/s"))
	float MaximumRollRateDegreesPerSecond = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Yaw", meta=(ClampMin="1.0", ForceUnits="deg/s"))
	float YawCenterSensitivityDegreesPerSecond = 140.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Yaw", meta=(ClampMin="1.0", ForceUnits="deg/s"))
	float MaximumYawRateDegreesPerSecond = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Pitch Roll", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PitchRollExpo = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Yaw", meta=(ClampMin="0.0", ClampMax="1.0"))
	float YawExpo = 0.20f;

	/** DJI Avata 2 공개 최대 상승/하강 속도 9 m/s를 현재 민간 FPV 기준값으로 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Translation", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float MaximumWorldVerticalSpeedCentimetersPerSecond = 900.0f;
};

/** 동일 기체에서도 바꿀 수 있는 반응성 프리셋이다. 기체 종류를 뜻하지 않는다. */
UENUM(BlueprintType)
enum class EDroneHandlingPreset : uint8
{
	Stable UMETA(DisplayName="안정"),
	Balanced UMETA(DisplayName="균형"),
	Agile UMETA(DisplayName="고기동")
};

/** 조작 방식이 이동 Component의 기본 수치를 얼마나 바꾸는지 정의한다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneControlModeTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Control Mode", meta=(ClampMin="0.01"))
	float AccelerationMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Control Mode", meta=(ClampMin="0.01"))
	float DecelerationMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Control Mode", meta=(ClampMin="0.0"))
	float TurningBoostMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Control Mode", meta=(ClampMin="0.01"))
	float YawRateMultiplier = 1.0f;

	/** true면 고도 입력도 World Up이 아니라 기울어진 기체의 Up 축을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Control Mode")
	bool bUseLocalAltitudeAxis = false;

	/** true면 외형만 기울이지 않고 충돌 Root를 기울여 이동 방향에도 자세가 반영된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Control Mode")
	bool bTiltCollisionRoot = false;
};

/** 안정/균형/고기동 프리셋별 배율이다. 파생 Blueprint에서 수치를 조정할 수 있다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneHandlingPresetTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Handling", meta=(ClampMin="0.01"))
	float MaxSpeedMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Handling", meta=(ClampMin="0.01"))
	float AccelerationMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Handling", meta=(ClampMin="0.01"))
	float YawRateMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Handling", meta=(ClampMin="0.01"))
	float AttitudeLimitMultiplier = 1.0f;
};
