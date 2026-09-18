#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Prototype/DronePrototypeGameMode.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "Weather/DroneWeatherController.h"
#include "Weather/DroneWeatherDebugVisualizer.h"
#include "Weather/DroneWeatherProfile.h"
#include "Weather/DroneWeatherResponseComponent.h"
#include "Weather/DroneWeatherWorldSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneWeatherContractTest,
	"Drone.Weather.ProfileAndWindContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneWeatherContractTest::RunTest(const FString& Parameters)
{
	// CreateNewMap이 내부 GC를 실행해도 Transient Profile이 사라지지 않도록 테스트 수명 동안 강하게 보관한다.
	TStrongObjectPtr<UDroneWeatherProfile> Profile(
		NewObject<UDroneWeatherProfile>(GetTransientPackage()));
	Profile->WeatherId = TEXT("Weather.Test.LightRain");
	Profile->GameplayUpdateHertz = 10.0f;
	Profile->TransitionSeconds = 0.0f;
	Profile->RandomSeed = 42;
	Profile->Wind.DirectionYawDegrees = 90.0f;
	Profile->Wind.BaseSpeedMetersPerSecond = 5.0f;
	Profile->Wind.GustAdditionalSpeedMetersPerSecond = 0.0f;
	Profile->Wind.DroneWindResponseMultiplier = 1.0f;
	Profile->Rain.Intensity01 = 0.5f;
	Profile->Rain.SpawnScale01 = 0.4f;
	Profile->Rain.VisibilityDistanceMeters = 300.0f;
	Profile->Rain.SurfaceWetness01 = 0.6f;

	FString ValidationError;
	TestTrue(TEXT("Finite weather profile validates"), Profile->ValidateProfile(ValidationError));
	Profile->Wind.GustIntervalSeconds = FVector2D(8.0f, 2.0f);
	TestFalse(TEXT("Reversed gust interval is rejected"), Profile->ValidateProfile(ValidationError));
	Profile->Wind.GustIntervalSeconds = FVector2D(2.0f, 8.0f);
	Profile->Wind.GustAttackSeconds = 0.0f;
	TestFalse(TEXT("Zero gust attack time is rejected"), Profile->ValidateProfile(ValidationError));
	Profile->Wind.GustAttackSeconds = 0.65f;

	const FVector FirstFlowOffset = ADroneWeatherDebugVisualizer::IntegrateFlowTravelOffset(
		FVector::ZeroVector,
		FVector(100.0f, 0.0f, 0.0f),
		1.0f,
		1.0f);
	const FVector TurnedFlowOffset = ADroneWeatherDebugVisualizer::IntegrateFlowTravelOffset(
		FirstFlowOffset,
		FVector(0.0f, 100.0f, 0.0f),
		1.0f,
		1.0f);
	TestTrue(TEXT("Changing wind direction preserves earlier flow travel instead of reprojecting it"),
		TurnedFlowOffset.Equals(FVector(100.0f, 100.0f, 0.0f), 0.01f));
	FVector FineStepOffset = FVector::ZeroVector;
	for (int32 Step = 0; Step < 100; ++Step)
	{
		FineStepOffset = ADroneWeatherDebugVisualizer::IntegrateFlowTravelOffset(
			FineStepOffset,
			FVector(200.0f, 50.0f, 0.0f),
			0.01f,
			0.75f);
	}
	const FVector CoarseStepOffset = ADroneWeatherDebugVisualizer::IntegrateFlowTravelOffset(
		FVector::ZeroVector,
		FVector(200.0f, 50.0f, 0.0f),
		1.0f,
		0.75f);
	TestTrue(TEXT("Flow offset integration is frame-step independent for constant wind"),
		FineStepOffset.Equals(CoarseStepOffset, 0.1f));

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UDroneWeatherWorldSubsystem* Weather = World
		? World->GetSubsystem<UDroneWeatherWorldSubsystem>()
		: nullptr;
	TestNotNull(TEXT("Weather subsystem exists per World"), Weather);
	if (!World || !Weather)
	{
		return false;
	}
	TestTrue(TEXT("Valid profile applies instantly"), Weather->ApplyWeatherProfile(Profile.Get(), true));
	const FDroneWeatherSnapshot Snapshot = Weather->GetSnapshot();
	TestEqual(TEXT("Weather ID is published"), Snapshot.WeatherId, Profile->WeatherId);
	TestTrue(TEXT("90 degree wind points along World Y at 5 m/s"),
		FMath::Abs(Snapshot.WindVelocityCentimetersPerSecond.X) < 0.1f
			&& FMath::IsNearlyEqual(Snapshot.WindVelocityCentimetersPerSecond.Y, 500.0f, 0.1f));
	TestTrue(TEXT("Rain intensity is published"), FMath::IsNearlyEqual(Snapshot.RainIntensity01, 0.5f));
	TestTrue(TEXT("Visibility converts meters to centimeters"),
		FMath::IsNearlyEqual(Snapshot.VisibilityDistanceCentimeters, 30000.0f));

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>(Params);
	UDroneWeatherResponseComponent* Response = Drone
		? Drone->FindComponentByClass<UDroneWeatherResponseComponent>()
		: nullptr;
	TestNotNull(TEXT("Every prototype Drone owns a Weather Response Component"), Response);
	if (Response)
	{
		const FVector AssistedDrift = Response->CalculateTargetWindDriftVelocity(
			Snapshot,
			EDroneControlMode::AssistedEasy);
		const FVector AcroDrift = Response->CalculateTargetWindDriftVelocity(
			Snapshot,
			EDroneControlMode::AcroRateRealisticGreybox);
		TestTrue(TEXT("Assisted control compensates more wind than Rate/Acro"),
			AssistedDrift.Size() < AcroDrift.Size());
		TestTrue(TEXT("Rate/Acro receives the full 5 m/s profile drift by default"),
			FMath::IsNearlyEqual(AcroDrift.Size(), 500.0f, 0.1f));
	}

	ADroneWeatherController* Controller = World->SpawnActor<ADroneWeatherController>(Params);
	TestTrue(TEXT("Placed weather controller does not Tick"), Controller && !Controller->PrimaryActorTick.bCanEverTick);

	Weather->ClearWeather(true);
	TestTrue(TEXT("Clearing weather removes wind and rain"),
		Weather->GetSnapshot().WindVelocityCentimetersPerSecond.IsNearlyZero()
			&& FMath::IsNearlyZero(Weather->GetSnapshot().RainIntensity01));
	if (Controller) Controller->Destroy();
	if (Drone) Drone->Destroy();
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneWeatherProfileAssetsTest,
	"Drone.Weather.ProfileAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneWeatherProfileAssetsTest::RunTest(const FString& Parameters)
{
	struct FExpectedWeatherAsset
	{
		const TCHAR* ObjectPath;
		FName WeatherId;
		float ExpectedBaseWindMetersPerSecond;
		float ExpectedRainIntensity01;
	};

	const FExpectedWeatherAsset ExpectedAssets[] =
	{
		{ TEXT("/Game/Drone/Data/Weather/DA_Weather_Clear.DA_Weather_Clear"),
			FName(TEXT("Weather.Clear")), 0.0f, 0.0f },
		{ TEXT("/Game/Drone/Data/Weather/DA_Weather_LightWind.DA_Weather_LightWind"),
			FName(TEXT("Weather.LightWind")), 4.0f, 0.0f },
		{ TEXT("/Game/Drone/Data/Weather/DA_Weather_RainStorm_Greybox.DA_Weather_RainStorm_Greybox"),
			FName(TEXT("Weather.RainStorm.Greybox")), 8.0f, 0.8f },
	};

	for (const FExpectedWeatherAsset& Expected : ExpectedAssets)
	{
		UDroneWeatherProfile* Profile = LoadObject<UDroneWeatherProfile>(nullptr, Expected.ObjectPath);
		TestNotNull(FString::Printf(TEXT("Weather Profile exists: %s"), Expected.ObjectPath), Profile);
		if (!Profile)
		{
			continue;
		}

		FString ValidationError;
		TestTrue(FString::Printf(TEXT("Weather Profile validates: %s"), Expected.ObjectPath),
			Profile->ValidateProfile(ValidationError));
		TestEqual(TEXT("Weather ID matches the asset contract"), Profile->WeatherId, Expected.WeatherId);
		TestTrue(TEXT("Base wind keeps the reviewed Greybox value"),
			FMath::IsNearlyEqual(Profile->Wind.BaseSpeedMetersPerSecond, Expected.ExpectedBaseWindMetersPerSecond));
		TestTrue(TEXT("Rain intensity keeps the reviewed Greybox value"),
			FMath::IsNearlyEqual(Profile->Rain.Intensity01, Expected.ExpectedRainIntensity01));
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneWeatherSystemsTestMapTest,
	"Drone.Weather.SystemsTestMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneWeatherSystemsTestMapTest::RunTest(const FString& Parameters)
{
	constexpr const TCHAR* MapObjectPath =
		TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneWeatherSystemsTest.Lvl_DroneWeatherSystemsTest");
	constexpr const TCHAR* ExpectedGameModePath =
		TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
	const FName OwnedTag(TEXT("DroneWeatherSystemsTest.Owned"));
	const FName VisualizerTag(TEXT("DroneWeatherSystemsTest.Visualizer"));

	UWorld* TestWorld = LoadObject<UWorld>(nullptr, MapObjectPath);
	UClass* ExpectedGameMode = LoadClass<ADronePrototypeGameMode>(nullptr, ExpectedGameModePath);
	TestNotNull(TEXT("Weather Systems test World loads"), TestWorld);
	TestNotNull(TEXT("Prototype GameMode Blueprint loads"), ExpectedGameMode);
	if (!TestWorld || !ExpectedGameMode)
	{
		return false;
	}

	TestTrue(TEXT("Weather test map uses the directly controllable Prototype GameMode"),
		TestWorld->GetWorldSettings()
			&& TestWorld->GetWorldSettings()->DefaultGameMode == ExpectedGameMode);

	ADroneWeatherController* WeatherController = nullptr;
	ADroneWeatherDebugVisualizer* WeatherVisualizer = nullptr;
	int32 ControllerCount = 0;
	int32 OwnedActorCount = 0;
	int32 VisualizerCount = 0;
	for (TActorIterator<AActor> It(TestWorld); It; ++It)
	{
		AActor* Actor = *It;
		OwnedActorCount += Actor && Actor->ActorHasTag(OwnedTag) ? 1 : 0;
		VisualizerCount += Actor && Actor->ActorHasTag(VisualizerTag) ? 1 : 0;
		if (ADroneWeatherDebugVisualizer* Candidate = Cast<ADroneWeatherDebugVisualizer>(Actor))
		{
			WeatherVisualizer = Candidate;
		}
		if (ADroneWeatherController* Candidate = Cast<ADroneWeatherController>(Actor))
		{
			WeatherController = Candidate;
			++ControllerCount;
		}
	}

	TestEqual(TEXT("Weather test map has one authoritative Weather Controller"), ControllerCount, 1);
	TestEqual(TEXT("Weather test map keeps its complete owned Greybox set"), OwnedActorCount, 9);
	TestEqual(TEXT("Weather test map has one readable runtime wind Visualizer"), VisualizerCount, 1);
	TestNotNull(TEXT("Weather test map Visualizer uses the project native contract"), WeatherVisualizer);
	TestEqual(TEXT("Weather Visualizer exposes twenty-four moving beads"), WeatherVisualizer ? WeatherVisualizer->GetFlowBeadCount() : 0, 24);
	TestTrue(TEXT("Weather Visualizer shows the runtime readout by default"), WeatherVisualizer && WeatherVisualizer->bShowOnScreenReadout);
	TestTrue(TEXT("Weather Visualizer enables the 1/2/3/4 mode comparison keys"), WeatherVisualizer && WeatherVisualizer->bEnableControlModeHotkeys);
	TestTrue(TEXT("Weather Visualizer smooths displayed wind instead of snapping"),
		WeatherVisualizer && WeatherVisualizer->FlowVelocityResponseSeconds > 0.0f);
	if (WeatherController)
	{
		UDroneWeatherProfile* Profile = WeatherController->WeatherProfile.LoadSynchronous();
		TestNotNull(TEXT("Weather Controller resolves its Profile"), Profile);
		TestEqual(TEXT("Weather test map starts with LightWind"),
			Profile ? Profile->WeatherId : NAME_None,
			FName(TEXT("Weather.LightWind")));
		TestTrue(TEXT("Weather test applies immediately for repeatable PIE"), WeatherController->bApplyInstantly);
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneWeatherDebugPresetTest,
	"Drone.Weather.DebugPresetEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneWeatherDebugPresetTest::RunTest(const FString& Parameters)
{
	// Transient world only: no level loading/saving and no PIE timing dependency.
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	// CreateWorld initializes once; calling InitializeNewWorld again duplicates WorldSettings.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false,
		FName(TEXT("Lvl_DroneWeatherSystemsTest")), nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Transient weather test world exists"), World)) return false;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	ADroneWeatherDebugVisualizer* Visualizer = World->SpawnActor<ADroneWeatherDebugVisualizer>(Params);
	UDroneWeatherWorldSubsystem* Weather = World->GetSubsystem<UDroneWeatherWorldSubsystem>();
	if (TestNotNull(TEXT("Visualizer exists"), Visualizer) && TestNotNull(TEXT("Subsystem exists"), Weather))
	{
		TestTrue(TEXT("Storm preset applies through the debug entry"), Visualizer->ApplyTestWeatherPreset(2));
		const FDroneWeatherSnapshot Storm = Weather->GetSnapshot();
		TestEqual(TEXT("Storm publishes its ID"), Storm.WeatherId, FName(TEXT("Weather.RainStorm.Greybox")));
		TestEqual(TEXT("Storm publishes rain intensity"), Storm.RainIntensity01, 0.8f);
		TestTrue(TEXT("Storm publishes nonzero spawn scale and shared wind"),
			Storm.RainSpawnScale01 > 0.0f && !Storm.WindVelocityCentimetersPerSecond.IsNearlyZero());
		TestTrue(TEXT("Storm drives a bounded nonzero debug preview"),
			ADroneWeatherDebugVisualizer::CalculateRainPreviewStreakCount(
				Storm.RainIntensity01, Storm.RainSpawnScale01, 80) > 0);
		TestEqual(TEXT("Preview count respects its maximum"),
			ADroneWeatherDebugVisualizer::CalculateRainPreviewStreakCount(1.0f, 1.0f, 80), 80);
		TestFalse(TEXT("Invalid preset is rejected"), Visualizer->ApplyTestWeatherPreset(3));
		TestEqual(TEXT("Invalid preset preserves snapshot"), Weather->GetSnapshot().WeatherId, Storm.WeatherId);
		TestTrue(TEXT("LightWind applies"), Visualizer->ApplyTestWeatherPreset(1));
		TestEqual(TEXT("LightWind keeps wind without rain"), Weather->GetSnapshot().RainIntensity01, 0.0f);
		TestTrue(TEXT("Clear applies"), Visualizer->ApplyTestWeatherPreset(0));
		TestTrue(TEXT("Clear removes wind and rain spawn"),
			Weather->GetSnapshot().WindVelocityCentimetersPerSecond.IsNearlyZero()
			&& FMath::IsNearlyZero(Weather->GetSnapshot().RainSpawnScale01));
		TestEqual(TEXT("Clear produces no debug rain"),
			ADroneWeatherDebugVisualizer::CalculateRainPreviewStreakCount(
				Weather->GetSnapshot().RainIntensity01, Weather->GetSnapshot().RainSpawnScale01, 80), 0);
	}
	World->DestroyWorld(false);

	// A second transient world proves the guard without changing the editor's current level.
	UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Game, false,
		FName(TEXT("NotWeatherTestMap")), nullptr, true, ERHIFeatureLevel::Num, &Values);
	ADroneWeatherDebugVisualizer* OtherVisualizer = OtherWorld
		? OtherWorld->SpawnActor<ADroneWeatherDebugVisualizer>(Params)
		: nullptr;
	TestTrue(TEXT("Debug entry cannot change another map"),
		OtherVisualizer && !OtherVisualizer->ApplyTestWeatherPreset(2));
	if (OtherWorld) OtherWorld->DestroyWorld(false);
	return !HasAnyErrors();
}

#endif
