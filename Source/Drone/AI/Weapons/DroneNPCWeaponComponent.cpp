#include "AI/Weapons/DroneNPCWeaponComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Health/DroneHealthComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool IsLivingActor(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	const UDroneHealthComponent* Health = Actor->FindComponentByClass<UDroneHealthComponent>();
	return !Health || !Health->IsDead();
}

AController* ResolveDamageInstigator(AActor* OwnerActor)
{
	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	return OwnerPawn ? OwnerPawn->GetController() : nullptr;
}
}

UDroneNPCWeaponComponent::UDroneNPCWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneNPCWeaponComponent::ConfigureWeapon(const EDroneNPCWeaponType InWeaponType)
{
	StopFire();
	WeaponType = InWeaponType;
	CurrentMagazineAmmo = GetMagazineCapacity();
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

void UDroneNPCWeaponComponent::ConfigureDamageGreybox(
	const float InRifleDamage,
	const float InShotgunDamagePerPellet)
{
	RifleDamage = FMath::Max(0.0f, InRifleDamage);
	ShotgunDamagePerPellet = FMath::Max(0.0f, InShotgunDamagePerPellet);
}

void UDroneNPCWeaponComponent::ConfigureMagazineGreybox(
	const int32 InRifleCapacity,
	const int32 InShotgunCapacity)
{
	StopFire();
	RifleMagazineCapacity = FMath::Max(1, InRifleCapacity);
	ShotgunMagazineCapacity = FMath::Max(1, InShotgunCapacity);
	CurrentMagazineAmmo = GetMagazineCapacity();
}

int32 UDroneNPCWeaponComponent::GetMagazineCapacity() const
{
	if (WeaponType == EDroneNPCWeaponType::Rifle)
	{
		return RifleMagazineCapacity;
	}
	if (WeaponType == EDroneNPCWeaponType::Shotgun)
	{
		return ShotgunMagazineCapacity;
	}
	return 0;
}

bool UDroneNPCWeaponComponent::CanFire(AActor* TargetActor, const FVector AimPoint) const
{
	const AActor* OwnerActor = GetOwner();
	const bool bValidRequest = WeaponType != EDroneNPCWeaponType::Unarmed
		&& HasMagazineAmmo()
		&& IsLivingActor(TargetActor)
		&& TargetActor != OwnerActor
		// Owner 없는 객체는 AI-WPN-01의 순수 공용 계약 테스트에만 사용한다.
		&& (!OwnerActor || IsLivingActor(OwnerActor))
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
		if (bIsFiring)
		{
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
	}
	else if (WeaponType == EDroneNPCWeaponType::Shotgun)
	{
		TryFireShotgunVolley();
		if (bIsFiring)
		{
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
	const int32 Capacity = GetMagazineCapacity();
	if (Capacity <= 0
		|| CurrentMagazineAmmo >= Capacity
		|| (GetOwner() && !IsLivingActor(GetOwner())))
	{
		return false;
	}

	// AI-AMMO-01은 시간·예비 탄약 없이 명시적 요청 즉시 완료하는 기능 경계만 만든다.
	StopFire();
	CurrentMagazineAmmo = Capacity;
	LastRifleShotTimeSeconds = -DBL_MAX;
	LastShotgunVolleyTimeSeconds = -DBL_MAX;
	++AcceptedReloadRequestCount;
	++ReloadCompletedEventCount;
	OnReloadCompleted.Broadcast(WeaponType, CurrentMagazineAmmo, Capacity);
	return true;
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
	ConsumeMagazineRound();
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
		UGameplayStatics::ApplyDamage(
			TargetActor,
			RifleDamage,
			ResolveDamageInstigator(OwnerActor),
			OwnerActor,
			nullptr);
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

	// 메시·AnimBP·Niagara·Sound는 이 이벤트를 구독한다. 마지막 탄환도 StopFire 전에 반드시 알린다.
	++WeaponFiredEventCount;
	OnWeaponFired.Broadcast(WeaponType, LastRifleTraceStart, CurrentAimPoint);
	if (!HasMagazineAmmo())
	{
		StopFire();
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
	AActor* TargetActor = CurrentTarget.Get();
	if (!bIsFiring
		|| !TargetActor
		|| !CanFire(TargetActor, TargetActor->GetActorLocation()))
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
	ConsumeMagazineRound();
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
			UGameplayStatics::ApplyDamage(
				TargetActor,
				ShotgunDamagePerPellet,
				ResolveDamageInstigator(OwnerActor),
				OwnerActor,
				nullptr);
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

	// Shotgun은 Pellet마다가 아니라 방아쇠 한 번(Volley)에 표현 이벤트 하나만 보낸다.
	++WeaponFiredEventCount;
	OnWeaponFired.Broadcast(WeaponType, TraceStart, CurrentAimPoint);
	if (!HasMagazineAmmo())
	{
		StopFire();
	}

	return TargetHitsThisVolley > 0;
}

void UDroneNPCWeaponComponent::HandleShotgunFireTimer()
{
	AActor* TargetActor = CurrentTarget.Get();
	if (!bIsFiring
		|| !TargetActor
		|| !CanFire(TargetActor, TargetActor->GetActorLocation()))
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

void UDroneNPCWeaponComponent::ConsumeMagazineRound()
{
	CurrentMagazineAmmo = FMath::Max(0, CurrentMagazineAmmo - 1);
}
