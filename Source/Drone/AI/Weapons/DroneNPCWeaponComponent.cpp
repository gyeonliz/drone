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
	ShotgunVolleyAttemptCount = 0;
	ShotgunPelletTraceCount = 0;
	ShotgunTargetHitPelletCount = 0;
	LastShotgunPelletTraceEnds.Reset();
	LastShotgunVolleyTimeSeconds = -DBL_MAX;
}

void UDroneNPCWeaponComponent::ConfigureRifleGreybox(
	const float InRange,
	const float InCooldownSeconds)
{
	RifleRange = FMath::Max(1.0f, InRange);
	RifleCooldownSeconds = FMath::Max(0.01f, InCooldownSeconds);
}

void UDroneNPCWeaponComponent::ConfigureShotgunGreybox(
	const float InRange,
	const float InCooldownSeconds,
	const int32 InPelletCount,
	const float InSpreadHalfAngleDegrees)
{
	ShotgunRange = FMath::Max(1.0f, InRange);
	ShotgunCooldownSeconds = FMath::Max(0.01f, InCooldownSeconds);
	ShotgunPelletCount = FMath::Clamp(InPelletCount, 1, 64);
	ShotgunSpreadHalfAngleDegrees = FMath::Clamp(InSpreadHalfAngleDegrees, 0.0f, 45.0f);
}

bool UDroneNPCWeaponComponent::CanFire(AActor* TargetActor, const FVector AimPoint) const
{
	const bool bValidRequest = WeaponType != EDroneNPCWeaponType::Unarmed
		&& IsValid(TargetActor)
		&& TargetActor != GetOwner()
		&& !AimPoint.ContainsNaN();
	if (!bValidRequest)
	{
		return false;
	}

	if (WeaponType == EDroneNPCWeaponType::Rifle)
	{
		return IsRifleTargetInRange(TargetActor, AimPoint);
	}
	if (WeaponType == EDroneNPCWeaponType::Shotgun)
	{
		return IsShotgunTargetInRange(TargetActor, AimPoint);
	}
	return true;
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
	else if (WeaponType == EDroneNPCWeaponType::Shotgun)
	{
		TryFireShotgunVolley();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ShotgunFireTimerHandle,
				this,
				&UDroneNPCWeaponComponent::HandleShotgunFireTimer,
				ShotgunCooldownSeconds,
				true);
		}
	}
	return true;
}

void UDroneNPCWeaponComponent::StopFire()
{
	++StopFireRequestCount;
	ClearRifleFireTimer();
	ClearShotgunFireTimer();
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
	ClearShotgunFireTimer();
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

bool UDroneNPCWeaponComponent::TryFireShotgunVolley()
{
	if (WeaponType != EDroneNPCWeaponType::Shotgun || !bIsFiring)
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
	if (CurrentTimeSeconds - LastShotgunVolleyTimeSeconds + UE_DOUBLE_SMALL_NUMBER < ShotgunCooldownSeconds)
	{
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	OwnerActor->GetActorEyesViewPoint(TraceStart, ViewRotation);
	const FVector AimDirection = (CurrentAimPoint - TraceStart).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		return false;
	}

	FVector SpreadRight = FVector::RightVector;
	FVector SpreadUp = FVector::UpVector;
	AimDirection.FindBestAxisVectors(SpreadRight, SpreadUp);
	const float SpreadRadians = FMath::DegreesToRadians(ShotgunSpreadHalfAngleDegrees);
	const float SpreadCos = FMath::Cos(SpreadRadians);
	const float SpreadSin = FMath::Sin(SpreadRadians);
	const int32 SafePelletCount = FMath::Clamp(ShotgunPelletCount, 1, 64);

	LastShotgunVolleyTimeSeconds = CurrentTimeSeconds;
	++ShotgunVolleyAttemptCount;
	LastShotgunPelletTraceEnds.Reset(SafePelletCount);
	int32 TargetHitsThisVolley = 0;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneNPCShotgunTrace), true, OwnerActor);
	QueryParams.AddIgnoredActor(OwnerActor);
	for (int32 PelletIndex = 0; PelletIndex < SafePelletCount; ++PelletIndex)
	{
		FVector PelletDirection = AimDirection;
		if (PelletIndex > 0 && SpreadRadians > UE_SMALL_NUMBER)
		{
			// 첫 Pellet은 정확히 중앙, 나머지는 원뿔 가장자리에 균등 배치해
			// 자동화와 실제 Greybox 결과가 실행마다 동일하게 재현되도록 한다.
			const int32 SpreadPelletCount = FMath::Max(1, SafePelletCount - 1);
			const float Azimuth = 2.0f * UE_PI * static_cast<float>(PelletIndex - 1)
				/ static_cast<float>(SpreadPelletCount);
			const FVector RadialDirection = SpreadRight * FMath::Cos(Azimuth)
				+ SpreadUp * FMath::Sin(Azimuth);
			PelletDirection = (AimDirection * SpreadCos + RadialDirection * SpreadSin).GetSafeNormal();
		}

		const FVector TraceEnd = TraceStart + PelletDirection * ShotgunRange;
		LastShotgunPelletTraceEnds.Add(TraceEnd);
		++ShotgunPelletTraceCount;

		FHitResult Hit;
		const bool bBlockingHit = World->LineTraceSingleByChannel(
			Hit,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);
		const bool bHitTarget = bBlockingHit && Hit.GetActor() == TargetActor;
		if (bHitTarget)
		{
			++TargetHitsThisVolley;
			++ShotgunTargetHitPelletCount;
		}

		if (bDrawShotgunDebugTrace)
		{
			DrawDebugLine(
				World,
				TraceStart,
				bBlockingHit ? Hit.ImpactPoint : TraceEnd,
				bHitTarget ? FColor::Green : FColor::Orange,
				false,
				FMath::Max(0.05f, ShotgunCooldownSeconds * 0.8f),
				0,
				1.25f);
		}
	}

	return TargetHitsThisVolley > 0;
}

void UDroneNPCWeaponComponent::HandleShotgunFireTimer()
{
	if (!bIsFiring || !CurrentTarget.IsValid())
	{
		StopFire();
		return;
	}
	TryFireShotgunVolley();
}

void UDroneNPCWeaponComponent::ClearShotgunFireTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotgunFireTimerHandle);
	}
}

bool UDroneNPCWeaponComponent::IsShotgunTargetInRange(
	AActor* TargetActor,
	const FVector& AimPoint) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return true;
	}
	return IsValid(TargetActor)
		&& FVector::DistSquared(OwnerActor->GetActorLocation(), AimPoint) <= FMath::Square(ShotgunRange);
}
