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
	int32 ControllerCount = 0;
	int32 OwnedActorCount = 0;
	for (TActorIterator<AActor> It(TestWorld); It; ++It)
	{
		AActor* Actor = *It;
		OwnedActorCount += Actor && Actor->ActorHasTag(OwnedTag) ? 1 : 0;
		if (ADroneWeatherController* Candidate = Cast<ADroneWeatherController>(Actor))
		{
			WeatherController = Candidate;
			++ControllerCount;
		}
	}

	TestEqual(TEXT("Weather test map has one authoritative Weather Controller"), ControllerCount, 1);
	TestEqual(TEXT("Weather test map keeps its complete owned Greybox set"), OwnedActorCount, 8);
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

#endif
