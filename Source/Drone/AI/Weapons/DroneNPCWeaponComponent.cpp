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

/**
 * 조준 중심을 축으로 하는 원뿔 내부에서 이번 탄환의 방향을 하나 고른다.
 * 반각 0도는 자동화와 정확 사격 설정을 위해 중심 방향을 그대로 반환한다.
 */
FVector MakeRandomShotDirection(const FVector& CenterDirection, const float SpreadHalfAngleDegrees)
{
	const FVector SafeCenterDirection = CenterDirection.GetSafeNormal();
	const float SafeSpreadDegrees = FMath::Clamp(SpreadHalfAngleDegrees, 0.0f, 45.0f);
	if (SafeCenterDirection.IsNearlyZero() || SafeSpreadDegrees <= UE_SMALL_NUMBER)
	{
		return SafeCenterDirection;
	}

	return FMath::VRandCone(SafeCenterDirection, FMath::DegreesToRadians(SafeSpreadDegrees));
}
}

UDroneNPCWeaponComponent::UDroneNPCWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ProjectileClass = ADroneNPCProjectile::StaticClass();
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
	RifleProjectileSpawnCount = 0;
	LastRifleShotTimeSeconds = -DBL_MAX;
	ShotgunVolleyAttemptCount = 0;
	ShotgunPelletTraceCount = 0;
	ShotgunTargetHitPelletCount = 0;
	ShotgunProjectileSpawnCount = 0;
	LastSpawnedProjectile.Reset();
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

void UDroneNPCWeaponComponent::ConfigureAccuracyGreybox(
	const float InRifleSpreadHalfAngleDegrees,
	const float InShotgunSpreadHalfAngleDegrees)
{
	RifleSpreadHalfAngleDegrees = FMath::Clamp(InRifleSpreadHalfAngleDegrees, 0.0f, 45.0f);
	ShotgunSpreadHalfAngleDegrees = FMath::Clamp(InShotgunSpreadHalfAngleDegrees, 0.0f, 45.0f);
}

void UDroneNPCWeaponComponent::ConfigureProjectileBallisticsGreybox(
	const bool bInUseProjectileBallistics,
	const float InRifleProjectileSpeed,
	const float InShotgunProjectileSpeed)
{
	bUseProjectileBallistics = bInUseProjectileBallistics;
	RifleProjectileSpeed = FMath::Max(1.0f, InRifleProjectileSpeed);
	ShotgunProjectileSpeed = FMath::Max(1.0f, InShotgunProjectileSpeed);
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
		TryFireRifleRound();
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
		TryFireShotgunRound();
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

bool UDroneNPCWeaponComponent::TryFireRifleRound()
{
	return bUseProjectileBallistics
		? TryFireRifleProjectile()
		: TryFireRifleShot();
}

bool UDroneNPCWeaponComponent::TryFireRifleProjectile()
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

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	OwnerActor->GetActorEyesViewPoint(SpawnLocation, ViewRotation);
	const FVector CenterDirection = (CurrentAimPoint - SpawnLocation).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return false;
	}
	const FVector ShotDirection = MakeRandomShotDirection(CenterDirection, RifleSpreadHalfAngleDegrees);

	ADroneNPCProjectile* Projectile = SpawnProjectile(
		ProjectileClass,
		EDroneNPCProjectileSource::Rifle,
		TargetActor,
		SpawnLocation,
		ShotDirection,
		RifleDamage,
		RifleProjectileSpeed,
		RifleRange);
	if (!Projectile)
	{
		return false;
	}

	LastRifleShotTimeSeconds = CurrentTimeSeconds;
	ConsumeMagazineRound();
	++RifleProjectileSpawnCount;
	++WeaponFiredEventCount;
	OnWeaponFired.Broadcast(WeaponType, SpawnLocation, CurrentAimPoint);
	if (!HasMagazineAmmo())
	{
		StopFire();
	}
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
	const FVector CenterDirection = (CurrentAimPoint - LastRifleTraceStart).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return false;
	}
	const FVector ShotDirection = MakeRandomShotDirection(CenterDirection, RifleSpreadHalfAngleDegrees);
	LastRifleTraceEnd = LastRifleTraceStart + ShotDirection * RifleRange;
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
	TryFireRifleRound();
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

bool UDroneNPCWeaponComponent::TryFireShotgunRound()
{
	return bUseProjectileBallistics
		? TryFireShotgunProjectileVolley()
		: TryFireShotgunVolley();
}

bool UDroneNPCWeaponComponent::TryFireShotgunProjectileVolley()
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

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	OwnerActor->GetActorEyesViewPoint(SpawnLocation, ViewRotation);
	const FVector CenterDirection = (CurrentAimPoint - SpawnLocation).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return false;
	}

	const int32 SafePelletCount = FMath::Clamp(ShotgunPelletCount, 1, 64);

	int32 SpawnedProjectileCount = 0;
	LastShotgunPelletTraceEnds.Reset(SafePelletCount);
	for (int32 PelletIndex = 0; PelletIndex < SafePelletCount; ++PelletIndex)
	{
		// 중앙 명중을 보장하지 않는다. 모든 Pellet이 같은 원뿔 내부에서 독립적으로 방향을 고른다.
		const FVector PelletDirection = MakeRandomShotDirection(
			CenterDirection,
			ShotgunSpreadHalfAngleDegrees);

		LastShotgunPelletTraceEnds.Add(SpawnLocation + PelletDirection * ShotgunRange);
		if (SpawnProjectile(
			ProjectileClass,
			EDroneNPCProjectileSource::Shotgun,
			TargetActor,
			SpawnLocation,
			PelletDirection,
			ShotgunDamagePerPellet,
			ShotgunProjectileSpeed,
			ShotgunRange))
		{
			++SpawnedProjectileCount;
		}
	}

	if (SpawnedProjectileCount == 0)
	{
		return false;
	}

	LastShotgunVolleyTimeSeconds = CurrentTimeSeconds;
	ConsumeMagazineRound();
	++ShotgunVolleyAttemptCount;
	ShotgunProjectileSpawnCount += SpawnedProjectileCount;
	++WeaponFiredEventCount;
	OnWeaponFired.Broadcast(WeaponType, SpawnLocation, CurrentAimPoint);
	if (!HasMagazineAmmo())
	{
		StopFire();
	}
	return true;
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
		const FVector PelletDirection = MakeRandomShotDirection(
			AimDirection,
			ShotgunSpreadHalfAngleDegrees);

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
	TryFireShotgunRound();
}

ADroneNPCProjectile* UDroneNPCWeaponComponent::SpawnProjectile(
	const TSubclassOf<ADroneNPCProjectile> InProjectileClass,
	const EDroneNPCProjectileSource ProjectileSource,
	AActor* TargetActor,
	const FVector& SpawnLocation,
	const FVector& Direction,
	const float Damage,
	const float Speed,
	const float MaxTravelDistance)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || !IsValid(TargetActor) || Direction.IsNearlyZero())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* ResolvedProjectileClass = InProjectileClass
		? InProjectileClass.Get()
		: ADroneNPCProjectile::StaticClass();
	ADroneNPCProjectile* Projectile = World->SpawnActor<ADroneNPCProjectile>(
		ResolvedProjectileClass,
		SpawnLocation,
		Direction.Rotation(),
		SpawnParameters);
	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->InitializeProjectile(
		ProjectileSource,
		TargetActor,
		Damage,
		Speed,
		MaxTravelDistance);
	Projectile->OnProjectileImpact.AddDynamic(this, &UDroneNPCWeaponComponent::HandleProjectileImpact);
	LastSpawnedProjectile = Projectile;
	return Projectile;
}

void UDroneNPCWeaponComponent::HandleProjectileImpact(
	ADroneNPCProjectile* Projectile,
	const EDroneNPCProjectileSource Source,
	AActor* HitActor,
	const bool bHitIntendedTarget)
{
	if (Source == EDroneNPCProjectileSource::Rifle)
	{
		LastRifleHitActor = HitActor;
		if (bHitIntendedTarget)
		{
			++RifleTargetHitCount;
		}
	}
	else if (Source == EDroneNPCProjectileSource::Shotgun && bHitIntendedTarget)
	{
		++ShotgunTargetHitPelletCount;
	}
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
