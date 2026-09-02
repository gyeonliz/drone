#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/DroneAITypes.h"
#include "TimerManager.h"
#include "DroneNPCWeaponComponent.generated.h"

/**
 * Rifle과 Shotgun AI가 공유하는 최소 무기 호출 계약이다.
 *
 * 공용 표적·조준점 계약 뒤에 Rifle Greybox 단일 Trace를 제공한다. Shotgun Pellet,
 * Damage, 탄약과 최종 표현 자산은 후속 Weapon 구현이 같은 계약 뒤에 붙인다.
 */
UCLASS(ClassGroup=(DroneAI), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneNPCWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneNPCWeaponComponent();

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	void ConfigureWeapon(EDroneNPCWeaponType InWeaponType);

	/** AI-WPN-02의 시험값이다. 최종 난이도·에셋 규칙과 분리해 조정한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Rifle")
	void ConfigureRifleGreybox(float InRange, float InCooldownSeconds);

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	EDroneNPCWeaponType GetWeaponType() const { return WeaponType; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	bool CanFire(AActor* TargetActor, FVector AimPoint) const;

	/** 공용 발사 요청을 기록하고 Rifle이면 첫 단일 Trace와 반복 Timer를 시작한다. */
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

	/** 현재 Target을 향해 Cooldown이 허용한 Rifle 단일 Trace를 한 번 시도한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Rifle")
	bool TryFireRifleShot();

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle")
	float GetRifleRange() const { return RifleRange; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle")
	float GetRifleCooldownSeconds() const { return RifleCooldownSeconds; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle|Debug")
	int32 GetRifleTraceAttemptCount() const { return RifleTraceAttemptCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle|Debug")
	int32 GetRifleTargetHitCount() const { return RifleTargetHitCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle|Debug")
	AActor* GetLastRifleHitActor() const { return LastRifleHitActor.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle|Debug")
	FVector GetLastRifleTraceStart() const { return LastRifleTraceStart; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle|Debug")
	FVector GetLastRifleTraceEnd() const { return LastRifleTraceEnd; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleRifleFireTimer();
	void ClearRifleFireTimer();
	bool IsRifleTargetInRange(AActor* TargetActor, const FVector& AimPoint) const;

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

	/** Rifle 기본 Sight 반경과 맞춘 Greybox 사거리다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Rifle", meta=(ClampMin="1.0", ForceUnits="cm"))
	float RifleRange = 4000.0f;

	/** 초당 4발 Greybox 기준이다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Rifle", meta=(ClampMin="0.01", ForceUnits="s"))
	float RifleCooldownSeconds = 0.25f;

	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Rifle Debug")
	bool bDrawRifleDebugTrace = true;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Rifle Debug")
	int32 RifleTraceAttemptCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Rifle Debug")
	int32 RifleTargetHitCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Rifle Debug")
	TWeakObjectPtr<AActor> LastRifleHitActor;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Rifle Debug")
	FVector LastRifleTraceStart = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Rifle Debug")
	FVector LastRifleTraceEnd = FVector::ZeroVector;

	double LastRifleShotTimeSeconds = -DBL_MAX;
	FTimerHandle RifleFireTimerHandle;
};
