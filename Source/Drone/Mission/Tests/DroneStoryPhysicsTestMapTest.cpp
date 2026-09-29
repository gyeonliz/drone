#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/DroneMissionGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "Mission/DroneMissionDamageTarget.h"
#include "Mission/DroneMissionDefinition.h"
#include "Mission/DroneMissionReturnZone.h"
#include "Mission/DroneMissionTrigger.h"
#include "Physics/DroneCollisionResponseComponent.h"
#include "Physics/DroneBreakableWallPanel.h"
#include "Physics/DroneNetPlacementRig.h"
#include "Prototype/DronePrototypePawn.h"
#include "Signal/DroneJammingVolume.h"
#include "Vehicles/DroneGroundConformingVehicle.h"
#include "Vehicles/DroneVehicleSplineRoute.h"

namespace DroneStoryPhysicsTestMap
{
constexpr const TCHAR* PhysicsMapPath =
	TEXT("/Game/Drone/Maps/TestMap/Lvl_DronePhysicsSandbox.Lvl_DronePhysicsSandbox");
constexpr const TCHAR* PhysicsGameModePath =
	TEXT("/Game/Drone/Physics/Blueprints/BP_DronePhysicsTestGameMode.BP_DronePhysicsTestGameMode_C");
constexpr const TCHAR* PhysicsPawnPath =
	TEXT("/Game/Drone/Physics/Blueprints/BP_DronePhysicsCollisionTest.BP_DronePhysicsCollisionTest_C");
constexpr const TCHAR* MissionGameModePath =
	TEXT("/Game/Drone/Mission/Blueprints/Managers/BP_DroneMissionGameMode.BP_DroneMissionGameMode_C");

struct FStoryMapExpectation
{
	const TCHAR* MapPath;
	const TCHAR* MissionPath;
	FName MissionId;
	FName DefaultDroneId;
	TArray<EDroneMissionObjectiveEvent> Events;
	TArray<FName> TargetIds;
};

const TArray<FStoryMapExpectation> StoryExpectations =
{
	{
		TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneStory01_GoldenTimeTest.Lvl_DroneStory01_GoldenTimeTest"),
		TEXT("/Game/Drone/Data/Missions/DA_Mission_Story_GoldenTime_Test.DA_Mission_Story_GoldenTime_Test"),
		FName(TEXT("Mission.Story.GoldenTime.Test")), FName(TEXT("Drone.Drop.Greybox")),
		{EDroneMissionObjectiveEvent::PayloadDelivered, EDroneMissionObjectiveEvent::ReturnToBase},
		{FName(TEXT("Story.M1.Payload.Target")), FName(TEXT("Story.M1.Return"))}
	},
	{
		TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneStory02_InterceptTest.Lvl_DroneStory02_InterceptTest"),
		TEXT("/Game/Drone/Data/Missions/DA_Mission_Story_Intercept_Test.DA_Mission_Story_Intercept_Test"),
		FName(TEXT("Mission.Story.Intercept.Test")), FName(TEXT("Drone.FPVStrike.Greybox")),
		{EDroneMissionObjectiveEvent::TargetDestroyed},
		{FName(TEXT("Story.M2.Convoy.Target"))}
	},
	{
		TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneStory03_VeilBreakerTest.Lvl_DroneStory03_VeilBreakerTest"),
		TEXT("/Game/Drone/Data/Missions/DA_Mission_Story_VeilBreaker_Test.DA_Mission_Story_VeilBreaker_Test"),
		FName(TEXT("Mission.Story.VeilBreaker.Test")), FName(TEXT("Drone.FiberOptic.Greybox")),
		{EDroneMissionObjectiveEvent::JammingExited, EDroneMissionObjectiveEvent::ReturnToBase},
		{FName(TEXT("Story.M3.Jammer")), FName(TEXT("Story.M3.Return"))}
	},
	{
		TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneStory04_EndgameTest.Lvl_DroneStory04_EndgameTest"),
		TEXT("/Game/Drone/Data/Missions/DA_Mission_Story_Endgame_Test.DA_Mission_Story_Endgame_Test"),
		FName(TEXT("Mission.Story.Endgame.Test")), FName(TEXT("Drone.GroundUGV.Greybox")),
		{EDroneMissionObjectiveEvent::TargetDestroyed, EDroneMissionObjectiveEvent::ReturnToBase},
		{FName(TEXT("Story.M4.Command.Target")), FName(TEXT("Story.M4.Return"))}
	},
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneStoryPhysicsTestMapTest,
	"Drone.Mission.StoryPhysicsTestMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneStoryPhysicsTestMapTest::RunTest(const FString& Parameters)
{
	using namespace DroneStoryPhysicsTestMap;
	UWorld* PhysicsWorld = LoadObject<UWorld>(nullptr, PhysicsMapPath);
	UClass* PhysicsGameMode = LoadClass<AGameModeBase>(nullptr, PhysicsGameModePath);
	UClass* PhysicsPawnClass = LoadClass<ADronePrototypePawn>(nullptr, PhysicsPawnPath);
	TestNotNull(TEXT("Physics Sandbox map loads"), PhysicsWorld);
	TestNotNull(TEXT("Physics Sandbox GameMode loads"), PhysicsGameMode);
	TestNotNull(TEXT("Physics collision test Pawn loads"), PhysicsPawnClass);
	if (PhysicsWorld)
	{
		TestTrue(TEXT("Physics Sandbox uses its isolated GameMode"),
			PhysicsWorld->GetWorldSettings()->DefaultGameMode.Get() == PhysicsGameMode);
		int32 PlayerStarts = 0;
		int32 Walls = 0;
		int32 NetRigs = 0;
		int32 BreakableWalls = 0;
		for (TActorIterator<AActor> It(PhysicsWorld); It; ++It)
		{
			AActor* Actor = *It;
			PlayerStarts += Actor->IsA<APlayerStart>() ? 1 : 0;
			Walls += Actor->ActorHasTag(FName(TEXT("PhysicsSandbox.Wall"))) ? 1 : 0;
			if (const ADroneNetPlacementRig* NetRig = Cast<ADroneNetPlacementRig>(Actor))
			{
				++NetRigs;
				TestEqual(TEXT("Physics net exposes four editable anchors"), NetRig->GetAnchorCount(), 4);
				TestTrue(TEXT("Physics net builds a visible sagging grid"), NetRig->GetStrandInstanceCount() >= 12);
				const float NetWidth = FVector::Distance(NetRig->GetTopLeftCorner(), NetRig->GetTopRightCorner());
				const float NetHeight = FVector::Distance(NetRig->GetTopLeftCorner(), NetRig->GetBottomLeftCorner());
				TestTrue(TEXT("Saved Physics map net remains within 6.5 metres wide"), NetWidth <= 650.0f);
				TestTrue(TEXT("Saved Physics map net remains within 4 metres high"), NetHeight <= 400.0f);
			}
			if (const ADroneBreakableWallPanel* BreakableWall = Cast<ADroneBreakableWallPanel>(Actor))
			{
				++BreakableWalls;
				TestTrue(TEXT("Breakable wall builds a multi-piece grid"), BreakableWall->GetIntactPieceCount() >= 12);
				const FVector WallSize = BreakableWall->GetConfiguredWallSizeCentimeters();
				TestTrue(TEXT("Saved Physics map wall remains within 6.5 metres wide"), WallSize.Y <= 650.0f);
				TestTrue(TEXT("Saved Physics map wall remains within 4.5 metres high"), WallSize.Z <= 450.0f);
			}
		}
		TestEqual(TEXT("Physics Sandbox has one PlayerStart"), PlayerStarts, 1);
		TestTrue(TEXT("Physics Sandbox has wall collision surfaces"), Walls >= 2);
		TestEqual(TEXT("Physics Sandbox has one four-point net rig"), NetRigs, 1);
		TestEqual(TEXT("Physics Sandbox has one local-destruction wall"), BreakableWalls, 1);
	}
	if (const ADronePrototypePawn* PhysicsPawn =
		PhysicsPawnClass ? Cast<ADronePrototypePawn>(PhysicsPawnClass->GetDefaultObject()) : nullptr)
	{
		const UDroneCollisionResponseComponent* CollisionResponse = PhysicsPawn->GetCollisionResponseComponent();
		TestNotNull(TEXT("Physics test Pawn owns collision response component"), CollisionResponse);
		if (CollisionResponse)
		{
			TestTrue(TEXT("Physics test Pawn enables collision response"),
				CollisionResponse->IsCollisionResponseEnabled());
		}
	}

	UClass* MissionGameMode = LoadClass<ADroneMissionGameMode>(nullptr, MissionGameModePath);
	TestNotNull(TEXT("Story test Mission GameMode loads"), MissionGameMode);
	for (const FStoryMapExpectation& Expectation : StoryExpectations)
	{
		UWorld* World = LoadObject<UWorld>(nullptr, Expectation.MapPath);
		UDroneMissionDefinition* Mission = LoadObject<UDroneMissionDefinition>(nullptr, Expectation.MissionPath);
		TestNotNull(*FString::Printf(TEXT("Story map loads: %s"), Expectation.MapPath), World);
		TestNotNull(*FString::Printf(TEXT("Story Mission loads: %s"), *Expectation.MissionId.ToString()), Mission);
		if (!World || !Mission)
		{
			continue;
		}

		TestTrue(TEXT("Story test map uses Mission GameMode"),
			World->GetWorldSettings()->DefaultGameMode.Get() == MissionGameMode);
		TestTrue(TEXT("Story Mission validates"), Mission->IsDefinitionValid());
		TestEqual(TEXT("Story Mission ID matches"), Mission->MissionId, Expectation.MissionId);
		TestEqual(TEXT("Story Mission default Drone matches"), Mission->DefaultDroneId, Expectation.DefaultDroneId);
		TestEqual(TEXT("Story Mission references its own isolated map"),
			Mission->MissionMap.ToSoftObjectPath(), FSoftObjectPath(Expectation.MapPath));
		TestEqual(TEXT("Story Mission rule count matches"), Mission->ObjectiveRules.Num(), Expectation.Events.Num());
		for (int32 Index = 0; Index < Mission->ObjectiveRules.Num() && Index < Expectation.Events.Num(); ++Index)
		{
			TestTrue(TEXT("Story Mission objective event order matches"),
				Mission->ObjectiveRules[Index].Event == Expectation.Events[Index]);
			TestEqual(TEXT("Story Mission objective TargetId matches"),
				Mission->ObjectiveRules[Index].TargetId, Expectation.TargetIds[Index]);
		}
	}
	return !HasAnyErrors();
}

#endif
