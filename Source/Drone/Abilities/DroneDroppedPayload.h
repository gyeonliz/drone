#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneDroppedPayload.generated.h"

class USphereComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UProjectileMovementComponent;
class ADroneDroppedPayload;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDroneDroppedPayloadImpactSignature,
	ADroneDroppedPayload*, PayloadActor,
	AActor*, HitActor,
	bool, bHitIntendedTarget);

/** 중력과 Sweep 충돌을 사용하는 드랍용 Greybox Payload다. */
UCLASS(Blueprintable)
class DRONE_API ADroneDroppedPayload : public AActor
{
	GENERATED_BODY()

public:
	ADroneDroppedPayload();
	virtual void NotifyHit(
		UPrimitiveComponent* MyComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		bool bSelfMoved,
		FVector HitLocation,
		FVector HitNormal,
		FVector NormalImpulse,
		const FHitResult& Hit) override;

	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Payload")
	void InitializePayload(AActor* NewIntendedTarget, float DownwardSpeedCentimetersPerSecond);

	/** 맵 배치 Actor를 Drone 하단 Anchor에 실제로 부착한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Payload|Pickup")
	bool PrepareForCarry(AActor* NewCarrierActor, USceneComponent* CarryAnchor);

	/** 런타임 Spawn Actor도 보급품으로 전환할 수 있는 Blueprint 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Payload|Pickup")
	void ActivateCarryablePickup();

	/** NotifyHit와 자동화가 공유하는 1회 충돌 판정 경로다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Payload|Greybox")
	bool ResolveImpactGreybox(AActor* HitActor);

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload")
	AActor* GetIntendedTarget() const { return IntendedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload")
	bool IsImpactResolved() const { return bImpactResolved; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload")
	bool DidHitIntendedTarget() const { return bHitIntendedTarget; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload|Pickup")
	bool IsAvailableForPickup() const { return bAvailableForPickup; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload|Pickup")
	bool IsCarried() const { return bCarried; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload|Pickup")
	bool DoesStartAsCarryablePickup() const { return bStartsAsCarryablePickup; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload|Presentation")
	UStaticMeshComponent* GetPayloadVisual() const { return PayloadVisual; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload|Presentation")
	UTextRenderComponent* GetPickupLabel() const { return PickupLabel; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop|Payload")
	FDroneDroppedPayloadImpactSignature OnPayloadImpact;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Drop|Payload")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Drop|Payload")
	TObjectPtr<UStaticMeshComponent> PayloadVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Drop|Payload|Pickup")
	TObjectPtr<UTextRenderComponent> PickupLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Drop|Payload")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** 켜면 Level에 놓인 상태로 시작하며 Drone이 가까이서 적재할 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop|Payload|Pickup")
	bool bStartsAsCarryablePickup = false;

	/** 일반 1회용 Payload의 자동 제거 시간이다. Carryable로 활성화된 Actor에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Drop|Payload", meta=(ClampMin="0.1", ForceUnits="s"))
	float DroppedPayloadLifetimeSeconds = 15.0f;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> IntendedTarget;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop|Payload")
	bool bImpactResolved = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop|Payload")
	bool bHitIntendedTarget = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop|Payload|Pickup")
	bool bAvailableForPickup = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop|Payload|Pickup")
	bool bCarried = false;

	/** 한 번이라도 맵 Pickup으로 활성화된 Actor는 착지 후 사라지지 않고 다시 주울 수 있다. */
	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop|Payload|Pickup")
	bool bReusableCarryable = false;
};
