#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DronePlayerProjectile.generated.h"

class UProjectileMovementComponent;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
class ADronePlayerProjectile;

UENUM(BlueprintType)
enum class EDronePlayerProjectileMode : uint8
{
	Direct UMETA(DisplayName="Direct Projectile"),
	Radial UMETA(DisplayName="Radial Grenade")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDronePlayerProjectileImpactSignature,
	ADronePlayerProjectile*, Projectile,
	AActor*, HitActor,
	EDronePlayerProjectileMode, ProjectileMode);

/**
 * 플레이어 UGV가 사용하는 공용 Greybox 투사체다.
 * Direct는 충돌 Actor 한 명, Radial은 충돌 지점 반경에 Unreal 표준 Damage를 전달한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADronePlayerProjectile : public AActor
{
	GENERATED_BODY()

public:
	ADronePlayerProjectile();

	virtual void NotifyHit(
		UPrimitiveComponent* MyComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		bool bSelfMoved,
		FVector HitLocation,
		FVector HitNormal,
		FVector NormalImpulse,
		const FHitResult& Hit) override;

	UFUNCTION(BlueprintCallable, Category="Drone|PlayerWeapon|Projectile")
	void InitializeProjectile(
		EDronePlayerProjectileMode InMode,
		float InDamage,
		float InSpeed,
		float InMaximumTravelDistance,
		float InRadialDamageRadius,
		float InGravityScale);

	UFUNCTION(BlueprintPure, Category="Drone|PlayerWeapon|Projectile")
	EDronePlayerProjectileMode GetProjectileMode() const { return ProjectileMode; }

	UFUNCTION(BlueprintPure, Category="Drone|PlayerWeapon|Projectile")
	float GetProjectileDamage() const { return ProjectileDamage; }

	UFUNCTION(BlueprintPure, Category="Drone|PlayerWeapon|Projectile")
	float GetRadialDamageRadius() const { return RadialDamageRadius; }

	UFUNCTION(BlueprintPure, Category="Drone|PlayerWeapon|Projectile")
	USphereComponent* GetCollisionComponent() const { return CollisionComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|PlayerWeapon|Projectile")
	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }

	UPROPERTY(BlueprintAssignable, Category="Drone|PlayerWeapon|Projectile")
	FDronePlayerProjectileImpactSignature OnProjectileImpact;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|PlayerWeapon|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|PlayerWeapon|Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|PlayerWeapon|Projectile")
	TObjectPtr<UStaticMeshComponent> TrailVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|PlayerWeapon|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
	void ApplyImpactDamage(AActor* HitActor, const FVector& ImpactLocation);

	UPROPERTY(Transient)
	EDronePlayerProjectileMode ProjectileMode = EDronePlayerProjectileMode::Direct;

	UPROPERTY(Transient)
	float ProjectileDamage = 25.0f;

	UPROPERTY(Transient)
	float RadialDamageRadius = 0.0f;

	bool bImpactHandled = false;
};
