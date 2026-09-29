#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingRouteSelector.h"

namespace DroneTrainingRouteSelectionTestMap
{
constexpr const TCHAR* MapPath =
	TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneTrainingRouteSelectionTest.Lvl_DroneTrainingRouteSelectionTest");
constexpr const TCHAR* GameModePath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTrainingRouteSelectionTestMapTest,
	"Drone.Tutorial.TrainingRouteSelectionTestMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingRouteSelectionTestMapTest::RunTest(const FString& Parameters)
{
	using namespace DroneTrainingRouteSelectionTestMap;
	UWorld* World = LoadObject<UWorld>(nullptr, MapPath);
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, GameModePath);
	TestNotNull(TEXT("Training Route selection TestMap loads"), World);
	TestNotNull(TEXT("Prototype GameMode loads"), GameModeClass);
	if (!World || !GameModeClass)
	{
		return false;
	}

	TestTrue(TEXT("Training Route TestMap uses Prototype GameMode"),
		World->GetWorldSettings()->DefaultGameMode.Get() == GameModeClass);

	int32 PlayerStartCount = 0;
	int32 CourseCount = 0;
	int32 SelectorCount = 0;
	TSet<FName> UniqueCourseIds;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		PlayerStartCount += Actor->IsA<APlayerStart>() ? 1 : 0;
		if (const ADroneTrainingCourse* Course = Cast<ADroneTrainingCourse>(Actor))
		{
			++CourseCount;
			UniqueCourseIds.Add(Course->GetCourseId());
			TestTrue(TEXT("Every selectable Route uses automatic Gates"), Course->IsUsingAutomaticSplineGates());
			TestEqual(TEXT("Every selectable Route has five Gates"), Course->GetResolvedAutomaticGateCount(), 5);
			TestTrue(TEXT("Every selectable Route has an editable curved Spline"),
				Course->GetCourseSpline() && Course->GetCourseSpline()->GetNumberOfSplinePoints() >= 5);
		}
		if (const ADroneTrainingRouteSelector* Selector = Cast<ADroneTrainingRouteSelector>(Actor))
		{
			++SelectorCount;
			TestEqual(TEXT("Selector stores Routes 1-4"), Selector->GetConfiguredRouteCount(), 4);
		}
	}

	TestEqual(TEXT("Training Route TestMap has one PlayerStart"), PlayerStartCount, 1);
	TestEqual(TEXT("Training Route TestMap has four Courses"), CourseCount, 4);
	TestEqual(TEXT("Training Route TestMap has four unique Course IDs"), UniqueCourseIds.Num(), 4);
	TestEqual(TEXT("Training Route TestMap has one selector"), SelectorCount, 1);
	return !HasAnyErrors();
}

#endif

