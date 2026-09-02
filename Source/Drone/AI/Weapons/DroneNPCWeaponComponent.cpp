#include "AI/Weapons/DroneNPCWeaponComponent.h"

UDroneNPCWeaponComponent::UDroneNPCWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneNPCWeaponComponent::ConfigureWeapon(const EDroneNPCWeaponType InWeaponType)
{
	StopFire();
	WeaponType = InWeaponType;
}

bool UDroneNPCWeaponComponent::CanFire(AActor* TargetActor, const FVector AimPoint) const
{
	return WeaponType != EDroneNPCWeaponType::Unarmed
		&& IsValid(TargetActor)
		&& TargetActor != GetOwner()
		&& !AimPoint.ContainsNaN();
}

bool UDroneNPCWeaponComponent::StartFire(AActor* TargetActor, const FVector AimPoint)
{
	++FireRequestCount;
	if (!CanFire(TargetActor, AimPoint))
	{
		return false;
	}

	CurrentTarget = TargetActor;
	CurrentAimPoint = AimPoint;
	bIsFiring = true;
	++AcceptedFireRequestCount;
	return true;
}

void UDroneNPCWeaponComponent::StopFire()
{
	++StopFireRequestCount;
	bIsFiring = false;
	CurrentTarget.Reset();
	CurrentAimPoint = FVector::ZeroVector;
}

bool UDroneNPCWeaponComponent::Reload()
{
	++ReloadRequestCount;
	return WeaponType != EDroneNPCWeaponType::Unarmed;
}
