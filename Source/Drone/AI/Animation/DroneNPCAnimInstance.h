#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DroneNPCAnimInstance.generated.h"

/**
 * 프로젝트 소유 NPC AnimBP가 Gameplay 상태를 재구현하지 않고 시선 표현값만 읽는 경계다.
 * AIController가 Target 수명·제한·보간을 담당하고 이 클래스는 Bone별 회전으로 분배한다.
 */
UCLASS(Blueprintable, Transient)
class DRONE_API UDroneNPCAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Drone|AI|Gaze")
	FRotator DroneLookRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Drone|AI|Gaze")
	FRotator DroneLookSpineRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Drone|AI|Gaze")
	FRotator DroneLookNeckRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Drone|AI|Gaze")
	FRotator DroneLookHeadRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Drone|AI|Gaze")
	float DroneLookAlpha = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Drone|AI|Gaze")
	bool bHasDroneLookTarget = false;

	/** 총 시선 회전 중 척추에 분배할 비율. AnimBP Class Defaults에서 체형에 맞춰 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze|BoneWeights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SpineLookWeight = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze|BoneWeights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float NeckLookWeight = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze|BoneWeights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HeadLookWeight = 0.35f;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<class ADroneNPCAIController> CachedDroneController;
};
