#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneMissionDamageTarget.generated.h"

class AController;
class UBoxComponent;
class UDroneHealthComponent;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FDroneMissionTargetDamagedSignature,
	AActor*, TargetActor,
	float, PreviousHealth,
	float, CurrentHealth,
	float, AppliedDamage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDroneMissionTargetDestroyedSignature,
	AActor*, TargetActor,
	AController*, InstigatorController,
	AActor*, DamageCauser);

/**
 * 차량·재머·방공망·시설처럼 Story Map에서 파괴 목표로 배치하는 공용 표적이다.
 * Mesh/VFX/Sound는 파생 Blueprint가 담당하고 C++은 충돌, 체력, 단 한 번의 파괴 사건을 제공한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneMissionDamageTarget : public AActor
{
	GENERATED_BODY()

public:
	ADroneMissionDamageTarget();

	UFUNCTION(BlueprintPure, Category="Drone|Mission|DamageTarget")
	UBoxComponent* GetTargetCollision() const { return TargetCollision; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission|DamageTarget")
	UStaticMeshComponent* GetTargetVisual() const { return TargetVisual; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission|DamageTarget")
	UDroneHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission|DamageTarget")
	bool IsTargetDestroyed() const;

	UFUNCTION(BlueprintPure, Category="Drone|Mission|DamageTarget")
	float GetMaximumHealth() const { return MaximumHealth; }

	UFUNCTION(BlueprintCallable, Category="Drone|Mission|DamageTarget")
	void ResetMissionTarget();

	UPROPERTY(BlueprintAssignable, Category="Drone|Mission|DamageTarget")
	FDroneMissionTargetDamagedSignature OnMissionTargetDamaged;

	UPROPERTY(BlueprintAssignable, Category="Drone|Mission|DamageTarget")
	FDroneMissionTargetDestroyedSignature OnMissionTargetDestroyed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Mission|DamageTarget", meta=(DisplayName="Mission Target Damaged Visual"))
	void ReceiveMissionTargetDamagedVisual(float PreviousHealth, float CurrentHealth, float AppliedDamage);

	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Mission|DamageTarget", meta=(DisplayName="Mission Target Destroyed Visual"))
	void ReceiveMissionTargetDestroyedVisual(AController* InstigatorController, AActor* DamageCauser);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Mission|DamageTarget")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Mission|DamageTarget")
	TObjectPtr<UBoxComponent> TargetCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Mission|DamageTarget")
	TObjectPtr<UStaticMeshComponent> TargetVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Mission|DamageTarget")
	TObjectPtr<UDroneHealthComponent> HealthComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|DamageTarget", meta=(ClampMin="1.0"))
	float MaximumHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|DamageTarget")
	bool bDisableCollisionWhenDestroyed = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|DamageTarget")
	bool bHideVisualWhenDestroyed = false;

private:
	UFUNCTION()
	void HandleHealthChanged(float PreviousHealth, float CurrentHealth, float MaxHealth, float AppliedDamage);

	UFUNCTION()
	void HandleTargetDeath(AActor* DeadActor, AController* InstigatorController, AActor* DamageCauser);

	void TryRegisterWithMissionDirector();
	void UnregisterFromMissionDirector();
};
