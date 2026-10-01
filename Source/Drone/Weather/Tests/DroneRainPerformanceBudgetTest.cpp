#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "Weather/DroneRainVisualActor.h"
#include "Weather/DroneWeatherProfile.h"
#include "Weather/DroneWeatherWorldSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneRainPerformanceBudgetTest, "Drone.Weather.RainPerformanceBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDroneRainPerformanceBudgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UDroneWeatherProfile* Profile = NewObject<UDroneWeatherProfile>(World);
	Profile->WeatherId = TEXT("Weather.Test.RainBudget");
	Profile->Rain.Intensity01 = 1.f;
	Profile->Rain.SpawnScale01 = 1.f;
	UDroneWeatherWorldSubsystem* Weather = World->GetSubsystem<UDroneWeatherWorldSubsystem>();
	TestTrue(TEXT("Rain budget profile applies"), Weather->ApplyWeatherProfile(Profile, true));
	ADroneRainVisualActor* Rain = World->SpawnActor<ADroneRainVisualActor>();
	Rain->MaximumStreakCount = 512;
	Rain->CeilingTraceBudgetPerFrame = 3;
	Rain->UpdateRainForCamera(1.f / 60.f, FVector::ZeroVector);
	TestEqual(TEXT("First activation does not trace all 512 columns"), Rain->GetLastCeilingTraceColumnCount(), 3);
	Rain->UpdateRainForCamera(1.f / 60.f, FVector::ZeroVector);
	TestEqual(TEXT("Subsequent frames retain the same bounded trace budget"), Rain->GetLastCeilingTraceColumnCount(), 3);
	Weather->SetRuntimeRainEnabled(false);
	Rain->UpdateRainForCamera(1.f / 60.f, FVector::ZeroVector);
	TestEqual(TEXT("Clear weather does no ceiling column traces"), Rain->GetLastCeilingTraceColumnCount(), 0);
	Rain->SetRainEnabled(false);
	Weather->SetRuntimeRainEnabled(true);
	Rain->UpdateRainForCamera(1.f / 60.f, FVector::ZeroVector);
	TestEqual(TEXT("Explicitly disabled rain does no ceiling column traces"), Rain->GetLastCeilingTraceColumnCount(), 0);
	return !HasAnyErrors();
}
#endif
