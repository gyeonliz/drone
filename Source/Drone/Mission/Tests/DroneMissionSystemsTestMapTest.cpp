#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Flow/DroneMissionGameMode.h"
#include "Mission/DroneMissionReturnZone.h"
#include "Prototype/DronePrototypeGameMode.h"
#include "Signal/DroneJammingVolume.h"

namespace DroneMissionSystemsTestMap
{
constexpr const TCHAR* MapObjectPath =
	TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneMissionSystemsTest.Lvl_DroneMissionSystemsTest");
constexpr const TCHAR* ExpectedGameModePath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
const FName OwnedTag(TEXT("DroneMissionSystemsTest.Owned"));
const FName ReturnTargetTag(TEXT("Test.Mission.ReturnZone"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMissionSystemsTestMapTest,
	"Drone.Mission.MissionSystemsTestMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneMissionSystemsTestMapTest::RunTest(const FString& Parameters)
{
	using namespace DroneMissionSystemsTestMap;

	UWorld* TestWorld = LoadObject<UWorld>(nullptr, MapObjectPath);
	TestNotNull(TEXT("Mission Systems test World loads"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	UClass* ExpectedGameMode = LoadClass<ADronePrototypeGameMode>(nullptr, ExpectedGameModePath);
	TestNotNull(TEXT("Prototype GameMode Blueprint loads"), ExpectedGameMode);
	TestTrue(
		TEXT("Mission Systems test map starts a directly controllable Prototype Drone"),
		TestWorld->GetWorldSettings() && TestWorld->GetWorldSettings()->DefaultGameMode == ExpectedGameMode);

	TArray<ADroneJammingVolume*> Jammers;
	ADroneMissionReturnZone* ReturnZone = nullptr;
	int32 OwnedActorCount = 0;
	for (TActorIterator<AActor> It(TestWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->Tags.Contains(OwnedTag))
		{
			++OwnedActorCount;
		}
		if (ADroneJammingVolume* Jammer = Cast<ADroneJammingVolume>(Actor))
		{
			Jammers.Add(Jammer);
		}
		else if (ADroneMissionReturnZone* Candidate = Cast<ADroneMissionReturnZone>(Actor))
		{
			ReturnZone = Candidate;
		}
	}

	TestEqual(TEXT("Two Jamming Volumes are placed"), Jammers.Num(), 2);
	TestNotNull(TEXT("Return Zone is placed"), ReturnZone);
	TestEqual(TEXT("Map contains the complete owned Greybox set"), OwnedActorCount, 14);

	if (Jammers.Num() == 2)
	{
		Jammers.Sort([](const ADroneJammingVolume& Left, const ADroneJammingVolume& Right)
		{
			return Left.GetNormalizedJammingStrength() < Right.GetNormalizedJammingStrength();
		});
		TestEqual(TEXT("Weak zone strength is 35 percent"), Jammers[0]->GetNormalizedJammingStrength(), 0.35f);
		TestEqual(TEXT("Strong zone strength is 80 percent"), Jammers[1]->GetNormalizedJammingStrength(), 0.80f);
		TestTrue(
			TEXT("Weak and Strong zones overlap for maximum-source arbitration"),
			Jammers[0]->GetComponentsBoundingBox().Intersect(Jammers[1]->GetComponentsBoundingBox()));
	}

	if (ReturnZone)
	{
		TestTrue(TEXT("Return Zone has the stable target Tag"), ReturnZone->Tags.Contains(ReturnTargetTag));
		TestNotNull(TEXT("Return Zone exposes its overlap Trigger"), ReturnZone->GetReturnTrigger());
	}

	return true;
}

#endif
