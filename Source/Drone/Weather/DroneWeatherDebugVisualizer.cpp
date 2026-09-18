#include "Weather/DroneWeatherDebugVisualizer.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Math/RotationMatrix.h"
#include "Prototype/DronePrototypePawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Weather/DroneWeatherWorldSubsystem.h"
#include "Weather/DroneWeatherProfile.h"

namespace DroneWeatherDebug
{
constexpr uint64 SummaryMessageKey = 0x4452575401ULL;
constexpr uint64 HelpMessageKey = 0x4452575402ULL;

bool IsWeatherTestMap(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	const auto MatchesWeatherTestName = [](const FString& Name)
	{
		return Name == TEXT("Lvl_DroneWeatherSystemsTest")
			|| Name.EndsWith(TEXT("_Lvl_DroneWeatherSystemsTest"));
	};
	// A loaded map uses its package name; transient automation worlds live in the Transient package,
	// so also check the explicitly assigned world object name for the test-only entry path.
	return MatchesWeatherTestName(World->GetMapName())
		|| MatchesWeatherTestName(World->GetName());
}
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
		bRainDebugPreviewMap = DroneWeatherDebug::IsWeatherTestMap(World);
		RainPreviewRandomStream.Initialize(static_cast<int32>(GetUniqueID()));
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
	UpdateRainDebugPreview(DeltaSeconds);
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

int32 ADroneWeatherDebugVisualizer::CalculateRainPreviewStreakCount(
	const float RainIntensity01,
	const float RainSpawnScale01,
	const int32 MaxStreakCount)
{
	const int32 SafeMax = FMath::Clamp(MaxStreakCount, 0, 80);
	const float EffectiveRain = FMath::Clamp(RainIntensity01, 0.0f, 1.0f)
		* FMath::Clamp(RainSpawnScale01, 0.0f, 1.0f);
	return FMath::Clamp(FMath::RoundToInt(EffectiveRain * SafeMax), 0, SafeMax);
}

void ADroneWeatherDebugVisualizer::UpdateRainDebugPreview(const float DeltaSeconds)
{
	if (!bRainDebugPreviewMap)
	{
		return;
	}

	CurrentRainPreviewStreakCount = CalculateRainPreviewStreakCount(
		CachedSnapshot.RainIntensity01,
		CachedSnapshot.RainSpawnScale01,
		RainPreviewMaxStreakCount);
	if (CurrentRainPreviewStreakCount <= 0)
	{
		// No new draw calls while rain is off; already drawn lines expire within one preview interval.
		RainPreviewElapsedSeconds = 0.0f;
		return;
	}

	RainPreviewElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
	constexpr float PreviewUpdateIntervalSeconds = 0.2f;
	if (RainPreviewElapsedSeconds < PreviewUpdateIntervalSeconds)
	{
		return;
	}
	RainPreviewElapsedSeconds = 0.0f;

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FRotationMatrix ViewMatrix(ViewRotation);
	const FVector Forward = ViewMatrix.GetUnitAxis(EAxis::X);
	const FVector Right = ViewMatrix.GetUnitAxis(EAxis::Y);
	const FVector Up = ViewMatrix.GetUnitAxis(EAxis::Z);
	const FVector WindDrift = CachedSnapshot.WindVelocityCentimetersPerSecond.GetClampedToMaxSize(65.0f);
	constexpr float ViewDepthCentimeters = 1100.0f;
	constexpr float HalfWidthCentimeters = 850.0f;
	constexpr float HalfHeightCentimeters = 500.0f;
	constexpr float DropLengthCentimeters = 135.0f;
	constexpr float PreviewLineLifetimeSeconds = PreviewUpdateIntervalSeconds * 1.5f;
	for (int32 Index = 0; Index < CurrentRainPreviewStreakCount; ++Index)
	{
		const FVector Start = ViewLocation
			+ Forward * ViewDepthCentimeters
			+ Right * RainPreviewRandomStream.FRandRange(-HalfWidthCentimeters, HalfWidthCentimeters)
			+ Up * RainPreviewRandomStream.FRandRange(-HalfHeightCentimeters, HalfHeightCentimeters);
		const FVector End = Start + FVector(0.0f, 0.0f, -DropLengthCentimeters) + WindDrift;
		DrawDebugLine(
			World,
			Start,
			End,
			FColor(185, 220, 255),
			false,
			PreviewLineLifetimeSeconds,
			SDPG_World,
			1.0f);
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

bool ADroneWeatherDebugVisualizer::ApplyTestWeatherPreset(const int32 PresetIndex)
{
	// Restrict the development entry point to the existing dedicated map (PIE prefixes included).
	UWorld* World = GetWorld();
	if (!DroneWeatherDebug::IsWeatherTestMap(World))
	{
		return false;
	}
	static const TCHAR* ProfilePaths[] =
	{
		TEXT("/Game/Drone/Data/Weather/DA_Weather_Clear.DA_Weather_Clear"),
		TEXT("/Game/Drone/Data/Weather/DA_Weather_LightWind.DA_Weather_LightWind"),
		TEXT("/Game/Drone/Data/Weather/DA_Weather_RainStorm_Greybox.DA_Weather_RainStorm_Greybox")
	};
	if (PresetIndex < 0 || PresetIndex >= UE_ARRAY_COUNT(ProfilePaths))
	{
		return false;
	}
	UDroneWeatherWorldSubsystem* Subsystem = World->GetSubsystem<UDroneWeatherWorldSubsystem>();
	UDroneWeatherProfile* Profile = LoadObject<UDroneWeatherProfile>(nullptr, ProfilePaths[PresetIndex]);
	return Subsystem && Profile && Subsystem->ApplyWeatherProfile(Profile, true);
}

void ADroneWeatherDebugVisualizer::HandleControlModeHotkeys()
{
	if (!bEnableControlModeHotkeys && !bEnableWeatherPresetHotkeys)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	ADronePrototypePawn* Drone = PlayerController ? Cast<ADronePrototypePawn>(PlayerController->GetPawn()) : nullptr;
	if (!PlayerController)
	{
		return;
	}
	if (bEnableWeatherPresetHotkeys)
	{
		if (PlayerController->WasInputKeyJustPressed(EKeys::Seven)) ApplyTestWeatherPreset(0);
		else if (PlayerController->WasInputKeyJustPressed(EKeys::Eight)) ApplyTestWeatherPreset(1);
		else if (PlayerController->WasInputKeyJustPressed(EKeys::Nine)) ApplyTestWeatherPreset(2);
	}
	if (!bEnableControlModeHotkeys || !Drone)
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
		Drone->SetControlMode(EDroneControlMode::AcroRateMode1Greybox);
	}
	else if (PlayerController->WasInputKeyJustPressed(EKeys::Four) || PlayerController->WasInputKeyJustPressed(EKeys::NumPadFour))
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
				case EDroneControlMode::AcroRateMode1Greybox:
					ControlMode = TEXT("RATE/ACRO MODE 1 (0% correction)");
					break;
				case EDroneControlMode::AcroRateRealisticGreybox:
					ControlMode = TEXT("RATE/ACRO MODE 2 (0% correction)");
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
			TEXT("WEATHER TEST | %s | Wind %.1f m/s @ %.0f deg | Rain %.2f Spawn %.2f Wet %.2f | Debug streaks %d | Mode %s"),
			*CachedSnapshot.WeatherId.ToString(),
			SpeedMetersPerSecond,
			DirectionDegrees,
			CachedSnapshot.RainIntensity01,
			CachedSnapshot.RainSpawnScale01,
			CachedSnapshot.SurfaceWetness01,
			CurrentRainPreviewStreakCount,
			*ControlMode));
	GEngine->AddOnScreenDebugMessage(
		DroneWeatherDebug::HelpMessageKey,
		0.15f,
		FColor::Yellow,
		TEXT("1 Easy / 2 Manual / 3 Acro Mode 1 / 4 Acro Mode 2 | Weather: 7 Clear / 8 LightWind / 9 RainStorm + debug rain lines (not Niagara)."));
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
