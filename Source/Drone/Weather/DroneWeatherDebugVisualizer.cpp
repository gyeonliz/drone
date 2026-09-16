#include "Weather/DroneWeatherDebugVisualizer.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Weather/DroneWeatherWorldSubsystem.h"

namespace DroneWeatherDebug
{
constexpr uint64 SummaryMessageKey = 0x4452575401ULL;
constexpr uint64 HelpMessageKey = 0x4452575402ULL;
}

ADroneWeatherDebugVisualizer::ADroneWeatherDebugVisualizer()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FlowBeads = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FlowBeads"));
	FlowBeads->SetupAttachment(SceneRoot);
	FlowBeads->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlowBeads->SetGenerateOverlapEvents(false);
	FlowBeads->SetCanEverAffectNavigation(false);
	FlowBeads->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		FlowBeads->SetStaticMesh(SphereMesh.Object);
	}
}

void ADroneWeatherDebugVisualizer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildFlowBeads();
}

void ADroneWeatherDebugVisualizer::BeginPlay()
{
	Super::BeginPlay();
	RebuildFlowBeads();

	if (UWorld* World = GetWorld())
	{
		if (UDroneWeatherWorldSubsystem* Subsystem = World->GetSubsystem<UDroneWeatherWorldSubsystem>())
		{
			WeatherSubsystem = Subsystem;
			CachedSnapshot = Subsystem->GetSnapshot();
			Subsystem->OnWeatherSnapshotChanged.AddUniqueDynamic(
				this,
				&ADroneWeatherDebugVisualizer::HandleWeatherSnapshotChanged);
		}
	}
}

void ADroneWeatherDebugVisualizer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDroneWeatherWorldSubsystem* Subsystem = WeatherSubsystem.Get())
	{
		Subsystem->OnWeatherSnapshotChanged.RemoveDynamic(
			this,
			&ADroneWeatherDebugVisualizer::HandleWeatherSnapshotChanged);
	}
	WeatherSubsystem.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADroneWeatherDebugVisualizer::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	HandleControlModeHotkeys();
	UpdateFlowBeads(DeltaSeconds);
	UpdateOnScreenReadout();
}

void ADroneWeatherDebugVisualizer::HandleWeatherSnapshotChanged(const FDroneWeatherSnapshot NewSnapshot)
{
	CachedSnapshot = NewSnapshot;
}

void ADroneWeatherDebugVisualizer::RebuildFlowBeads()
{
	if (!FlowBeads)
	{
		return;
	}

	const int32 SafeCount = FMath::Clamp(FlowBeadCount, 4, 128);
	const FVector SafeExtent(
		FMath::Max(1.0f, FMath::Abs(FlowAreaExtent.X)),
		FMath::Max(1.0f, FMath::Abs(FlowAreaExtent.Y)),
		FMath::Max(1.0f, FMath::Abs(FlowAreaExtent.Z)));
	const float SafeScale = FMath::Clamp(FlowBeadScale, 0.01f, 1.0f);

	FlowBeads->ClearInstances();
	BaseBeadLocations.Reset(SafeCount);
	FlowTravelOffset = FVector::ZeroVector;
	DisplayedWindVelocity = FVector::ZeroVector;
	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		// 서로 다른 세 개의 비정수 배수로 반복 격자처럼 보이지 않는 결정적 분포를 만든다.
		const float X01 = FMath::Frac((Index + 0.5f) * 0.61803398875f);
		const float Y01 = FMath::Frac((Index + 0.5f) * 0.41421356237f);
		const float Z01 = FMath::Frac((Index + 0.5f) * 0.73205080756f);
		const FVector Location(
			FMath::Lerp(-SafeExtent.X, SafeExtent.X, X01),
			FMath::Lerp(-SafeExtent.Y, SafeExtent.Y, Y01),
			FMath::Lerp(-SafeExtent.Z, SafeExtent.Z, Z01));
		BaseBeadLocations.Add(Location);
		FlowBeads->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(SafeScale)));
	}
}

void ADroneWeatherDebugVisualizer::UpdateFlowBeads(const float DeltaSeconds)
{
	if (!FlowBeads || BaseBeadLocations.IsEmpty())
	{
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	const float ResponseAlpha = 1.0f - FMath::Exp(
		-SafeDeltaSeconds / FMath::Max(0.01f, FlowVelocityResponseSeconds));
	DisplayedWindVelocity = FMath::Lerp(
		DisplayedWindVelocity,
		CachedSnapshot.WindVelocityCentimetersPerSecond,
		ResponseAlpha);
	const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(DisplayedWindVelocity);
	FlowTravelOffset = IntegrateFlowTravelOffset(
		FlowTravelOffset,
		LocalVelocity,
		SafeDeltaSeconds,
		FlowPlaybackScale);
	const FVector SafeExtent(
		FMath::Max(1.0f, FMath::Abs(FlowAreaExtent.X)),
		FMath::Max(1.0f, FMath::Abs(FlowAreaExtent.Y)),
		FMath::Max(1.0f, FMath::Abs(FlowAreaExtent.Z)));
	const float SafeScale = FMath::Clamp(FlowBeadScale, 0.01f, 1.0f);
	const float SpeedAlpha = FMath::Clamp(
		LocalVelocity.Size() / FMath::Max(100.0f, FlowReferenceWindSpeedCentimetersPerSecond),
		0.0f,
		1.0f);
	const float LengthScale = FMath::Lerp(
		FMath::Max(0.1f, FlowBeadCalmLengthScale),
		FMath::Max(FlowBeadCalmLengthScale, FlowBeadMaximumLengthScale),
		SpeedAlpha);
	const float CrossSectionScale = FMath::Clamp(FlowBeadCrossSectionScale, 0.1f, 1.0f);
	const FVector InstanceScale(
		SafeScale * LengthScale,
		SafeScale * CrossSectionScale,
		SafeScale * CrossSectionScale);
	const FRotator FlowRotation = LocalVelocity.IsNearlyZero(0.1f)
		? FRotator::ZeroRotator
		: LocalVelocity.Rotation();

	for (int32 Index = 0; Index < BaseBeadLocations.Num(); ++Index)
	{
		FVector Location = BaseBeadLocations[Index] + FlowTravelOffset;
		Location.X = WrapCoordinate(Location.X, SafeExtent.X);
		Location.Y = WrapCoordinate(Location.Y, SafeExtent.Y);
		Location.Z = WrapCoordinate(Location.Z, SafeExtent.Z);
		FlowBeads->UpdateInstanceTransform(
			Index,
			FTransform(FlowRotation, Location, InstanceScale),
			false,
			Index == BaseBeadLocations.Num() - 1,
			true);
	}
}

FVector ADroneWeatherDebugVisualizer::IntegrateFlowTravelOffset(
	const FVector& CurrentOffset,
	const FVector& LocalWindVelocityCentimetersPerSecond,
	const float DeltaSeconds,
	const float PlaybackScale)
{
	return CurrentOffset
		+ LocalWindVelocityCentimetersPerSecond
			* FMath::Max(0.0f, DeltaSeconds)
			* FMath::Max(0.05f, PlaybackScale);
}

void ADroneWeatherDebugVisualizer::HandleControlModeHotkeys() const
{
	if (!bEnableControlModeHotkeys)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	ADronePrototypePawn* Drone = PlayerController ? Cast<ADronePrototypePawn>(PlayerController->GetPawn()) : nullptr;
	if (!PlayerController || !Drone)
	{
		return;
	}

	if (PlayerController->WasInputKeyJustPressed(EKeys::One) || PlayerController->WasInputKeyJustPressed(EKeys::NumPadOne))
	{
		Drone->SetControlMode(EDroneControlMode::AssistedEasy);
	}
	else if (PlayerController->WasInputKeyJustPressed(EKeys::Two) || PlayerController->WasInputKeyJustPressed(EKeys::NumPadTwo))
	{
		Drone->SetControlMode(EDroneControlMode::ManualRealisticGreybox);
	}
	else if (PlayerController->WasInputKeyJustPressed(EKeys::Three) || PlayerController->WasInputKeyJustPressed(EKeys::NumPadThree))
	{
		Drone->SetControlMode(EDroneControlMode::AcroRateRealisticGreybox);
	}
}

void ADroneWeatherDebugVisualizer::UpdateOnScreenReadout() const
{
	if (!bShowOnScreenReadout || !GEngine)
	{
		return;
	}

	const FVector WindVelocity = CachedSnapshot.WindVelocityCentimetersPerSecond;
	const float SpeedMetersPerSecond = WindVelocity.Size() / 100.0f;
	const float DirectionDegrees = WindVelocity.IsNearlyZero()
		? 0.0f
		: FMath::RadiansToDegrees(FMath::Atan2(WindVelocity.Y, WindVelocity.X));
	FString ControlMode(TEXT("NO DRONE"));
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			if (const ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(PlayerController->GetPawn()))
			{
				switch (Drone->GetControlMode())
				{
				case EDroneControlMode::ManualRealisticGreybox:
					ControlMode = TEXT("MANUAL (25% correction)");
					break;
				case EDroneControlMode::AcroRateRealisticGreybox:
					ControlMode = TEXT("RATE/ACRO (0% correction)");
					break;
				case EDroneControlMode::AssistedEasy:
				default:
					ControlMode = TEXT("EASY (65% correction)");
					break;
				}
			}
		}
	}

	GEngine->AddOnScreenDebugMessage(
		DroneWeatherDebug::SummaryMessageKey,
		0.15f,
		FColor::Cyan,
		FString::Printf(
			TEXT("WEATHER TEST | %s | Wind %.1f m/s @ %.0f deg | Mode %s"),
			*CachedSnapshot.WeatherId.ToString(),
			SpeedMetersPerSecond,
			DirectionDegrees,
			*ControlMode));
	GEngine->AddOnScreenDebugMessage(
		DroneWeatherDebug::HelpMessageKey,
		0.15f,
		FColor::Yellow,
		TEXT("Release movement keys to observe drift. Press 1 Easy / 2 Manual / 3 Rate-Acro and compare displacement."));
}

float ADroneWeatherDebugVisualizer::WrapCoordinate(const float Value, const float Extent)
{
	const float SafeExtent = FMath::Max(1.0f, Extent);
	const float Width = SafeExtent * 2.0f;
	float Wrapped = FMath::Fmod(Value + SafeExtent, Width);
	if (Wrapped < 0.0f)
	{
		Wrapped += Width;
	}
	return Wrapped - SafeExtent;
}
