#include "Weapons/DroneGroundWeaponComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Weapons/DronePlayerProjectile.h"

UDroneGroundWeaponComponent::UDroneGroundWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ProjectileClass = ADronePlayerProjectile::StaticClass();
}

void UDroneGroundWeaponComponent::ConfigureFeatureEnabled(const bool bEnabled)
{
	bFeatureEnabled = bEnabled;
	LastPrimaryFireTimeSeconds = -BIG_NUMBER;
	LastSecondaryFireTimeSeconds = -BIG_NUMBER;
	LastSpawnedProjectile.Reset();
}

bool UDroneGroundWeaponComponent::FirePrimary(USceneComponent* MuzzleComponent)
{
	return FireProjectile(MuzzleComponent, false);
}

bool UDroneGroundWeaponComponent::FireSecondary(USceneComponent* MuzzleComponent)
{
	return FireProjectile(MuzzleComponent, true);
}

bool UDroneGroundWeaponComponent::FireProjectile(USceneComponent* MuzzleComponent, const bool bGrenade)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!bFeatureEnabled || !IsValid(MuzzleComponent) || !IsValid(OwnerActor) || !World || !IsCooldownReady(bGrenade))
	{
		return false;
	}

	UClass* ResolvedProjectileClass = ProjectileClass
		? ProjectileClass.Get()
		: ADronePlayerProjectile::StaticClass();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePlayerProjectile* Projectile = World->SpawnActor<ADronePlayerProjectile>(
		ResolvedProjectileClass,
		MuzzleComponent->GetComponentTransform(),
		SpawnParameters);
	if (!Projectile)
	{
		return false;
	}

	Projectile->InitializeProjectile(
		bGrenade ? EDronePlayerProjectileMode::Radial : EDronePlayerProjectileMode::Direct,
		bGrenade ? SecondaryDamage : PrimaryDamage,
		bGrenade ? SecondaryProjectileSpeed : PrimaryProjectileSpeed,
		bGrenade ? SecondaryRangeCentimeters : PrimaryRangeCentimeters,
		bGrenade ? SecondaryDamageRadiusCentimeters : 0.0f,
		bGrenade ? SecondaryGravityScale : 0.0f);

	const float Now = World->GetTimeSeconds();
	if (bGrenade)
	{
		LastSecondaryFireTimeSeconds = Now;
	}
	else
	{
		LastPrimaryFireTimeSeconds = Now;
	}
	LastSpawnedProjectile = Projectile;
	OnGroundWeaponFired.Broadcast(bGrenade, Projectile);
	return true;
}

bool UDroneGroundWeaponComponent::IsCooldownReady(const bool bGrenade) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const float LastFireTime = bGrenade ? LastSecondaryFireTimeSeconds : LastPrimaryFireTimeSeconds;
	const float Interval = bGrenade ? SecondaryFireIntervalSeconds : PrimaryFireIntervalSeconds;
	return World->GetTimeSeconds() + KINDA_SMALL_NUMBER >= LastFireTime + FMath::Max(0.01f, Interval);
}
