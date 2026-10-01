#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/DroneDroppedPayload.h"
#include "Abilities/DroneRoleTestTarget.h"
#include "AI/DroneNPCCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/DroneMissionGameMode.h"
#include "GameFramework/PlayerStart.h"
#include "Mission/DroneMissionDamageTarget.h"
#include "Mission/DroneMissionDefinition.h"
#include "Mission/DroneMissionReturnZone.h"
#include "Mission/DroneMissionTrigger.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTutorialHeadingZone.h"
#include "Tutorial/DroneTutorialHoverZone.h"
#include "Components/SplineComponent.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"

namespace DroneTutorialMissionTest
{
constexpr const TCHAR* MapObjectPath =
	TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneTutorialMissionTest.Lvl_DroneTutorialMissionTest");
constexpr const TCHAR* GameModeClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Managers/BP_DroneMissionGameMode.BP_DroneMissionGameMode_C");
constexpr const TCHAR* HoverMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Hover.DA_Mission_Tutorial_Hover");
constexpr const TCHAR* ForwardMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Forward.DA_Mission_Tutorial_Forward");
constexpr const TCHAR* HeadingMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Heading.DA_Mission_Tutorial_Heading");
constexpr const TCHAR* GateMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_GateFlight.DA_Mission_Tutorial_GateFlight");
constexpr const TCHAR* PayloadMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Payload.DA_Mission_Tutorial_Payload");
constexpr const TCHAR* FPVMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_FPV.DA_Mission_Tutorial_FPV");
constexpr const TCHAR* UGVNPCMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_UGV_NPC.DA_Mission_Tutorial_UGV_NPC");
constexpr const TCHAR* UGVTurretMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_UGV_Turret.DA_Mission_Tutorial_UGV_Turret");
const FName HoverTag(TEXT("Tutorial.Hover.Zone"));
const FName ForwardTag(TEXT("Tutorial.Forward.Goal"));
const FName HeadingTag(TEXT("Tutorial.Orbit.Course"));
const FName GateCourseTag(TEXT("Tutorial.GateFlight.Course"));
const FName PayloadTag(TEXT("Tutorial.Payload.Target"));
const FName FPVTag(TEXT("Tutorial.FPV.Target"));
const FName UGVNPCTag(TEXT("Tutorial.UGV.NPC"));
const FName UGVTurretTag(TEXT("Tutorial.UGV.Turret"));
const FName ReturnTag(TEXT("Tutorial.Return.Zone"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTutorialMissionTestMapTest,
	"Drone.Tutorial.MissionLessonsTestMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTutorialMissionTestMapTest::RunTest(const FString& Parameters)
{
	using namespace DroneTutorialMissionTest;

	UWorld* World = LoadObject<UWorld>(nullptr, MapObjectPath);
	UClass* MissionGameModeClass = LoadClass<ADroneMissionGameMode>(nullptr, GameModeClassPath);
	TestNotNull(TEXT("Tutorial Mission test map loads"), World);
	TestNotNull(TEXT("Tutorial Mission GameMode Blueprint loads"), MissionGameModeClass);
	if (!World || !MissionGameModeClass)
	{
		return false;
	}
	TestTrue(TEXT("Tutorial Mission test map uses Mission GameMode"),
		World->GetWorldSettings()->DefaultGameMode.Get() == MissionGameModeClass);

	int32 PlayerStartCount = 0;
	int32 HoverZoneCount = 0;
	int32 ForwardTriggerCount = 0;
	int32 HeadingZoneCount = 0;
	int32 TrainingCourseCount = 0;
	int32 PayloadTargetCount = 0;
	int32 CarryableCount = 0;
	int32 FPVTargetCount = 0;
	int32 UGVNPCCount = 0;
	int32 UGVTurretTargetCount = 0;
	int32 ReturnZoneCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		PlayerStartCount += Actor->IsA<APlayerStart>() ? 1 : 0;
		HoverZoneCount += Actor->IsA<ADroneTutorialHoverZone>() && Actor->ActorHasTag(HoverTag) ? 1 : 0;
		ForwardTriggerCount += Actor->IsA<ADroneMissionTrigger>() && Actor->ActorHasTag(ForwardTag) ? 1 : 0;
		if (const ADroneTrainingCourse* Course = Cast<ADroneTrainingCourse>(Actor); Course && Course->ActorHasTag(HeadingTag))
		{
			++HeadingZoneCount;
			TestTrue(TEXT("Orbit spline is closed"), Course->GetCourseSpline()->IsClosedLoop());
			TestEqual(TEXT("Orbit has start, seven checkpoints, finish"), Course->GetResolvedAutomaticGateCount(), 9);
			TestTrue(TEXT("Orbit sequence is valid"), Course->GetGateSequenceComponent()->IsConfigurationValid());
			TestTrue(TEXT("Finish is at full spline length, not seven eighths"),
				FMath::IsNearlyEqual(Course->GetAutomaticGateDistanceAlongSpline(8), Course->GetCourseSpline()->GetSplineLength(), 1.f));
		}
		TrainingCourseCount += Actor->IsA<ADroneTrainingCourse>() ? 1 : 0;
		PayloadTargetCount += Actor->IsA<ADronePayloadRoleTestTarget>() && Actor->ActorHasTag(PayloadTag) ? 1 : 0;
		CarryableCount += Actor->IsA<ADroneDroppedPayload>() ? 1 : 0;
		FPVTargetCount += Actor->IsA<ADroneMissionDamageTarget>() && Actor->ActorHasTag(FPVTag) ? 1 : 0;
		UGVNPCCount += Actor->IsA<ADroneNPCCharacter>() && Actor->ActorHasTag(UGVNPCTag) ? 1 : 0;
		UGVTurretTargetCount += Actor->IsA<ADroneMissionDamageTarget>() && Actor->ActorHasTag(UGVTurretTag) ? 1 : 0;
		ReturnZoneCount += Actor->IsA<ADroneMissionReturnZone>() && Actor->ActorHasTag(ReturnTag) ? 1 : 0;
	}
	TestEqual(TEXT("Tutorial Mission map has one PlayerStart"), PlayerStartCount, 1);
	TestEqual(TEXT("Tutorial Mission map has one tagged Hover Zone"), HoverZoneCount, 1);
	TestEqual(TEXT("Tutorial Mission map has one tagged Forward Trigger"), ForwardTriggerCount, 1);
	TestEqual(TEXT("Tutorial Mission map has one tagged Orbit course"), HeadingZoneCount, 1);
	TestEqual(TEXT("Tutorial Mission map has two distinct courses"), TrainingCourseCount, 2);
	TestEqual(TEXT("Tutorial Mission map has one tagged Payload target"), PayloadTargetCount, 1);
	TestEqual(TEXT("Tutorial Mission map has one spare carryable object"), CarryableCount, 1);
	TestEqual(TEXT("Tutorial Mission map has one tagged FPV damage target"), FPVTargetCount, 1);
	TestEqual(TEXT("Tutorial Mission map has one tagged UGV NPC"), UGVNPCCount, 1);
	TestEqual(TEXT("Tutorial Mission map has one tagged UGV turret damage target"), UGVTurretTargetCount, 1);
	TestEqual(TEXT("Tutorial Mission map has one tagged Return Zone"), ReturnZoneCount, 1);

	struct FMissionExpectation
	{
		const TCHAR* Path;
		FName MissionId;
		FName DroneId;
		TArray<EDroneMissionObjectiveEvent> Events;
		TArray<FName> TargetIds;
	};
	const TArray<FMissionExpectation> Expectations =
	{
		{HoverMissionPath, FName(TEXT("Mission.Tutorial.Hover")), FName(TEXT("Drone.Scout.Greybox")),
			{EDroneMissionObjectiveEvent::HoverMaintained, EDroneMissionObjectiveEvent::ReturnToBase},
			{HoverTag, ReturnTag}},
		{ForwardMissionPath, FName(TEXT("Mission.Tutorial.Forward")), FName(TEXT("Drone.Scout.Greybox")),
			{EDroneMissionObjectiveEvent::Manual, EDroneMissionObjectiveEvent::ReturnToBase},
			{ForwardTag, ReturnTag}},
		{HeadingMissionPath, FName(TEXT("Mission.Tutorial.Heading")), FName(TEXT("Drone.Scout.Greybox")),
			{EDroneMissionObjectiveEvent::TrainingLap, EDroneMissionObjectiveEvent::ReturnToBase},
			{HeadingTag, ReturnTag}},
		{GateMissionPath, FName(TEXT("Mission.Tutorial.GateFlight")), FName(TEXT("Drone.Scout.Greybox")),
			{EDroneMissionObjectiveEvent::TrainingLap},
			{GateCourseTag}},
		{PayloadMissionPath, FName(TEXT("Mission.Tutorial.Payload")), FName(TEXT("Drone.Drop.Greybox")),
			{EDroneMissionObjectiveEvent::PayloadDelivered, EDroneMissionObjectiveEvent::ReturnToBase},
			{PayloadTag, ReturnTag}},
		{FPVMissionPath, FName(TEXT("Mission.Tutorial.FPV")), FName(TEXT("Drone.FPVStrike.Greybox")),
			{EDroneMissionObjectiveEvent::TargetDestroyed},
			{FPVTag}},
		{UGVNPCMissionPath, FName(TEXT("Mission.Tutorial.UGV.NPC")), FName(TEXT("Drone.GroundUGV.Greybox")),
			{EDroneMissionObjectiveEvent::TargetDestroyed},
			{UGVNPCTag}},
		{UGVTurretMissionPath, FName(TEXT("Mission.Tutorial.UGV.Turret")), FName(TEXT("Drone.GroundUGV.Greybox")),
			{EDroneMissionObjectiveEvent::TargetDestroyed},
			{UGVTurretTag}},
	};

	for (const FMissionExpectation& Expectation : Expectations)
	{
		UDroneMissionDefinition* Mission = LoadObject<UDroneMissionDefinition>(nullptr, Expectation.Path);
		TestNotNull(*FString::Printf(TEXT("%s loads"), *Expectation.MissionId.ToString()), Mission);
		if (!Mission)
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s validates"), *Expectation.MissionId.ToString()), Mission->IsDefinitionValid());
		TestEqual(TEXT("Tutorial Mission ID matches"), Mission->MissionId, Expectation.MissionId);
		TestEqual(TEXT("Tutorial Mission allows exactly one role Drone"), Mission->AllowedDroneIds.Num(), 1);
		TestEqual(TEXT("Tutorial Mission default Drone matches lesson"), Mission->DefaultDroneId, Expectation.DroneId);
		TestEqual(TEXT("Tutorial Mission rule count matches"), Mission->ObjectiveRules.Num(), Expectation.Events.Num());
		const FString AssetName = FSoftObjectPath(Expectation.Path).GetAssetName();
		const FString Key = AssetName.RightChop(FString(TEXT("DA_Mission_Tutorial_")).Len());
		const FString MapName = FString::Printf(TEXT("Lvl_Tutorial_%s_Test"), *Key);
		const FString IndependentPath = FString::Printf(TEXT("/Game/Drone/Maps/TestMap/Tutorial/%s.%s"), *MapName, *MapName);
		TestEqual(TEXT("Tutorial Mission points to its independent map"), Mission->MissionMap.ToSoftObjectPath(), FSoftObjectPath(IndependentPath));
		UWorld* LessonWorld = LoadObject<UWorld>(nullptr, *IndependentPath);
		TestNotNull(TEXT("Independent lesson world loads"), LessonWorld);
		if (LessonWorld)
		{
			TestTrue(TEXT("Independent lesson uses Mission GameMode"), LessonWorld->GetWorldSettings()->DefaultGameMode.Get() == MissionGameModeClass);
			for (const FName Tag : Expectation.TargetIds)
			{
				bool bFound = false;
				for (TActorIterator<AActor> It(LessonWorld); It; ++It) bFound |= It->ActorHasTag(Tag);
				TestTrue(*FString::Printf(TEXT("Objective target %s exists in its independent map"), *Tag.ToString()), bFound);
			}
		}
		for (int32 RuleIndex = 0; RuleIndex < Mission->ObjectiveRules.Num() && RuleIndex < Expectation.Events.Num(); ++RuleIndex)
		{
			TestTrue(TEXT("Tutorial Mission Event order matches"), Mission->ObjectiveRules[RuleIndex].Event == Expectation.Events[RuleIndex]);
			TestEqual(TEXT("Tutorial Mission TargetId matches tagged map Actor"), Mission->ObjectiveRules[RuleIndex].TargetId, Expectation.TargetIds[RuleIndex]);
		}
	}

	return !HasAnyErrors();
}

#endif
