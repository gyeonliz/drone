#pragma once

#include "CoreMinimal.h"
#include "AI/DroneAITypes.h"
#include "GameFramework/Actor.h"
#include "DroneNPCProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class ADroneNPCProjectile;

/** Rifle/Shotgun/유인 MG/무인 포탑이 같은 투사체를 사용하면서 결과를 원래 무기로 돌려주기 위한 구분값이다. */
UENUM(BlueprintType)
enum class EDroneNPCProjectileSource : uint8
{
	Rifle,
	Shotgun,
	MGTurret,
	AutomaticTurret
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FDroneNPCProjectileImpactSignature,
	ADroneNPCProjectile*, Projectile,
	EDroneNPCProjectileSource, Source,
	AActor*, HitActor,
	bool, bHitIntendedTarget);

/**
 * 드론이 실제로 보고 피할 수 있는 AI 공용 Greybox 탄환이다.
 *
 * 복잡한 물리 Simulation 대신 ProjectileMovement의 Sweep 충돌만 사용한다. 충돌한 첫 물체에서
 * 제거되며, 현재 무기 계약과 동일하게 처음 지정한 Target에 맞았을 때만 Damage를 적용한다.
 * Mesh/Niagara는 Blueprint 파생 클래스에서 교체할 수 있다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneNPCProjectile : public AActor
{
	GENERATED_BODY()

public:
	ADroneNPCProjectile();
	virtual void NotifyHit(
		UPrimitiveComponent* MyComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		bool bSelfMoved,
		FVector HitLocation,
		FVector HitNormal,
		FVector NormalImpulse,
		const FHitResult& Hit) override;

	/** Spawn 직후 첫 이동 Tick 전에 호출해 역할, 표적, 피해량과 탄속을 확정한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Projectile")
	void InitializeProjectile(
		EDroneNPCProjectileSource InSource,
		AActor* InTargetActor,
		float InDamage,
		float InSpeed,
		float InMaxTravelDistance);

	UFUNCTION(BlueprintPure, Category="Drone|AI|Projectile")
	EDroneNPCProjectileSource GetProjectileSource() const { return ProjectileSource; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Projectile")
	AActor* GetIntendedTarget() const { return IntendedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Projectile")
	float GetProjectileDamage() const { return ProjectileDamage; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Projectile")
	float GetProjectileSpeed() const { return ProjectileSpeed; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Projectile")
	USphereComponent* GetCollisionComponent() const { return CollisionComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Projectile")
	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }

	UPROPERTY(BlueprintAssignable, Category="Drone|AI|Projectile")
	FDroneNPCProjectileImpactSignature OnProjectileImpact;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Projectile|Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	/** 기본 Engine Sphere는 Greybox 확인용이다. 최종 탄환/Tracer Mesh는 BP에서 바꾼다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Projectile|Components")
	TObjectPtr<UStaticMeshComponent> ProjectileVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Projectile|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Projectile")
	EDroneNPCProjectileSource ProjectileSource = EDroneNPCProjectileSource::Rifle;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Projectile")
	TWeakObjectPtr<AActor> IntendedTarget;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Projectile")
	float ProjectileDamage = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Projectile")
	float ProjectileSpeed = 4500.0f;

	bool bImpactHandled = false;
};
