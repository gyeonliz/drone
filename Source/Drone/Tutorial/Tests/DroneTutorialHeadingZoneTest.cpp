#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationCommon.h"
#include "Tutorial/DroneTutorialHeadingZone.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTutorialHeadingZoneTest,
	"Drone.Tutorial.HeadingZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTutorialHeadingZoneTest::RunTest(const FString& Parameters)
{
	const ADroneTutorialHeadingZone* Defaults = GetDefault<ADroneTutorialHeadingZone>();
	TestNotNull(TEXT("Heading Zone defaults exist"), Defaults);
	if (!Defaults)
	{
		return false;
	}
	TestFalse(TEXT("Heading Zone does not use Actor Tick"), Defaults->PrimaryActorTick.bCanEverTick);
	TestEqual(TEXT("Heading target defaults to east"), Defaults->GetTargetHeadingDegrees(), 90.0f);
	TestEqual(TEXT("Heading tolerance defaults to eight degrees"), Defaults->GetHeadingToleranceDegrees(), 8.0f);
	TestEqual(TEXT("Heading hold defaults to one second"), Defaults->GetRequiredHoldSeconds(), 1.0f);

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		AddError(TEXT("Could not create Heading Zone test World"));
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();
	ADroneTutorialHeadingZone* Zone = World->SpawnActor<ADroneTutorialHeadingZone>();
	ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>();
	TestNotNull(TEXT("Heading Zone spawns"), Zone);
	TestNotNull(TEXT("Test Drone spawns"), Drone);
	if (Zone && Drone)
	{
		Drone->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
		TestTrue(TEXT("Exact target heading has zero error"),
			FMath::IsNearlyZero(Zone->GetAbsoluteHeadingErrorDegrees(Drone), KINDA_SMALL_NUMBER));
		Drone->SetActorRotation(FRotator(0.0f, -179.0f, 0.0f));
		TestEqual(TEXT("Heading error uses shortest wrapped angle"),
			Zone->GetAbsoluteHeadingErrorDegrees(Drone), 91.0f);
	}
	return !HasAnyErrors();
}

#endif
