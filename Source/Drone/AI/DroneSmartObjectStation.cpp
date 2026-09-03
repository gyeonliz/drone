#include "AI/DroneSmartObjectStation.h"

#include "AI/DroneAITags.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
}

ADroneSmartObjectStation::ADroneSmartObjectStation()
{
	PrimaryActorTick.bCanEverTick = false;

	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	SetRootComponent(StationRoot);

	MGTurretAimPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MGTurretAimPivot"));
	MGTurretAimPivot->SetupAttachment(StationRoot);

	// 외형 Asset은 프로젝트 소유 BP에서 지정한다. C++는 ThirdParty 경로를 하드코딩하지 않는다.
	StationMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("StationMesh"));
	StationMesh->SetupAttachment(MGTurretAimPivot);
	StationMesh->SetSimulatePhysics(false);

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

void ADroneSmartObjectStation::SetSmartObjectDefinition(USmartObjectDefinition* Definition)
{
	SmartObjectComponent->SetDefinition(Definition);
}

USmartObjectDefinition* ADroneSmartObjectStation::GetSmartObjectDefinition() const
{
	return const_cast<USmartObjectDefinition*>(SmartObjectComponent->GetBaseDefinition());
}

void ADroneSmartObjectStation::SetStationSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	StationMesh->SetSkeletalMeshAsset(SkeletalMesh);
}

USkeletalMesh* ADroneSmartObjectStation::GetStationSkeletalMesh() const
{
	return StationMesh->GetSkeletalMeshAsset();
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
		|| !MGTurretAimPivot)
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

	// 최종 Skeletal Mesh의 Bone 이름에 의존하지 않는 Greybox 회전이다.
	MGTurretAimPivot->SetWorldRotation(InitialAimDirection.Rotation());
	const FVector TraceStart = MGTurretAimPivot->GetComponentTransform().TransformPosition(MGTurretMuzzleOffset);
	const FVector AimDirection = (MGTurretAimPoint - TraceStart).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		return false;
	}

	const double CurrentTimeSeconds = World->GetTimeSeconds();
	if (CurrentTimeSeconds - LastMGTurretShotTimeSeconds + UE_DOUBLE_SMALL_NUMBER < MGTurretCooldownSeconds)
	{
		return true;
	}

	LastMGTurretShotTimeSeconds = CurrentTimeSeconds;
	++MGTurretTraceAttemptCount;
	const FVector TraceEnd = TraceStart + AimDirection * MGTurretRange;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneMGTurretTrace), true, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(UserActor);
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

void ADroneSmartObjectStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndMGTurretUse(nullptr);
	Super::EndPlay(EndPlayReason);
}
