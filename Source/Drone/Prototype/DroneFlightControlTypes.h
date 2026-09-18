#pragma once

#include "CoreMinimal.h"
#include "DroneFlightControlTypes.generated.h"

/**
 * 조작 보조 수준이다. 기체 역할이나 속도 프리셋과 독립적으로 바꿀 수 있다.
 * ManualRealisticGreybox는 제한된 기울기와 관성을 사용하는 중간 단계다.
 * 두 Acro 모드는 스틱을 각속도 명령으로 해석하고 자동 수평 복귀를 끄며,
 * 실제 RC 송신기의 Mode 1/Mode 2 수직축 배치를 각각 사용한다.
 * 두 실제 조작형 모두 모터별 추력/PID/공기역학을 1:1 시뮬레이션하지는 않는다.
 */
UENUM(BlueprintType)
enum class EDroneControlMode : uint8
{
	AssistedEasy UMETA(DisplayName="쉬운 조작"),
	ManualRealisticGreybox UMETA(DisplayName="실제 조작형 (그레이박스)"),
	AcroRateMode1Greybox UMETA(DisplayName="FPV Rate/Acro Mode 1 (그레이박스)"),
	/** 기존 Asset 호환 이름이다. 동작 의미는 RC 송신기 Mode 2다. */
	AcroRateRealisticGreybox UMETA(DisplayName="FPV Rate/Acro Mode 2 (그레이박스)")
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

	/** 스틱 중립에서 중력을 상쇄하는 0~1 추력 위치다. 낮을수록 남는 상승 추력이 커진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Physics", meta=(ClampMin="0.05", ClampMax="0.95"))
	float HoverThrottleNormalized = 0.50f;

	/** World Down으로 적용할 중력 가속도다. 최대 추력은 이 값 / HoverThrottle로 계산한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Physics", meta=(ClampMin="1.0", ForceUnits="cm/s^2"))
	float GravityAccelerationCentimetersPerSecondSquared = 980.0f;

	/** 공기 저항 Greybox 계수다. 0이면 관성을 그대로 유지하고 값이 클수록 속도가 빨리 줄어든다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Physics", meta=(ClampMin="0.0", ForceUnits="1/s"))
	float LinearDragPerSecond = 0.12f;

	/** 목표 Body Rate에 도달하는 응답 시간이다. 작은 값일수록 스틱 반응이 즉각적이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Acro Rate|Physics", meta=(ClampMin="0.001", ForceUnits="s"))
	float BodyRateResponseTimeSeconds = 0.08f;
};

/** 동일 기체에서도 바꿀 수 있는 속도 단계다. 기존 열거형 이름은 Asset 호환을 위해 유지한다. */
UENUM(BlueprintType)
enum class EDroneHandlingPreset : uint8
{
	Stable UMETA(DisplayName="느림"),
	Balanced UMETA(DisplayName="보통"),
	Agile UMETA(DisplayName="빠름")
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

/** 느림/보통/빠름 단계별 배율이다. 파생 Blueprint에서 수치를 조정할 수 있다. */
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
