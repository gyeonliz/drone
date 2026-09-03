#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/DroneAITypes.h"
#include "AI/Weapons/DroneNPCProjectile.h"
#include "TimerManager.h"
#include "DroneNPCWeaponComponent.generated.h"

/** 실제 한 발/한 Volley가 실행됐을 때 총구 섬광·발사음·Animation을 연결하는 BP 경계다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDroneNPCWeaponFiredSignature,
	EDroneNPCWeaponType, WeaponType,
	FVector, TraceStart,
	FVector, AimPoint);

/** 즉시 Greybox Reload가 완료됐을 때 장전 표현과 UI를 연결하는 BP 경계다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDroneNPCWeaponReloadCompletedSignature,
	EDroneNPCWeaponType, WeaponType,
	int32, CurrentAmmo,
	int32, MagazineCapacity);

/**
 * Rifle과 Shotgun AI가 공유하는 최소 무기 호출 계약이다.
 *
 * 공용 표적·조준점 계약 뒤에 회피 가능한 Projectile 발사를 기본으로 제공한다.
 * 기존 Rifle 단일 Trace와 Shotgun 다중 Pellet Trace는 비교·자동화용 선택 경계로 유지한다.
 * 현재 탄약은 예비 탄약 없이 탄창·즉시 Reload만 검증하는 Greybox이며 최종 표현과 분리한다.
 */
UCLASS(ClassGroup=(DroneAI), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneNPCWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneNPCWeaponComponent();

	/** Rifle은 Trace 한 발마다, Shotgun은 Pellet 수와 무관하게 Volley마다 한 번 발생한다. */
	UPROPERTY(BlueprintAssignable, Category="Drone|AI|Weapon|Visual")
	FDroneNPCWeaponFiredSignature OnWeaponFired;

	/** Reload가 실제로 탄창을 채운 경우에만 발생한다. 가득 찬 탄창의 거절 요청은 발생하지 않는다. */
	UPROPERTY(BlueprintAssignable, Category="Drone|AI|Weapon|Visual")
	FDroneNPCWeaponReloadCompletedSignature OnReloadCompleted;

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	void ConfigureWeapon(EDroneNPCWeaponType InWeaponType);

	/** AI-WPN-02의 시험값이다. 최종 난이도·에셋 규칙과 분리해 조정한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Rifle")
	void ConfigureRifleGreybox(float InRange, float InCooldownSeconds);

	/** AI-WPN-03 시험값이다. Pellet 수와 Spread는 최종 난이도·표현 규칙이 아니다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Shotgun")
	void ConfigureShotgunGreybox(
		float InRange,
		float InCooldownSeconds,
		int32 InPelletCount,
		float InSpreadHalfAngleDegrees);

	/**
	 * 개인 무기 명중률을 결정하는 원뿔 반각을 설정한다.
	 * 0도면 조준점으로 정확히 발사하고 값이 커질수록 원뿔 내부의 무작위 방향으로 더 넓게 퍼진다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Accuracy")
	void ConfigureAccuracyGreybox(
		float InRifleSpreadHalfAngleDegrees,
		float InShotgunSpreadHalfAngleDegrees);

	/**
	 * true면 실제 이동 Projectile, false면 기존 즉시 Trace를 사용한다.
	 * 기본 탄속은 Rifle 4500, Shotgun 3500, MG 5500 cm/s이며 여기서는 개인 무기 두 종류만 설정한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Projectile")
	void ConfigureProjectileBallisticsGreybox(
		bool bInUseProjectileBallistics,
		float InRifleProjectileSpeed,
		float InShotgunProjectileSpeed);

	/** 현재 회색상자 피해량만 따로 조정한다. 기본값은 Rifle 10, Shotgun Pellet당 8이다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Damage")
	void ConfigureDamageGreybox(float InRifleDamage, float InShotgunDamagePerPellet);

	/** AI-AMMO-01 시험용 탄창 용량을 설정하고 현재 장비의 탄창을 가득 채운다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Ammo")
	void ConfigureMagazineGreybox(int32 InRifleCapacity, int32 InShotgunCapacity);

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Damage")
	float GetRifleDamage() const { return RifleDamage; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Damage")
	float GetShotgunDamagePerPellet() const { return ShotgunDamagePerPellet; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	EDroneNPCWeaponType GetWeaponType() const { return WeaponType; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	bool CanFire(AActor* TargetActor, FVector AimPoint) const;

	/** 공용 발사 요청을 기록하고 장비에 맞는 첫 발사와 반복 Timer를 시작한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	bool StartFire(AActor* TargetActor, FVector AimPoint);

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	void StopFire();

	/** 현재 탄창이 비어 있거나 일부 소모됐을 때 즉시 가득 채운다. 예비 탄약·시간은 후속 범위다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	bool Reload();

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Ammo")
	int32 GetCurrentMagazineAmmo() const { return CurrentMagazineAmmo; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Ammo")
	int32 GetMagazineCapacity() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Ammo")
	bool HasMagazineAmmo() const { return CurrentMagazineAmmo > 0; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	bool IsFiring() const { return bIsFiring; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	FVector GetCurrentAimPoint() const { return CurrentAimPoint; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Projectile")
	bool UsesProjectileBallistics() const { return bUseProjectileBallistics; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Projectile")
	float GetRifleProjectileSpeed() const { return RifleProjectileSpeed; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Projectile")
	float GetShotgunProjectileSpeed() const { return ShotgunProjectileSpeed; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Projectile|Debug")
	int32 GetRifleProjectileSpawnCount() const { return RifleProjectileSpawnCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Projectile|Debug")
	int32 GetShotgunProjectileSpawnCount() const { return ShotgunProjectileSpawnCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Projectile|Debug")
	ADroneNPCProjectile* GetLastSpawnedProjectile() const { return LastSpawnedProjectile.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetFireRequestCount() const { return FireRequestCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetAcceptedFireRequestCount() const { return AcceptedFireRequestCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetStopFireRequestCount() const { return StopFireRequestCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetReloadRequestCount() const { return ReloadRequestCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetAcceptedReloadRequestCount() const { return AcceptedReloadRequestCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetWeaponFiredEventCount() const { return WeaponFiredEventCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Debug")
	int32 GetReloadCompletedEventCount() const { return ReloadCompletedEventCount; }

	/** 비교·테스트용 즉시 Rifle Trace다. 일반 StartFire는 현재 Ballistics 설정에 따라 분기한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Rifle")
	bool TryFireRifleShot();

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle")
	float GetRifleRange() const { return RifleRange; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle")
	float GetRifleCooldownSeconds() const { return RifleCooldownSeconds; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Rifle")
	float GetRifleSpreadHalfAngleDegrees() const { return RifleSpreadHalfAngleDegrees; }

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

	/** 비교·테스트용 즉시 Shotgun Trace 묶음이다. 일반 StartFire는 현재 Ballistics 설정에 따라 분기한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon|Shotgun")
	bool TryFireShotgunVolley();

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Shotgun")
	float GetShotgunRange() const { return ShotgunRange; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Shotgun")
	float GetShotgunCooldownSeconds() const { return ShotgunCooldownSeconds; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Shotgun")
	int32 GetShotgunPelletCount() const { return ShotgunPelletCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Shotgun")
	float GetShotgunSpreadHalfAngleDegrees() const { return ShotgunSpreadHalfAngleDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Shotgun|Debug")
	int32 GetShotgunVolleyAttemptCount() const { return ShotgunVolleyAttemptCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Shotgun|Debug")
	int32 GetShotgunPelletTraceCount() const { return ShotgunPelletTraceCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon|Shotgun|Debug")
	int32 GetShotgunTargetHitPelletCount() const { return ShotgunTargetHitPelletCount; }

	/** 자동화가 한 Volley의 실제 Pellet 방향 분리를 확인하는 C++ 전용 조회다. */
	const TArray<FVector>& GetLastShotgunPelletTraceEnds() const { return LastShotgunPelletTraceEnds; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool TryFireRifleRound();
	bool TryFireRifleProjectile();
	void HandleRifleFireTimer();
	void ClearRifleFireTimer();
	bool IsRifleTargetInRange(AActor* TargetActor, const FVector& AimPoint) const;
	bool TryFireShotgunRound();
	bool TryFireShotgunProjectileVolley();
	void HandleShotgunFireTimer();
	void ClearShotgunFireTimer();
	bool IsShotgunTargetInRange(AActor* TargetActor, const FVector& AimPoint) const;
	ADroneNPCProjectile* SpawnProjectile(
		TSubclassOf<ADroneNPCProjectile> InProjectileClass,
		EDroneNPCProjectileSource ProjectileSource,
		AActor* TargetActor,
		const FVector& SpawnLocation,
		const FVector& Direction,
		float Damage,
		float Speed,
		float MaxTravelDistance);

	UFUNCTION()
	void HandleProjectileImpact(
		ADroneNPCProjectile* Projectile,
		EDroneNPCProjectileSource Source,
		AActor* HitActor,
		bool bHitIntendedTarget);

	void ConsumeMagazineRound();

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon")
	EDroneNPCWeaponType WeaponType = EDroneNPCWeaponType::Unarmed;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon")
	TWeakObjectPtr<AActor> CurrentTarget;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon")
	FVector CurrentAimPoint = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon")
	bool bIsFiring = false;

	/** true가 실제 Gameplay 기본값이다. false는 기존 Trace 회귀 테스트와 비교 플레이용이다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Projectile")
	bool bUseProjectileBallistics = true;

	/** Asset을 지정하지 않아도 Native Greybox Projectile이 Spawn된다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Projectile")
	TSubclassOf<ADroneNPCProjectile> ProjectileClass;

	/** 40m에서 약 0.89초가 걸리는 회피 가능한 Rifle Greybox 탄속이다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Projectile", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float RifleProjectileSpeed = 4500.0f;

	/** Shotgun은 근거리 위협을 유지하면서도 이동 중인 Drone이 피할 수 있게 Rifle보다 느리다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Projectile", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float ShotgunProjectileSpeed = 3500.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Projectile Debug")
	int32 RifleProjectileSpawnCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Projectile Debug")
	int32 ShotgunProjectileSpawnCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Projectile Debug")
	TWeakObjectPtr<ADroneNPCProjectile> LastSpawnedProjectile;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 FireRequestCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 AcceptedFireRequestCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 StopFireRequestCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 ReloadRequestCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 AcceptedReloadRequestCount = 0;

	/** BP 이벤트가 실제 발사 횟수와 1:1인지 자동화에서 확인하는 값이다. */
	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 WeaponFiredEventCount = 0;

	/** 거절된 Reload가 표현 이벤트를 만들지 않는지 자동화에서 확인하는 값이다. */
	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Debug")
	int32 ReloadCompletedEventCount = 0;

	/** 예비 탄약과 Reload 시간 없이 사용하는 Rifle Greybox 탄창이다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Ammo", meta=(ClampMin="1"))
	int32 RifleMagazineCapacity = 30;

	/** Pellet 수가 아니라 Trigger 한 번에 Shell 한 발을 소모한다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Ammo", meta=(ClampMin="1"))
	int32 ShotgunMagazineCapacity = 8;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Ammo")
	int32 CurrentMagazineAmmo = 0;

	/** Rifle 기본 Sight 반경과 맞춘 Greybox 사거리다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Rifle", meta=(ClampMin="1.0", ForceUnits="cm"))
	float RifleRange = 4000.0f;

	/** 초당 4발 Greybox 기준이다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Rifle", meta=(ClampMin="0.01", ForceUnits="s"))
	float RifleCooldownSeconds = 0.25f;

	/**
	 * 소총 한 발이 목표 중심 주위에서 벗어날 수 있는 원뿔 반각이다.
	 * 역할 Blueprint의 NPCWeaponComponent에서 조정하며 0이면 정확히 조준점으로 발사한다.
	 */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Rifle", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg"))
	float RifleSpreadHalfAngleDegrees = 2.5f;

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

	/** 체력 100 기준 10발 처치인 회색상자 기본 피해량이다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Damage", meta=(ClampMin="0.0"))
	float RifleDamage = 10.0f;

	/** 근거리 역할을 구분하기 위한 Greybox 사거리다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Shotgun", meta=(ClampMin="1.0", ForceUnits="cm"))
	float ShotgunRange = 1600.0f;

	/** 약 초당 1발인 Greybox 기준이다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Shotgun", meta=(ClampMin="0.01", ForceUnits="s"))
	float ShotgunCooldownSeconds = 0.9f;

	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Shotgun", meta=(ClampMin="1", ClampMax="64"))
	int32 ShotgunPelletCount = 8;

	/** 모든 Pellet이 무작위로 퍼지는 원뿔 반각이다. 0이면 모든 Pellet이 조준점으로 향한다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Shotgun", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg"))
	float ShotgunSpreadHalfAngleDegrees = 6.0f;

	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Shotgun Debug")
	bool bDrawShotgunDebugTrace = true;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Shotgun Debug")
	int32 ShotgunVolleyAttemptCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Shotgun Debug")
	int32 ShotgunPelletTraceCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Shotgun Debug")
	int32 ShotgunTargetHitPelletCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Weapon Shotgun Debug")
	TArray<FVector> LastShotgunPelletTraceEnds;

	double LastShotgunVolleyTimeSeconds = -DBL_MAX;
	FTimerHandle ShotgunFireTimerHandle;

	/** 한 Volley가 아니라 Target에 실제 적중한 Pellet 하나당 적용하는 피해량이다. */
	UPROPERTY(EditAnywhere, Category="Drone AI Weapon Damage", meta=(ClampMin="0.0"))
	float ShotgunDamagePerPellet = 8.0f;
};
