#include "AI/Weapons/DroneNPCWeaponComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UDroneNPCWeaponComponent::UDroneNPCWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneNPCWeaponComponent::ConfigureWeapon(const EDroneNPCWeaponType InWeaponType)
{
	StopFire();
	WeaponType = InWeaponType;
	RifleTraceAttemptCount = 0;
	RifleTargetHitCount = 0;
	LastRifleHitActor.Reset();
	LastRifleTraceStart = FVector::ZeroVector;
	LastRifleTraceEnd = FVector::ZeroVector;
	LastRifleShotTimeSeconds = -DBL_MAX;
}

void UDroneNPCWeaponComponent::ConfigureRifleGreybox(
	const float InRange,
	const float InCooldownSeconds)
{
	RifleRange = FMath::Max(1.0f, InRange);
	RifleCooldownSeconds = FMath::Max(0.01f, InCooldownSeconds);
}

bool UDroneNPCWeaponComponent::CanFire(AActor* TargetActor, const FVector AimPoint) const
{
	return WeaponType != EDroneNPCWeaponType::Unarmed
		&& IsValid(TargetActor)
		&& TargetActor != GetOwner()
		&& !AimPoint.ContainsNaN()
		&& (WeaponType != EDroneNPCWeaponType::Rifle || IsRifleTargetInRange(TargetActor, AimPoint));
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

	if (WeaponType == EDroneNPCWeaponType::Rifle)
	{
		TryFireRifleShot();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RifleFireTimerHandle,
				this,
				&UDroneNPCWeaponComponent::HandleRifleFireTimer,
				RifleCooldownSeconds,
				true);
		}
	}
	return true;
}

void UDroneNPCWeaponComponent::StopFire()
{
	++StopFireRequestCount;
	ClearRifleFireTimer();
	bIsFiring = false;
	CurrentTarget.Reset();
	CurrentAimPoint = FVector::ZeroVector;
}

bool UDroneNPCWeaponComponent::Reload()
{
	++ReloadRequestCount;
	return WeaponType != EDroneNPCWeaponType::Unarmed;
}

bool UDroneNPCWeaponComponent::TryFireRifleShot()
{
	if (WeaponType != EDroneNPCWeaponType::Rifle || !bIsFiring)
	{
		return false;
	}

	AActor* TargetActor = CurrentTarget.Get();
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(TargetActor) || !IsValid(OwnerActor) || !World)
	{
		return false;
	}

	CurrentAimPoint = TargetActor->GetActorLocation();
	if (!CanFire(TargetActor, CurrentAimPoint))
	{
		return false;
	}

	const double CurrentTimeSeconds = World->GetTimeSeconds();
	if (CurrentTimeSeconds - LastRifleShotTimeSeconds + UE_DOUBLE_SMALL_NUMBER < RifleCooldownSeconds)
	{
		return false;
	}

	FRotator ViewRotation = FRotator::ZeroRotator;
	OwnerActor->GetActorEyesViewPoint(LastRifleTraceStart, ViewRotation);
	LastRifleTraceEnd = CurrentAimPoint;
	LastRifleShotTimeSeconds = CurrentTimeSeconds;
	++RifleTraceAttemptCount;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneNPCRifleTrace), true, OwnerActor);
	QueryParams.AddIgnoredActor(OwnerActor);
	FHitResult Hit;
	const bool bBlockingHit = World->LineTraceSingleByChannel(
		Hit,
		LastRifleTraceStart,
		LastRifleTraceEnd,
		ECC_Visibility,
		QueryParams);

	LastRifleHitActor = bBlockingHit ? Hit.GetActor() : nullptr;
	const bool bHitTarget = bBlockingHit && Hit.GetActor() == TargetActor;
	if (bHitTarget)
	{
		++RifleTargetHitCount;
	}

	if (bDrawRifleDebugTrace)
	{
		const FVector DrawEnd = bBlockingHit ? Hit.ImpactPoint : LastRifleTraceEnd;
		DrawDebugLine(
			World,
			LastRifleTraceStart,
			DrawEnd,
			bHitTarget ? FColor::Green : FColor::Red,
			false,
			FMath::Max(0.05f, RifleCooldownSeconds * 0.8f),
			0,
			1.5f);
	}

	return bHitTarget;
}

void UDroneNPCWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRifleFireTimer();
	Super::EndPlay(EndPlayReason);
}

void UDroneNPCWeaponComponent::HandleRifleFireTimer()
{
	if (!bIsFiring || !CurrentTarget.IsValid())
	{
		StopFire();
		return;
	}
	TryFireRifleShot();
}

void UDroneNPCWeaponComponent::ClearRifleFireTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RifleFireTimerHandle);
	}
}

bool UDroneNPCWeaponComponent::IsRifleTargetInRange(
	AActor* TargetActor,
	const FVector& AimPoint) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		// Owner 없는 순수 계약 객체는 기존 AI-WPN-01 단위 테스트 경계를 유지한다.
		return true;
	}
	return IsValid(TargetActor)
		&& FVector::DistSquared(OwnerActor->GetActorLocation(), AimPoint) <= FMath::Square(RifleRange);
}
