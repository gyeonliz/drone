#pragma once

#include "CoreMinimal.h"
#include "DroneFlightControlTypes.generated.h"

/**
 * 조작 보조 수준이다. 기체 역할이나 비행 성능과 독립적으로 바꿀 수 있다.
 * ManualRealisticGreybox는 제한된 기울기와 관성을 사용하는 중간 단계다.
 * 두 Acro 모드는 스틱을 각속도 명령으로 해석하고 자동 수평 복귀를 끄며,
 * 실제 RC 송신기의 Mode 1/Mode 2 수직축 배치를 각각 사용한다.
 * Mode 1/2는 송신기 축 배치만 다르며 같은 질량·추력·모터 응답·항력 모델을 사용한다.
 * 비행 컨트롤러의 내부 PID와 각 모터/프로펠러 유동을 1:1 해석하는 모델은 아니다.
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

/**
 * 모든 조작 모드가 공유하는 기체 물리 기준값이다. Mode 1/2는 이 값을 똑같이 사용하고
 * Payload 질량만 총질량에 더해져 속도·가속·호버 여유를 낮춘다.
 */
USTRUCT(BlueprintType)
struct DRONE_API FDronePhysicalFlightSettings
{
	GENERATED_BODY()

	/** 기존 빠름 프리셋을 단일 무적재 기준 성능으로 승격하는 배율이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Flight|Performance", meta=(ClampMin="0.1"))
	float UnloadedMaximumSpeedMultiplier = 1.25f;

	/** 배터리 포함, Payload 제외 기체 질량이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Flight|Mass", meta=(ClampMin="0.01", ForceUnits="kg"))
	float DryMassKilograms = 2.0f;

	/** 전체 모터가 낼 수 있는 최대 합산 추력이다. 총질량이 늘면 같은 추력에서 가속도가 낮아진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Flight|Thrust", meta=(ClampMin="0.1", ForceUnits="N"))
	float MaximumTotalThrustNewtons = 40.0f;

	/** 스로틀 명령이 실제 합산 추력에 도달하는 1차 모터 응답 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Flight|Thrust", meta=(ClampMin="0.001", ForceUnits="s"))
	float MotorResponseTimeSeconds = 0.055f;

	/** 고속에서 속도의 제곱에 비례해 커지는 Greybox 항력 계수다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Flight|Drag", meta=(ClampMin="0.0", ForceUnits="1/cm"))
	float QuadraticDragPerCentimeter = 0.00004f;

	/** 매우 무거운 Payload에서도 조작 가능한 시험 하한이다. 추력 부족에 의한 하강은 별도로 그대로 발생한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Flight|Payload", meta=(ClampMin="0.1", ClampMax="1.0"))
	float MinimumLoadedSpeedMultiplier = 0.55f;
};

/** 자산/Blueprint 직렬화 호환용이다. 런타임 속도 선택에는 더 이상 사용하지 않는다. */
UENUM(BlueprintType)
enum class EDroneHandlingPreset : uint8
{
	Stable UMETA(DisplayName="Legacy Stable"),
	Balanced UMETA(DisplayName="기체 기본 성능"),
	Agile UMETA(DisplayName="Legacy Agile")
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

/** 과거 속도 단계 직렬화 호환용이다. 새 런타임 계산에서는 사용하지 않는다. */
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
