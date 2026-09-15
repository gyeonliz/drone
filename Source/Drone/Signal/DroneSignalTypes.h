#pragma once

#include "CoreMinimal.h"
#include "DroneSignalTypes.generated.h"

/** 재밍 강도에 따른 재현 가능한 게임 상태다. 실제 전파 규격이나 장비 동작을 뜻하지 않는다. */
UENUM(BlueprintType)
enum class EDroneSignalInterferenceStage : uint8
{
	None UMETA(DisplayName="Normal"),
	Weak UMETA(DisplayName="Weak"),
	Moderate UMETA(DisplayName="Moderate"),
	Strong UMETA(DisplayName="Strong")
};

/** Pawn 조작과 HUD가 같은 순간의 신호 상태를 읽는 사본이다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneSignalSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Drone|Signal")
	float NormalizedSignalQuality = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Signal")
	float NormalizedJammingStrength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Drone|Signal")
	EDroneSignalInterferenceStage Stage = EDroneSignalInterferenceStage::None;

	/** Strong에서만 이동 최대 속도·가속도에 적용하는 결정적 배율이다. 키 입력은 버리지 않는다. */
	UPROPERTY(BlueprintReadOnly, Category="Drone|Signal")
	float ControlResponseMultiplier = 1.0f;

	/** WBP가 영상 Noise Material/Overlay를 조정할 때 쓸 0~1 값이다. Native HUD는 경고 문구를 표시한다. */
	UPROPERTY(BlueprintReadOnly, Category="Drone|Signal")
	float VideoNoiseIntensity = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDroneSignalSnapshotChangedSignature, FDroneSignalSnapshot, Snapshot);
