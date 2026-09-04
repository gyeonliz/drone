#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneHealthComponent.generated.h"

class AController;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FDroneHealthChangedSignature,
	float, PreviousHealth,
	float, CurrentHealth,
	float, MaxHealth,
	float, AppliedDamage);

/** Native C++ 표현 수신용 경로. BlueprintAssignable 계약은 OnHealthChanged에 그대로 유지한다. */
DECLARE_MULTICAST_DELEGATE_FourParams(
	FDroneHealthChangedNativeSignature,
	float,
	float,
	float,
	float);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDroneDeathSignature,
	AActor*, DeadActor,
	AController*, InstigatorController,
	AActor*, DamageCauser);

/**
 * 드론과 NPC가 함께 사용하는 최소 체력 Component다.
 *
 * 무기 종류나 사망 연출을 알지 않고 Damage 수신, 체력 감소, 단 한 번의 사망 알림만
 * 담당한다. 래그돌·리스폰·실패 화면 같은 표현과 게임 규칙은 소유 Actor/BP에서 붙인다.
 */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneHealthComponent();

	UFUNCTION(BlueprintPure, Category="Drone|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="Drone|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Drone|Health")
	float GetHealthNormalized() const;

	UFUNCTION(BlueprintPure, Category="Drone|Health")
	bool IsDead() const { return bDead; }

	/** 표준 ApplyDamage 경로 외에 시험·환경 피해가 직접 호출할 수 있는 공용 진입점이다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Health")
	bool ApplyHealthDamage(float Damage, AController* InstigatorController, AActor* DamageCauser);

	/** 새 Spawn/향후 Respawn에서 최대 체력으로 되돌리는 최소 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Health")
	void ResetHealth();

	UFUNCTION(BlueprintPure, Category="Drone|Health|Debug")
	int32 GetDeathEventCount() const { return DeathEventCount; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Health")
	FDroneHealthChangedSignature OnHealthChanged;

	FDroneHealthChangedNativeSignature OnHealthChangedNative;

	UPROPERTY(BlueprintAssignable, Category="Drone|Health")
	FDroneDeathSignature OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 현재 NPC와 Drone 공통 Greybox 기본값. BP 파생 클래스에서 역할별 조정 가능하다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Health", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|Health")
	bool bDead = false;

private:
	UFUNCTION()
	void HandleOwnerAnyDamage(
		AActor* DamagedActor,
		float Damage,
		const UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser);

	UPROPERTY(Transient)
	int32 DeathEventCount = 0;
};
