#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DronePayloadDropComponent.generated.h"

class ADroneDroppedPayload;
class UPrimitiveComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneDropViewChangedSignature,
	bool, bDropViewEnabled);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDronePayloadDroppedSignature,
	ADroneDroppedPayload*, PayloadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDronePayloadResolvedSignature,
	ADroneDroppedPayload*, PayloadActor,
	AActor*, HitActor,
	bool, bHitIntendedTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDronePayloadPickedUpSignature,
	ADroneDroppedPayload*, PayloadActor);

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

	/** 적재 중이면 투하하고, 비어 있으면 가장 가까운 맵 배치 화물을 적재한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Drop")
	bool ActivatePrimaryPayloadAction();

	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Pickup")
	ADroneDroppedPayload* TryPickupNearestCarryablePayload();

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Pickup")
	ADroneDroppedPayload* FindBestAvailableCarryablePayload() const;

	/** 지정 대상이 없으면 World에서 가장 가까운 미완료 투하 표적을 찾는다. */
	UFUNCTION(BlueprintPure, Category="Drone|Drop")
	AActor* FindBestAvailablePayloadTarget() const;

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

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Pickup")
	ADroneDroppedPayload* GetCarriedPayloadActor() const { return CarriedPayloadActor.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|Drop")
	TSubclassOf<ADroneDroppedPayload> GetPayloadClass() const { return PayloadClass; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop")
	FDronePayloadDroppedSignature OnPayloadDropped;

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop")
	FDronePayloadResolvedSignature OnPayloadResolved;

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop|Pickup")
	FDronePayloadPickedUpSignature OnPayloadPickedUp;

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop")
	FDroneDropViewChangedSignature OnDropViewChanged;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop", meta=(ClampMin="1"))
	int32 InitialPayloadCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop")
	TSubclassOf<ADroneDroppedPayload> PayloadClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop")
	FVector PayloadSpawnOffset = FVector(0.0f, 0.0f, -65.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float InitialDownwardSpeedCentimetersPerSecond = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop|Presentation")
	FName CarriedPayloadVisualTag = TEXT("DroneCarriedPayload");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop|Pickup", meta=(ClampMin="1.0", ForceUnits="cm"))
	float CarryablePickupRangeCentimeters = 300.0f;

private:
	UFUNCTION()
	void HandlePayloadImpact(ADroneDroppedPayload* PayloadActor, AActor* HitActor, bool bHitIntendedTarget);

	void UpdateCarriedPayloadVisual();
	USceneComponent* FindCarriedPayloadAnchor() const;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop")
	bool bFeatureEnabled = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop")
	int32 RemainingPayloadCount = 0;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> DropTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADroneDroppedPayload> CarriedPayloadActor;

	UPROPERTY(Transient)
	int32 SuccessfulDeliveryCount = 0;
};
