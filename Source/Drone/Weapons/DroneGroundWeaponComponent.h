#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneGroundWeaponComponent.generated.h"

class ADronePlayerProjectile;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneGroundWeaponFiredSignature,
	bool, bGrenade,
	ADronePlayerProjectile*, Projectile);

/**
 * GroundDrive UGV의 플레이어 입력 전용 무장 Component다.
 * AI 표적/연사 상태를 가진 NPC Weapon과 분리하고 Pawn의 상부 Muzzle Transform만 받는다.
 */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneGroundWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneGroundWeaponComponent();

	UFUNCTION(BlueprintCallable, Category="Drone|GroundWeapon")
	void ConfigureFeatureEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Drone|GroundWeapon")
	bool IsFeatureEnabled() const { return bFeatureEnabled; }

	UFUNCTION(BlueprintCallable, Category="Drone|GroundWeapon")
	bool FirePrimary(USceneComponent* MuzzleComponent);

	UFUNCTION(BlueprintCallable, Category="Drone|GroundWeapon")
	bool FireSecondary(USceneComponent* MuzzleComponent);

	UFUNCTION(BlueprintPure, Category="Drone|GroundWeapon")
	float GetPrimaryDamage() const { return PrimaryDamage; }

	UFUNCTION(BlueprintPure, Category="Drone|GroundWeapon")
	float GetSecondaryDamage() const { return SecondaryDamage; }

	UFUNCTION(BlueprintPure, Category="Drone|GroundWeapon")
	ADronePlayerProjectile* GetLastSpawnedProjectile() const { return LastSpawnedProjectile.Get(); }

	UPROPERTY(BlueprintAssignable, Category="Drone|GroundWeapon")
	FDroneGroundWeaponFiredSignature OnGroundWeaponFired;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Primary", meta=(ClampMin="0.0"))
	float PrimaryDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Primary", meta=(ClampMin="0.01", ForceUnits="s"))
	float PrimaryFireIntervalSeconds = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Primary", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float PrimaryProjectileSpeed = 6000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Primary", meta=(ClampMin="1.0", ForceUnits="cm"))
	float PrimaryRangeCentimeters = 8000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Secondary", meta=(ClampMin="0.0"))
	float SecondaryDamage = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Secondary", meta=(ClampMin="0.01", ForceUnits="s"))
	float SecondaryFireIntervalSeconds = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Secondary", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float SecondaryProjectileSpeed = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Secondary", meta=(ClampMin="1.0", ForceUnits="cm"))
	float SecondaryRangeCentimeters = 6000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Secondary", meta=(ClampMin="1.0", ForceUnits="cm"))
	float SecondaryDamageRadiusCentimeters = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon|Secondary", meta=(ClampMin="0.0"))
	float SecondaryGravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|GroundWeapon")
	TSubclassOf<ADronePlayerProjectile> ProjectileClass;

private:
	bool FireProjectile(USceneComponent* MuzzleComponent, bool bGrenade);
	bool IsCooldownReady(bool bGrenade) const;

	UPROPERTY(Transient)
	bool bFeatureEnabled = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADronePlayerProjectile> LastSpawnedProjectile;

	float LastPrimaryFireTimeSeconds = -BIG_NUMBER;
	float LastSecondaryFireTimeSeconds = -BIG_NUMBER;
};
