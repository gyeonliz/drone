#include "Weather/DroneWeatherController.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Weather/DroneWeatherProfile.h"
#include "Weather/DroneWeatherWorldSubsystem.h"

ADroneWeatherController::ADroneWeatherController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	EditorPlacementCone = CreateEditorOnlyDefaultSubobject<UStaticMeshComponent>(TEXT("EditorPlacementCone"));
	if (EditorPlacementCone)
	{
		EditorPlacementCone->SetupAttachment(SceneRoot);
		EditorPlacementCone->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
		EditorPlacementCone->SetRelativeScale3D(FVector(0.75f));
		EditorPlacementCone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EditorPlacementCone->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		EditorPlacementCone->SetGenerateOverlapEvents(false);
		EditorPlacementCone->SetCanEverAffectNavigation(false);
		EditorPlacementCone->SetHiddenInGame(true);
		EditorPlacementCone->SetCastShadow(false);
		EditorPlacementCone->SetIsVisualizationComponent(true);

		static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(
			TEXT("/Engine/BasicShapes/Cone.Cone"));
		if (ConeMesh.Succeeded())
		{
			EditorPlacementCone->SetStaticMesh(ConeMesh.Object);
		}
	}
#endif
}

void ADroneWeatherController::BeginPlay()
{
	Super::BeginPlay();
	ApplyConfiguredWeather();
	if (bEnableRandomWind)
	{
		InitializeRandomWind();
		SetActorTickEnabled(true);
	}
}

void ADroneWeatherController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UDroneWeatherWorldSubsystem* Subsystem = World->GetSubsystem<UDroneWeatherWorldSubsystem>())
		{
			Subsystem->ClearRuntimeWindOverride();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ADroneWeatherController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bEnableRandomWind)
	{
		SetActorTickEnabled(false);
		return;
	}
	DirectionChangeRemainingSeconds -= FMath::Max(0.0f, DeltaSeconds);
	SpeedChangeRemainingSeconds -= FMath::Max(0.0f, DeltaSeconds);
	if (DirectionChangeRemainingSeconds <= 0.0f)
	{
		ChooseNewRandomWindDirection();
	}
	if (SpeedChangeRemainingSeconds <= 0.0f)
	{
		ChooseNewRandomWindSpeed();
	}
	PublishRandomWind(DeltaSeconds);
}

float ADroneWeatherController::RandomInterval(
	const float MinimumSeconds,
	const float MaximumSeconds)
{
	const float Minimum = FMath::Max(0.1f, FMath::Min(MinimumSeconds, MaximumSeconds));
	const float Maximum = FMath::Max(Minimum, FMath::Max(MinimumSeconds, MaximumSeconds));
	return RandomWindStream.FRandRange(Minimum, Maximum);
}

void ADroneWeatherController::InitializeRandomWind()
{
	RandomWindStream.Initialize(RandomWindSeed);
	CurrentWindDirectionYawDegrees = 0.0f;
	CurrentWindSpeedMetersPerSecond = FMath::Max(0.0f, MinimumWindSpeedMetersPerSecond);
	ChooseNewRandomWindDirection();
	ChooseNewRandomWindSpeed();
	CurrentWindDirectionYawDegrees = TargetWindDirectionYawDegrees;
	CurrentWindSpeedMetersPerSecond = bTargetDirectionIsCalm ? 0.0f : TargetWindSpeedMetersPerSecond;
	bCurrentDirectionIsCalm = bTargetDirectionIsCalm;
	PublishRandomWind(0.0f);
}

void ADroneWeatherController::ChooseNewRandomWindDirection()
{
	const int32 ChoiceCount = bIncludeCalm ? 9 : 8;
	const int32 Choice = RandomWindStream.RandRange(0, ChoiceCount - 1);
	bTargetDirectionIsCalm = bIncludeCalm && Choice == 8;
	if (!bTargetDirectionIsCalm)
	{
		// Unreal World +X=E, +Y=N 기준: E, NE, N, NW, W, SW, S, SE.
		TargetWindDirectionYawDegrees = static_cast<float>(Choice) * 45.0f;
	}
	DirectionChangeRemainingSeconds = RandomInterval(
		MinimumDirectionChangeIntervalSeconds,
		MaximumDirectionChangeIntervalSeconds);
}

void ADroneWeatherController::ChooseNewRandomWindSpeed()
{
	const float Minimum = FMath::Max(0.0f, FMath::Min(
		MinimumWindSpeedMetersPerSecond,
		MaximumWindSpeedMetersPerSecond));
	const float Maximum = FMath::Max(Minimum, FMath::Max(
		MinimumWindSpeedMetersPerSecond,
		MaximumWindSpeedMetersPerSecond));
	TargetWindSpeedMetersPerSecond = RandomWindStream.FRandRange(Minimum, Maximum);
	SpeedChangeRemainingSeconds = RandomInterval(
		MinimumSpeedChangeIntervalSeconds,
		MaximumSpeedChangeIntervalSeconds);
}

void ADroneWeatherController::PublishRandomWind(const float DeltaSeconds)
{
	UWorld* World = GetWorld();
	UDroneWeatherWorldSubsystem* Subsystem = World
		? World->GetSubsystem<UDroneWeatherWorldSubsystem>()
		: nullptr;
	if (!Subsystem)
	{
		return;
	}
	const float DirectionAlpha = 1.0f - FMath::Exp(
		-FMath::Max(0.0f, DeltaSeconds) / FMath::Max(0.01f, DirectionBlendSeconds));
	const float DirectionDelta = FMath::FindDeltaAngleDegrees(
		CurrentWindDirectionYawDegrees,
		TargetWindDirectionYawDegrees);
	CurrentWindDirectionYawDegrees = FMath::UnwindDegrees(
		CurrentWindDirectionYawDegrees + DirectionDelta * DirectionAlpha);
	const float SpeedAlpha = 1.0f - FMath::Exp(
		-FMath::Max(0.0f, DeltaSeconds) / FMath::Max(0.01f, SpeedBlendSeconds));
	const float EffectiveTargetSpeed = bTargetDirectionIsCalm ? 0.0f : TargetWindSpeedMetersPerSecond;
	CurrentWindSpeedMetersPerSecond = FMath::Lerp(
		CurrentWindSpeedMetersPerSecond,
		EffectiveTargetSpeed,
		SpeedAlpha);

	// CALM 진입은 속도를 부드럽게 0으로, 이탈은 새 목표 세기로 부드럽게 올린다.
	bCurrentDirectionIsCalm = bTargetDirectionIsCalm && CurrentWindSpeedMetersPerSecond <= 0.05f;
	Subsystem->SetRuntimeWindOverride(CurrentWindDirectionYawDegrees, CurrentWindSpeedMetersPerSecond);
}

bool ADroneWeatherController::ApplyConfiguredWeather()
{
	UWorld* World = GetWorld();
	UDroneWeatherProfile* Profile = WeatherProfile.LoadSynchronous();
	UDroneWeatherWorldSubsystem* Subsystem = World
		? World->GetSubsystem<UDroneWeatherWorldSubsystem>()
		: nullptr;
	return Subsystem && Profile && Subsystem->ApplyWeatherProfile(Profile, bApplyInstantly);
}
