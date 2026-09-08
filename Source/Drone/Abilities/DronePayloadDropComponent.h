#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DronePayloadDropComponent.generated.h"

class ADroneDroppedPayload;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDronePayloadDroppedSignature,
	ADroneDroppedPayload*, PayloadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDronePayloadResolvedSignature,
	ADroneDroppedPayload*, PayloadActor,
	AActor*, HitActor,
	bool, bHitIntendedTarget);

/** 드랍 역할의 탑뷰 요청, 1회 Payload Spawn과 목표 접촉 결과를 소유한다. */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDronePayloadDropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDronePayloadDropComponent();

	void ConfigureFeatureEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Drone|Drop")
	void SetDropTarget(AActor* NewTargetActor);

	UFUNCTION(BlueprintCallable, Category="Drone|Drop")
	void SetDropViewEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Drone|Drop")
	ADroneDroppedPayload* DropPayload();

	/** Mission 재시작/보급 지점에서만 호출할 Greybox 재장전 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Greybox")
	void ReloadPayloadsForMission();

	UFUNCTION(BlueprintPure, Category="Drone|Drop")
	bool IsFeatureEnabled() const { return bFeatureEnabled; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop")
	int32 GetRemainingPayloadCount() const { return RemainingPayloadCount; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop")
	AActor* GetDropTarget() const { return DropTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Debug")
	int32 GetSuccessfulDeliveryCount() const { return SuccessfulDeliveryCount; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop")
	FDronePayloadDroppedSignature OnPayloadDropped;

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop")
	FDronePayloadResolvedSignature OnPayloadResolved;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop", meta=(ClampMin="1"))
	int32 InitialPayloadCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop")
	TSubclassOf<ADroneDroppedPayload> PayloadClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop")
	FVector PayloadSpawnOffset = FVector(0.0f, 0.0f, -65.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float InitialDownwardSpeedCentimetersPerSecond = 300.0f;

private:
	UFUNCTION()
	void HandlePayloadImpact(ADroneDroppedPayload* PayloadActor, AActor* HitActor, bool bHitIntendedTarget);

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop")
	bool bFeatureEnabled = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop")
	int32 RemainingPayloadCount = 0;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> DropTarget;

	UPROPERTY(Transient)
	int32 SuccessfulDeliveryCount = 0;
};
