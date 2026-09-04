#include "AI/DroneAutomaticTurret.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Health/DroneHealthComponent.h"
#include "Prototype/DronePrototypePawn.h"

ADroneAutomaticTurret::ADroneAutomaticTurret()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 자동포탑은 Smart Object Slot과 사수 위치를 사용하지 않는다. Component는 기존 MG
	// 회전 계층 호환을 위해 남겨 두되 게임과 Editor Preview에서는 표시하지 않는다.
	if (GetMGTurretOperatorAnchor())
	{
		GetMGTurretOperatorAnchor()->SetVisibility(false);
		GetMGTurretOperatorAnchor()->SetHiddenInGame(true);
	}
	if (GetSlotFacingPreview())
	{
		GetSlotFacingPreview()->SetVisibility(false);
		GetSlotFacingPreview()->SetHiddenInGame(true);
	}
}

void ADroneAutomaticTurret::BeginPlay()
{
	Super::BeginPlay();
	TargetScanTimeRemaining = 0.0f;
	if (bAutomaticTurretEnabled)
	{
		RefreshAutomaticTarget();
	}
}

void ADroneAutomaticTurret::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAutomaticTurretEnabled)
	{
		ChangeAutomaticTarget(nullptr);
		return;
	}

	TargetScanTimeRemaining -= FMath::Max(0.0f, DeltaSeconds);
	if (TargetScanTimeRemaining <= 0.0f)
	{
		RefreshAutomaticTarget();
		TargetScanTimeRemaining = FMath::Max(0.02f, TargetScanIntervalSeconds);
	}

	AActor* Target = AutomaticTarget.Get();
	if (!Target)
	{
		return;
	}

	if (!IsMGTurretInUse())
	{
		if (!BeginMGTurretUse(this, Target))
		{
			ChangeAutomaticTarget(nullptr);
		}
		return;
	}

	if (!UpdateMGTurretUse(this, Target))
	{
		ChangeAutomaticTarget(nullptr);
	}
}

void ADroneAutomaticTurret::SetAutomaticTurretEnabled(const bool bEnabled)
{
	if (bAutomaticTurretEnabled == bEnabled)
	{
		return;
	}

	bAutomaticTurretEnabled = bEnabled;
	if (!bEnabled)
	{
		ChangeAutomaticTarget(nullptr);
	}
	else
	{
		TargetScanTimeRemaining = 0.0f;
		RefreshAutomaticTarget();
	}
}

void ADroneAutomaticTurret::ConfigureAutomaticTargetingGreybox(
	const float InDetectionRange,
	const float InLoseTargetRange,
	const float InScanIntervalSeconds,
	const bool bInRequireTargetLineOfSight)
{
	DetectionRange = FMath::Max(1.0f, InDetectionRange);
	LoseTargetRange = FMath::Max(DetectionRange, InLoseTargetRange);
	TargetScanIntervalSeconds = FMath::Max(0.02f, InScanIntervalSeconds);
	bRequireTargetLineOfSight = bInRequireTargetLineOfSight;
	TargetScanTimeRemaining = 0.0f;
}

AActor* ADroneAutomaticTurret::RefreshAutomaticTarget()
{
	if (!bAutomaticTurretEnabled || !GetWorld())
	{
		ChangeAutomaticTarget(nullptr);
		return nullptr;
	}

	if (const ADronePrototypePawn* CurrentDrone = Cast<ADronePrototypePawn>(AutomaticTarget.Get()))
	{
		if (IsEligibleDroneTarget(CurrentDrone, LoseTargetRange)
			&& HasTargetLineOfSight(CurrentDrone))
		{
			return AutomaticTarget.Get();
		}
	}

	ADronePrototypePawn* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(DetectionRange);
	for (TActorIterator<ADronePrototypePawn> It(GetWorld()); It; ++It)
	{
		ADronePrototypePawn* Candidate = *It;
		if (!IsEligibleDroneTarget(Candidate, DetectionRange) || !HasTargetLineOfSight(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (!BestTarget || DistanceSquared < BestDistanceSquared)
		{
			BestTarget = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}

	ChangeAutomaticTarget(BestTarget);
	return BestTarget;
}

bool ADroneAutomaticTurret::IsEligibleDroneTarget(
	const ADronePrototypePawn* Candidate,
	const float Range) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	const UDroneHealthComponent* Health = Candidate->FindComponentByClass<UDroneHealthComponent>();
	if (Health && Health->IsDead())
	{
		return false;
	}

	const FVector ScanOrigin = GetMGTurretAimPivot()
		? GetMGTurretAimPivot()->GetComponentLocation()
		: GetActorLocation();
	return FVector::DistSquared(ScanOrigin, Candidate->GetActorLocation()) <= FMath::Square(FMath::Max(1.0f, Range));
}

bool ADroneAutomaticTurret::HasTargetLineOfSight(const AActor* Candidate) const
{
	if (!bRequireTargetLineOfSight || !Candidate || !GetWorld())
	{
		return Candidate != nullptr;
	}

	const FVector TraceStart = GetMGTurretAimPivot()
		? GetMGTurretAimPivot()->GetComponentLocation()
		: GetActorLocation();
	const FVector TraceEnd = Candidate->GetActorLocation();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneAutomaticTurretSight), true, this);
	QueryParams.AddIgnoredActor(this);
	if (const AActor* ParentActor = GetAttachParentActor())
	{
		QueryParams.AddIgnoredActor(ParentActor);
	}
	if (const AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams);
	const bool bVisible = !bBlocked || Hit.GetActor() == Candidate;
	if (bDrawDetectionDebugLine)
	{
		DrawDebugLine(
			GetWorld(),
			TraceStart,
			bBlocked ? Hit.ImpactPoint : TraceEnd,
			bVisible ? FColor::Cyan : FColor::Orange,
			false,
			FMath::Max(0.05f, TargetScanIntervalSeconds),
			0,
			1.5f);
	}
	return bVisible;
}

void ADroneAutomaticTurret::ChangeAutomaticTarget(AActor* NewTarget)
{
	AActor* PreviousTarget = AutomaticTarget.Get();
	if (PreviousTarget == NewTarget)
	{
		return;
	}

	if (IsMGTurretInUse())
	{
		EndMGTurretUse(this);
	}
	AutomaticTarget = NewTarget;
	if (NewTarget)
	{
		BeginMGTurretUse(this, NewTarget);
	}
	OnAutomaticTargetChanged.Broadcast(PreviousTarget, NewTarget);
}

void ADroneAutomaticTurret::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ChangeAutomaticTarget(nullptr);
	Super::EndPlay(EndPlayReason);
}

ADroneEmplacedAutomaticTurret::ADroneEmplacedAutomaticTurret()
{
	MountType = EDroneAutomaticTurretMountType::Emplaced;
	DetectionRange = 4500.0f;
	LoseTargetRange = 5200.0f;
	ConfigureMGTurretGreybox(5200.0f, 0.22f);
	ConfigureMGTurretDamageGreybox(10.0f);
	ConfigureMGTurretAccuracyGreybox(2.5f);
	MGTurretMuzzleOffset = FVector(115.0f, 0.0f, 0.0f);

	if (MGTurretBaseMesh)
	{
		MGTurretBaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
		MGTurretBaseMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.7f));
	}
	if (MGTurretYawPivot)
	{
		MGTurretYawPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	}
	if (MGTurretMuzzle)
	{
		MGTurretMuzzle->SetRelativeLocation(FVector(115.0f, 0.0f, 0.0f));
	}
}

ADroneVehicleAutomaticTurret::ADroneVehicleAutomaticTurret()
{
	MountType = EDroneAutomaticTurretMountType::VehicleMounted;
	DetectionRange = 5500.0f;
	LoseTargetRange = 6200.0f;
	ConfigureMGTurretGreybox(6500.0f, 0.12f);
	ConfigureMGTurretDamageGreybox(7.0f);
	ConfigureMGTurretAccuracyGreybox(3.0f);
	MGTurretMuzzleOffset = FVector(105.0f, 0.0f, 0.0f);

	if (MGTurretBaseMesh)
	{
		MGTurretBaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
		MGTurretBaseMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.16f));
	}
	if (MGTurretYawPivot)
	{
		MGTurretYawPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
	}
	if (MGTurretMuzzle)
	{
		MGTurretMuzzle->SetRelativeLocation(FVector(105.0f, 0.0f, 0.0f));
	}
}
