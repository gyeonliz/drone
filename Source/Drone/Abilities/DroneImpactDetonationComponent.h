#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneImpactDetonationComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDroneImpactArmedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneImpactDetonatedSignature,
	FVector, ExplosionLocation,
	AActor*, HitActor);

/** FPV 역할의 명시적 Arm → 유효 속도 충돌 → 1회 폭발/기체 파괴 Greybox다. */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneImpactDetonationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneImpactDetonationComponent();

	void ConfigureFeatureEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Drone|FPV|Impact")
	bool ArmImpactDetonation();

	UFUNCTION(BlueprintCallable, Category="Drone|FPV|Impact")
	void DisarmImpactDetonation();

	/** Owner Hit Event와 테스트가 공유하는 단일 판정 경로다. */
	UFUNCTION(BlueprintCallable, Category="Drone|FPV|Impact")
	bool TryDetonateFromImpact(AActor* HitActor, float ImpactSpeedCentimetersPerSecond);

	UFUNCTION(BlueprintPure, Category="Drone|FPV|Impact")
	bool IsFeatureEnabled() const { return bFeatureEnabled; }

	UFUNCTION(BlueprintPure, Category="Drone|FPV|Impact")
	bool IsArmed() const { return bArmed; }

	UFUNCTION(BlueprintPure, Category="Drone|FPV|Impact")
	bool HasDetonated() const { return bDetonated; }

	UFUNCTION(BlueprintPure, Category="Drone|FPV|Impact|Debug")
	int32 GetDetonationCount() const { return DetonationCount; }

	UFUNCTION(BlueprintCallable, Category="Drone|FPV|Impact|Greybox")
	void ConfigureImpactGreybox(float NewMinimumImpactSpeed, float NewExplosionDamage, float NewExplosionRadius);

	UPROPERTY(BlueprintAssignable, Category="Drone|FPV|Impact")
	FDroneImpactArmedSignature OnImpactDetonationArmed;

	UPROPERTY(BlueprintAssignable, Category="Drone|FPV|Impact")
	FDroneImpactDetonatedSignature OnImpactDetonated;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|FPV|Impact", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumImpactSpeedCentimetersPerSecond = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|FPV|Impact", meta=(ClampMin="0.0"))
	float ExplosionDamage = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|FPV|Impact", meta=(ClampMin="1.0", ForceUnits="cm"))
	float ExplosionRadiusCentimeters = 350.0f;

private:
	UFUNCTION()
	void HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|FPV|Impact")
	bool bFeatureEnabled = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|FPV|Impact")
	bool bArmed = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|FPV|Impact")
	bool bDetonated = false;

	UPROPERTY(Transient)
	int32 DetonationCount = 0;
};
