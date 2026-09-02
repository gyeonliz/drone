#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/DroneAITypes.h"
#include "DroneNPCWeaponComponent.generated.h"

/**
 * Rifle과 Shotgun AI가 공유하는 최소 무기 호출 계약이다.
 *
 * AI-WPN-01에서는 표적·조준점과 요청 상태만 관리한다. Trace, Pellet, Damage,
 * 탄약, 발사 간격과 표현 자산은 후속 Weapon 구현이 이 계약 뒤에 붙인다.
 */
UCLASS(ClassGroup=(DroneAI), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneNPCWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneNPCWeaponComponent();

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	void ConfigureWeapon(EDroneNPCWeaponType InWeaponType);

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	EDroneNPCWeaponType GetWeaponType() const { return WeaponType; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	bool CanFire(AActor* TargetActor, FVector AimPoint) const;

	/** 공용 발사 요청을 기록한다. 실제 Projectile·Trace·Damage는 아직 생성하지 않는다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	bool StartFire(AActor* TargetActor, FVector AimPoint);

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	void StopFire();

	/** 후속 탄약 구현이 연결될 공용 재장전 요청 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	bool Reload();

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	bool IsFiring() const { return bIsFiring; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	FVector GetCurrentAimPoint() const { return CurrentAimPoint; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetFireRequestCount() const { return FireRequestCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetAcceptedFireRequestCount() const { return AcceptedFireRequestCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetStopFireRequestCount() const { return StopFireRequestCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetReloadRequestCount() const { return ReloadRequestCount; }

private:
	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon")
	EDroneNPCWeaponType WeaponType = EDroneNPCWeaponType::Unarmed;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon")
	TWeakObjectPtr<AActor> CurrentTarget;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon")
	FVector CurrentAimPoint = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon")
	bool bIsFiring = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 FireRequestCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 AcceptedFireRequestCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 StopFireRequestCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 ReloadRequestCount = 0;
};
