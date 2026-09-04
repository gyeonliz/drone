#include "AI/DroneSmartObjectStation.h"

#include "AI/DroneAITags.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Health/DroneHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SmartObjectComponent.h"
#include "SmartObjectDefinition.h"

namespace
{
bool IsLivingStationActor(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	const UDroneHealthComponent* Health = Actor->FindComponentByClass<UDroneHealthComponent>();
	return !Health || !Health->IsDead();
}

FVector MakeRandomMGTurretShotDirection(
	const FVector& CenterDirection,
	const float SpreadHalfAngleDegrees)
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

ADroneSmartObjectStation::ADroneSmartObjectStation()
{
	PrimaryActorTick.bCanEverTick = false;
	MGTurretProjectileClass = ADroneNPCProjectile::StaticClass();

	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	SetRootComponent(StationRoot);

	SmartObjectComponent = CreateDefaultSubobject<USmartObjectComponent>(TEXT("SmartObjectComponent"));
	SmartObjectComponent->SetupAttachment(StationRoot);

	// Definition Slot Transform을 배치할 때 Actor +X 방향을 확인하는 Editor 표식이다.
	SlotFacingPreview = CreateDefaultSubobject<UArrowComponent>(TEXT("SlotFacingPreview"));
	// SmartObjectComponent의 상대 위치·회전을 Blueprint에서 조정하면 실제 Slot과
	// Preview가 항상 함께 움직이도록 같은 Component 아래에 둔다.
	SlotFacingPreview->SetupAttachment(SmartObjectComponent);
	SlotFacingPreview->ArrowColor = FColor::Cyan;
	SlotFacingPreview->ArrowSize = 1.5f;
	SlotFacingPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SlotFacingPreview->SetCanEverAffectNavigation(false);
}

float ADroneSmartObjectStation::GetMGTurretCurrentYawDegrees() const
{
	return MGTurretYawPivot ? MGTurretYawPivot->GetRelativeRotation().Yaw : 0.0f;
}

float ADroneSmartObjectStation::GetMGTurretCurrentPitchDegrees() const
{
	return MGTurretAimPivot ? MGTurretAimPivot->GetRelativeRotation().Pitch : 0.0f;
}

void ADroneSmartObjectStation::SetSmartObjectDefinition(USmartObjectDefinition* Definition)
{
	SmartObjectComponent->SetDefinition(Definition);
}

USmartObjectDefinition* ADroneSmartObjectStation::GetSmartObjectDefinition() const
{
	return const_cast<USmartObjectDefinition*>(SmartObjectComponent->GetBaseDefinition());
}

FGameplayTag ADroneSmartObjectStation::GetExpectedActivityTag() const
{
	return DroneAITags::GetActivityTag(Activity);
}

bool ADroneSmartObjectStation::HasSmartObjectDefinition() const
{
	return SmartObjectComponent && SmartObjectComponent->GetBaseDefinition() != nullptr;
}

bool ADroneSmartObjectStation::BeginMGTurretUse(AActor* UserActor, AActor* TargetActor)
{
	if (Activity != EDroneSmartObjectActivity::MGTurret
		|| !IsLivingStationActor(UserActor)
		|| !IsLivingStationActor(TargetActor)
		|| UserActor == TargetActor)
	{
		return false;
	}

	if (bMGTurretInUse && MGTurretUser.IsValid() && MGTurretUser.Get() != UserActor)
	{
		return false;
	}

	const bool bNewOccupation = !bMGTurretInUse || MGTurretUser.Get() != UserActor;
	bMGTurretInUse = true;
	MGTurretUser = UserActor;
	MGTurretTarget = TargetActor;
	if (bNewOccupation)
	{
		++MGTurretOccupationCount;
		LastMGTurretShotTimeSeconds = -DBL_MAX;
		OnMGTurretUseChanged.Broadcast(UserActor, TargetActor, true);
	}

	if (!UpdateMGTurretUse(UserActor, TargetActor))
	{
		EndMGTurretUse(UserActor);
		return false;
	}
	return true;
}

bool ADroneSmartObjectStation::UpdateMGTurretUse(AActor* UserActor, AActor* TargetActor)
{
	UWorld* World = GetWorld();
	if (!World
		|| !bMGTurretInUse
		|| MGTurretUser.Get() != UserActor
		|| !IsLivingStationActor(UserActor)
		|| !IsLivingStationActor(TargetActor)
		|| !MGTurretBaseMount
		|| !MGTurretYawPivot
		|| !MGTurretAimPivot
		|| !MGTurretMuzzle)
	{
		return false;
	}
	MGTurretTarget = TargetActor;
	MGTurretAimPoint = TargetActor->GetActorLocation();
	const FVector PivotLocation = MGTurretAimPivot->GetComponentLocation();
	const FVector InitialAimDirection = (MGTurretAimPoint - PivotLocation).GetSafeNormal();
	if (InitialAimDirection.IsNearlyZero()
		|| FVector::DistSquared(PivotLocation, MGTurretAimPoint) > FMath::Square(MGTurretRange))
	{
		return false;
	}

	// 고정 하단부는 유지하고, 몸체는 Yaw만, 포신은 Pitch만 담당한다.
	// BaseMount의 +X가 포탑 정면이므로 공급 에셋 축 보정은 BP에서 BaseMount 또는 각 Mesh에 적용한다.
	FRotator DesiredLocalAimRotation = MGTurretBaseMount->GetComponentTransform()
		.InverseTransformVectorNoScale(InitialAimDirection)
		.Rotation();
	DesiredLocalAimRotation.Normalize();
	const float DesiredYaw = FMath::Clamp(
		FMath::UnwindDegrees(DesiredLocalAimRotation.Yaw),
		-MGTurretMaxYawDegrees,
		MGTurretMaxYawDegrees);
	const float DesiredPitch = FMath::Clamp(
		FMath::UnwindDegrees(DesiredLocalAimRotation.Pitch),
		-MGTurretMaxPitchDownDegrees,
		MGTurretMaxPitchUpDegrees);
	const float DeltaSeconds = FMath::Max(0.0f, World->GetDeltaSeconds());
	const float CurrentYaw = MGTurretYawPivot->GetRelativeRotation().Yaw;
	const float CurrentPitch = MGTurretAimPivot->GetRelativeRotation().Pitch;
	const float InterpolatedYaw = FMath::RInterpTo(
		FRotator(0.0f, CurrentYaw, 0.0f),
		FRotator(0.0f, DesiredYaw, 0.0f),
		DeltaSeconds,
		MGTurretAimInterpolationSpeed).Yaw;
	MGTurretYawPivot->SetRelativeRotation(FRotator(
		0.0f,
		InterpolatedYaw,
		0.0f));
	MGTurretAimPivot->SetRelativeRotation(FRotator(
		FMath::FInterpTo(CurrentPitch, DesiredPitch, DeltaSeconds, MGTurretAimInterpolationSpeed),
		0.0f,
		0.0f));

	const FVector TraceStart = MGTurretMuzzle->GetComponentLocation();
	const FVector AimDirection = MGTurretAimPivot->GetForwardVector().GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		return false;
	}
	const float AimDot = FMath::Clamp(FVector::DotProduct(AimDirection, InitialAimDirection), -1.0f, 1.0f);
	const float AlignmentErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(AimDot));
	MGTurretAlignmentErrorDegrees = AlignmentErrorDegrees;
	if (AlignmentErrorDegrees > MGTurretFireAlignmentToleranceDegrees)
	{
		return true;
	}
	// Pivot은 목표 중심을 계속 바라보되 실제 탄환만 설정된 원뿔 안에서 흔들리게 한다.
	const FVector ShotDirection = MakeRandomMGTurretShotDirection(
		AimDirection,
		MGTurretSpreadHalfAngleDegrees);

	const double CurrentTimeSeconds = World->GetTimeSeconds();
	if (CurrentTimeSeconds - LastMGTurretShotTimeSeconds + UE_DOUBLE_SMALL_NUMBER < MGTurretCooldownSeconds)
	{
		return true;
	}

	const FVector TraceEnd = TraceStart + ShotDirection * MGTurretRange;
	if (bUseMGTurretProjectileBallistics)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.Instigator = Cast<APawn>(UserActor);
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* ResolvedProjectileClass = MGTurretProjectileClass
			? MGTurretProjectileClass.Get()
			: ADroneNPCProjectile::StaticClass();
		ADroneNPCProjectile* Projectile = World->SpawnActor<ADroneNPCProjectile>(
			ResolvedProjectileClass,
			TraceStart,
			ShotDirection.Rotation(),
			SpawnParameters);
		if (!Projectile)
		{
			return true;
		}

		Projectile->InitializeProjectile(
			GetMGTurretProjectileSource(),
			TargetActor,
			MGTurretDamage,
			MGTurretProjectileSpeed,
			MGTurretRange);
		Projectile->GetCollisionComponent()->IgnoreActorWhenMoving(UserActor, true);
		if (AActor* AttachParent = GetAttachParentActor())
		{
			Projectile->GetCollisionComponent()->IgnoreActorWhenMoving(AttachParent, true);
		}
		if (AActor* OwnerActor = GetOwner())
		{
			Projectile->GetCollisionComponent()->IgnoreActorWhenMoving(OwnerActor, true);
		}
		Projectile->OnProjectileImpact.AddDynamic(
			this,
			&ADroneSmartObjectStation::HandleMGTurretProjectileImpact);
		LastMGTurretProjectile = Projectile;
		LastMGTurretShotTimeSeconds = CurrentTimeSeconds;
		++MGTurretProjectileSpawnCount;
		OnMGTurretShot.Broadcast(TraceStart, TraceEnd, nullptr);
		return true;
	}

	LastMGTurretShotTimeSeconds = CurrentTimeSeconds;
	++MGTurretTraceAttemptCount;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneMGTurretTrace), true, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(UserActor);
	if (AActor* AttachParent = GetAttachParentActor())
	{
		QueryParams.AddIgnoredActor(AttachParent);
	}
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}
	FHitResult Hit;
	const bool bBlockingHit = World->LineTraceSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams);
	AActor* HitActor = bBlockingHit ? Hit.GetActor() : nullptr;
	const bool bHitTarget = HitActor == TargetActor;
	if (bHitTarget)
	{
		++MGTurretTargetHitCount;
		const APawn* UserPawn = Cast<APawn>(UserActor);
		UGameplayStatics::ApplyDamage(
			TargetActor,
			MGTurretDamage,
			UserPawn ? UserPawn->GetController() : nullptr,
			this,
			nullptr);
	}

	const FVector ActualTraceEnd = bBlockingHit ? Hit.ImpactPoint : TraceEnd;
	if (bDrawMGTurretDebugTrace)
	{
		DrawDebugLine(
			World,
			TraceStart,
			ActualTraceEnd,
			bHitTarget ? FColor::Green : FColor::Red,
			false,
			FMath::Max(0.05f, MGTurretCooldownSeconds * 0.8f),
			0,
			2.0f);
	}
	OnMGTurretShot.Broadcast(TraceStart, ActualTraceEnd, HitActor);
	return true;
}

void ADroneSmartObjectStation::EndMGTurretUse(AActor* UserActor)
{
	if (!bMGTurretInUse || (UserActor && MGTurretUser.IsValid() && MGTurretUser.Get() != UserActor))
	{
		return;
	}

	AActor* PreviousUser = MGTurretUser.Get();
	AActor* PreviousTarget = MGTurretTarget.Get();
	bMGTurretInUse = false;
	MGTurretUser.Reset();
	MGTurretTarget.Reset();
	MGTurretAimPoint = FVector::ZeroVector;
	MGTurretAlignmentErrorDegrees = 180.0f;
	++MGTurretReleaseCount;
	OnMGTurretUseChanged.Broadcast(PreviousUser, PreviousTarget, false);
}

void ADroneSmartObjectStation::ConfigureMGTurretGreybox(
	const float InRange,
	const float InCooldownSeconds)
{
	MGTurretRange = FMath::Max(1.0f, InRange);
	MGTurretCooldownSeconds = FMath::Max(0.01f, InCooldownSeconds);
}

void ADroneSmartObjectStation::ConfigureMGTurretDamageGreybox(const float InDamage)
{
	MGTurretDamage = FMath::Max(0.0f, InDamage);
}

void ADroneSmartObjectStation::ConfigureMGTurretProjectileGreybox(
	const bool bInUseProjectileBallistics,
	const float InProjectileSpeed)
{
	bUseMGTurretProjectileBallistics = bInUseProjectileBallistics;
	MGTurretProjectileSpeed = FMath::Max(1.0f, InProjectileSpeed);
}

void ADroneSmartObjectStation::ConfigureMGTurretAccuracyGreybox(
	const float InSpreadHalfAngleDegrees)
{
	MGTurretSpreadHalfAngleDegrees = FMath::Clamp(InSpreadHalfAngleDegrees, 0.0f, 45.0f);
}

void ADroneSmartObjectStation::HandleMGTurretProjectileImpact(
	ADroneNPCProjectile* Projectile,
	const EDroneNPCProjectileSource Source,
	AActor* HitActor,
	const bool bHitIntendedTarget)
{
	if (Source == GetMGTurretProjectileSource() && bHitIntendedTarget)
	{
		++MGTurretTargetHitCount;
	}
}

void ADroneSmartObjectStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndMGTurretUse(nullptr);
	Super::EndPlay(EndPlayReason);
}
