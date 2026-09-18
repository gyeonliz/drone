#include "Weather/DroneWeatherResponseComponent.h"

#include "Engine/World.h"
#include "Prototype/DronePrototypePawn.h"
#include "Weather/DroneWeatherWorldSubsystem.h"

UDroneWeatherResponseComponent::UDroneWeatherResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDroneWeatherResponseComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerDrone = Cast<ADronePrototypePawn>(GetOwner());
	if (UWorld* World = GetWorld())
	{
		if (UDroneWeatherWorldSubsystem* Subsystem = World->GetSubsystem<UDroneWeatherWorldSubsystem>())
		{
			WeatherSubsystem = Subsystem;
			CachedWeatherSnapshot = Subsystem->GetSnapshot();
			Subsystem->OnWeatherSnapshotChanged.AddUniqueDynamic(
				this,
				&UDroneWeatherResponseComponent::HandleWeatherSnapshotChanged);
		}
	}
	SetComponentTickEnabled(!CachedWeatherSnapshot.WindVelocityCentimetersPerSecond.IsNearlyZero());
}

void UDroneWeatherResponseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDroneWeatherWorldSubsystem* Subsystem = WeatherSubsystem.Get())
	{
		Subsystem->OnWeatherSnapshotChanged.RemoveDynamic(
			this,
			&UDroneWeatherResponseComponent::HandleWeatherSnapshotChanged);
	}
	WeatherSubsystem.Reset();
	OwnerDrone.Reset();
	Super::EndPlay(EndPlayReason);
}

void UDroneWeatherResponseComponent::HandleWeatherSnapshotChanged(const FDroneWeatherSnapshot NewSnapshot)
{
	CachedWeatherSnapshot = NewSnapshot;
	if (!NewSnapshot.WindVelocityCentimetersPerSecond.IsNearlyZero() || !CurrentWindDriftVelocity.IsNearlyZero())
	{
		SetComponentTickEnabled(true);
	}
}

FVector UDroneWeatherResponseComponent::CalculateTargetWindDriftVelocity(
	const FDroneWeatherSnapshot& WeatherSnapshot,
	const EDroneControlMode ControlMode) const
{
	float Compensation = AssistedCompensation01;
	switch (ControlMode)
	{
	case EDroneControlMode::ManualRealisticGreybox:
		Compensation = LimitedAttitudeCompensation01;
		break;
	case EDroneControlMode::AcroRateMode1Greybox:
	case EDroneControlMode::AcroRateRealisticGreybox:
		Compensation = AcroCompensation01;
		break;
	case EDroneControlMode::AssistedEasy:
	default:
		break;
	}

	const FVector Target = WeatherSnapshot.WindVelocityCentimetersPerSecond
		* FMath::Clamp(WeatherSnapshot.DroneWindResponseMultiplier, 0.0f, 2.0f)
		* FMath::Clamp(DroneWindResponseMultiplier, 0.0f, 2.0f)
		* (1.0f - FMath::Clamp(Compensation, 0.0f, 1.0f));
	return Target.GetClampedToMaxSize(FMath::Max(0.0f, MaximumDriftSpeedCentimetersPerSecond));
}

void UDroneWeatherResponseComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ADronePrototypePawn* Drone = OwnerDrone.Get();
	if (!Drone || DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector TargetDrift = CalculateTargetWindDriftVelocity(
		CachedWeatherSnapshot,
		Drone->GetControlMode());
	const float InterpolationSpeed = ResponseTimeSeconds <= UE_SMALL_NUMBER
		? 1000.0f
		: 1.0f / ResponseTimeSeconds;
	CurrentWindDriftVelocity = FMath::VInterpTo(
		CurrentWindDriftVelocity,
		TargetDrift,
		DeltaTime,
		InterpolationSpeed);

	FHitResult Hit;
	Drone->AddActorWorldOffset(CurrentWindDriftVelocity * DeltaTime, true, &Hit);
	if (Hit.bBlockingHit)
	{
		CurrentWindDriftVelocity = FVector::VectorPlaneProject(CurrentWindDriftVelocity, Hit.Normal);
	}
	if (TargetDrift.IsNearlyZero(0.1f) && CurrentWindDriftVelocity.IsNearlyZero(0.1f))
	{
		CurrentWindDriftVelocity = FVector::ZeroVector;
		SetComponentTickEnabled(false);
	}
}
