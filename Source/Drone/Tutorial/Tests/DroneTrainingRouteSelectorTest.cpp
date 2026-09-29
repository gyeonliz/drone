#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingRouteSelector.h"

#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTrainingRouteSelectorTest,
	"Drone.Tutorial.TrainingRouteSelector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingRouteSelectorTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("Route selector test World exists"), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TArray<ADroneTrainingCourse*> Routes;
	for (int32 RouteIndex = 0; RouteIndex < 4; ++RouteIndex)
	{
		ADroneTrainingCourse* Route = World->SpawnActor<ADroneTrainingCourse>(
			ADroneTrainingCourse::StaticClass(),
			FTransform(FVector(0.0f, static_cast<float>(RouteIndex) * 1000.0f, 0.0f)),
			SpawnParameters);
		TestNotNull(*FString::Printf(TEXT("Route %d spawns"), RouteIndex + 1), Route);
		if (Route)
		{
			Routes.Add(Route);
		}
	}

	ADroneTrainingRouteSelector* Selector = World->SpawnActor<ADroneTrainingRouteSelector>(
		ADroneTrainingRouteSelector::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("Route selector spawns"), Selector);
	if (!Selector || Routes.Num() != 4)
	{
		return false;
	}

	Selector->ConfigureRoutes(Routes);
	TestEqual(TEXT("Four Routes are configured"), Selector->GetConfiguredRouteCount(), 4);
	TestTrue(TEXT("Number 3 activates Route 3"), Selector->ActivateRouteNumber(3));
	TestEqual(TEXT("Active Route number is 3"), Selector->GetActiveRouteNumber(), 3);
	TestTrue(TEXT("Only Route 3 reports active"), Selector->IsRouteActive(3));
	TestFalse(TEXT("Route 1 reports inactive"), Selector->IsRouteActive(1));
	TestFalse(TEXT("Route 2 reports inactive"), Selector->IsRouteActive(2));
	TestFalse(TEXT("Route 4 reports inactive"), Selector->IsRouteActive(4));
	TestFalse(TEXT("Out-of-range Route is rejected"), Selector->ActivateRouteNumber(0));
	TestFalse(TEXT("Out-of-range Route 5 is rejected by fixed selection"), Selector->ActivateRouteNumber(5));

	TestTrue(TEXT("Random activation succeeds"), Selector->ActivateRandomRoute());
	TestTrue(TEXT("Random activation remains within Routes 1-4"),
		Selector->GetActiveRouteNumber() >= 1 && Selector->GetActiveRouteNumber() <= 4);
	TestTrue(TEXT("Random activation avoids immediate repeat when possible"),
		Selector->GetActiveRouteNumber() != 3);

	int32 VisibleRouteCount = 0;
	for (ADroneTrainingCourse* Route : Routes)
	{
		VisibleRouteCount += Route && !Route->IsHidden() ? 1 : 0;
	}
	TestEqual(TEXT("Exactly one Route remains visible"), VisibleRouteCount, 1);

	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif

