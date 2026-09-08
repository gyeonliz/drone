#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneDroppedPayload.generated.h"

class USphereComponent;
class UStaticMeshComponent;
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

	/** NotifyHit와 자동화가 공유하는 1회 충돌 판정 경로다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Payload|Greybox")
	bool ResolveImpactGreybox(AActor* HitActor);

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload")
	AActor* GetIntendedTarget() const { return IntendedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload")
	bool IsImpactResolved() const { return bImpactResolved; }

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Payload")
	bool DidHitIntendedTarget() const { return bHitIntendedTarget; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop|Payload")
	FDroneDroppedPayloadImpactSignature OnPayloadImpact;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Drop|Payload")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Drop|Payload")
	TObjectPtr<UStaticMeshComponent> PayloadVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Drop|Payload")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> IntendedTarget;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop|Payload")
	bool bImpactResolved = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop|Payload")
	bool bHitIntendedTarget = false;
};
