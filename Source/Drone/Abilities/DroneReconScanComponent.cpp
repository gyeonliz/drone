#include "Abilities/DroneReconScanComponent.h"

#include "Abilities/DroneReconScanTargetComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UDroneReconScanComponent::UDroneReconScanComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDroneReconScanComponent::ConfigureFeatureEnabled(const bool bEnabled)
{
	if (!bEnabled)
	{
		CancelScan();
	}
	bFeatureEnabled = bEnabled;
}

bool UDroneReconScanComponent::StartScan(AActor* TargetActor)
{
	if (!bFeatureEnabled || ActiveTarget.IsValid() || !IsTargetValidForScan(TargetActor))
	{
		return false;
	}

	ActiveTarget = TargetActor;
	ScanElapsedSeconds = 0.0f;
	OnScanProgress.Broadcast(TargetActor, 0.0f);
	return true;
}

bool UDroneReconScanComponent::StartBestAvailableScan()
{
	return StartScan(FindBestAvailableScanTarget());
}

AActor* UDroneReconScanComponent::FindBestAvailableScanTarget() const
{
	const UWorld* World = GetWorld();
	const AActor* OwnerActor = GetOwner();
	if (!bFeatureEnabled || !World || !OwnerActor || ActiveTarget.IsValid())
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate
			|| !Candidate->FindComponentByClass<UDroneReconScanTargetComponent>()
			|| !IsTargetValidForScan(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			OwnerActor->GetActorLocation(),
			Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}
	return BestTarget;
}

void UDroneReconScanComponent::CancelScan()
{
	AActor* CanceledTarget = ActiveTarget.Get();
	ActiveTarget.Reset();
	ScanElapsedSeconds = 0.0f;
	if (CanceledTarget)
	{
		OnScanCanceled.Broadcast(CanceledTarget);
	}
}

float UDroneReconScanComponent::GetScanProgressNormalized() const
{
	return ActiveTarget.IsValid()
		? FMath::Clamp(ScanElapsedSeconds / FMath::Max(0.05f, ScanDurationSeconds), 0.0f, 1.0f)
		: 0.0f;
}

void UDroneReconScanComponent::ConfigureScanGreybox(
	const float NewRange,
	const float NewDuration,
	const float NewHalfAngleDegrees,
	const bool bNewRequireLineOfSight)
{
	ScanRangeCentimeters = FMath::Max(1.0f, NewRange);
	ScanDurationSeconds = FMath::Max(0.05f, NewDuration);
	ScanHalfAngleDegrees = FMath::Clamp(NewHalfAngleDegrees, 1.0f, 180.0f);
	bRequireLineOfSight = bNewRequireLineOfSight;
}

void UDroneReconScanComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* TargetActor = ActiveTarget.Get();
	if (!TargetActor)
	{
		return;
	}
	if (!bFeatureEnabled || !IsTargetValidForScan(TargetActor))
	{
		CancelScan();
		return;
	}

	ScanElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	OnScanProgress.Broadcast(TargetActor, GetScanProgressNormalized());
	if (ScanElapsedSeconds >= ScanDurationSeconds)
	{
		CompleteScan();
	}
}

bool UDroneReconScanComponent::IsTargetValidForScan(AActor* TargetActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(TargetActor) || TargetActor == OwnerActor)
	{
		return false;
	}

	const UDroneReconScanTargetComponent* TargetComponent =
		TargetActor->FindComponentByClass<UDroneReconScanTargetComponent>();
	if (!TargetComponent || TargetComponent->IsScanCompleted())
	{
		return false;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - OwnerActor->GetActorLocation();
	if (ToTarget.SizeSquared() > FMath::Square(ScanRangeCentimeters))
	{
		return false;
	}
	const FVector Direction = ToTarget.GetSafeNormal();
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(ScanHalfAngleDegrees));
	if (!Direction.IsNearlyZero() && FVector::DotProduct(OwnerActor->GetActorForwardVector(), Direction) < MinimumDot)
	{
		return false;
	}

	if (!bRequireLineOfSight)
	{
		return true;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DroneReconScan), false, OwnerActor);
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(
		Hit,
		OwnerActor->GetActorLocation(),
		TargetActor->GetActorLocation(),
		ECC_Visibility,
		Params);
	return !bBlocked || Hit.GetActor() == TargetActor;
}

void UDroneReconScanComponent::CompleteScan()
{
	AActor* CompletedTarget = ActiveTarget.Get();
	ActiveTarget.Reset();
	ScanElapsedSeconds = 0.0f;
	if (!CompletedTarget)
	{
		return;
	}

	UDroneReconScanTargetComponent* TargetComponent =
		CompletedTarget->FindComponentByClass<UDroneReconScanTargetComponent>();
	if (TargetComponent && TargetComponent->MarkScanned(GetOwner()))
	{
		++CompletedScanCount;
		OnScanCompleted.Broadcast(CompletedTarget);
	}
}
