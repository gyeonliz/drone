#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Mission/DroneMissionDefinition.h"
#include "Mission/DroneMissionDirector.h"
#include "Mission/DroneMissionReturnZone.h"
#include "Signal/DroneJammingVolume.h"
#include "Prototype/DronePrototypePawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMissionObjectiveRuleTest,
	"Drone.Mission.ObjectiveRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneMissionObjectiveRuleTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* Flow = NewObject<UDroneGameFlowSubsystem>(GameInstance);
	TestNotNull(TEXT("Editor World exists"), World);
	TestTrue(TEXT("Existing Drone/Mission Catalog loads"), Flow && Flow->EnsureDefaultCatalog());
	const ADroneMissionReturnZone* ZoneDefaults = GetDefault<ADroneMissionReturnZone>();
	TestNotNull(TEXT("Blueprint-ready Return Zone owns a Box Trigger"), ZoneDefaults ? ZoneDefaults->GetReturnTrigger() : nullptr);
	TestTrue(
		TEXT("Return Zone overlaps player Pawn collision without Tick"),
		ZoneDefaults && ZoneDefaults->GetReturnTrigger()
			&& ZoneDefaults->GetReturnTrigger()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap
			&& !ZoneDefaults->PrimaryActorTick.bCanEverTick);
	if (!World || !Flow)
	{
		return false;
	}

	UDroneMissionDefinition* Base = Flow->FindMissionDefinition(FName(TEXT("Mission.Tutorial.Training")));
	TestNotNull(TEXT("Tutorial Mission supplies shared map and Drone IDs"), Base);
	if (!Base)
	{
		return false;
	}
	UDroneMissionDefinition* Mission = NewObject<UDroneMissionDefinition>(Flow);
	Mission->MissionId = FName(TEXT("Mission.Automation.ObjectiveRules"));
	Mission->DisplayName = FText::FromString(TEXT("Rule contract test"));
	Mission->MissionMap = Base->MissionMap;
	Mission->AllowedDroneIds = Base->AllowedDroneIds;
	Mission->DefaultDroneId = Base->DefaultDroneId;

	FDroneMissionObjectiveRule Recon;
	Recon.ObjectiveId = FName(TEXT("Recon"));
	Recon.Description = FText::FromString(TEXT("Scan two unique targets"));
	Recon.Event = EDroneMissionObjectiveEvent::ReconScan;
	Recon.RequiredProgress = 2;
	Recon.TimeLimitSeconds = 30.0f;
	Recon.TargetId = FName(TEXT("MissionTarget.Recon"));
	FDroneMissionObjectiveRule Delivery;
	Delivery.ObjectiveId = FName(TEXT("Delivery"));
	Delivery.Description = FText::FromString(TEXT("Deliver payload"));
	Delivery.Event = EDroneMissionObjectiveEvent::PayloadDelivered;
	FDroneMissionObjectiveRule Escape;
	Escape.ObjectiveId = FName(TEXT("EscapeJam"));
	Escape.Description = FText::FromString(TEXT("Leave tagged jamming zone"));
	Escape.Event = EDroneMissionObjectiveEvent::JammingExited;
	Escape.TargetId = FName(TEXT("MissionTarget.Jammer"));
	FDroneMissionObjectiveRule Disable;
	Disable.ObjectiveId = FName(TEXT("DisableJam"));
	Disable.Description = FText::FromString(TEXT("Disable tagged jammer"));
	Disable.Event = EDroneMissionObjectiveEvent::JammerDisabled;
	Disable.TargetId = Escape.TargetId;
	FDroneMissionObjectiveRule Return;
	Return.ObjectiveId = FName(TEXT("Return"));
	Return.Description = FText::FromString(TEXT("Return to base"));
	Return.Event = EDroneMissionObjectiveEvent::ReturnToBase;
	Mission->ObjectiveRules = {Recon, Delivery, Escape, Disable, Return};
	const FName TargetStillAtLargeFact(TEXT("Story.TargetStillAtLarge"));
	const FName TargetEliminatedFact(TEXT("Story.TargetEliminated"));
	// 미션 2의 '차량은 미끼' 선택: 성공하면 다음 미션에 추적 목표가 남는다.
	Mission->StoryFactsGrantedOnSuccess = {TargetStillAtLargeFact};
	Mission->StoryFactsRemovedOnSuccess = {TargetEliminatedFact};
	FString Error;
	TestTrue(TEXT("New Rule objectives validate without legacy texts"), Mission->ValidateDefinition(Error));
	TestTrue(TEXT("Mission with Rule objectives registers"), Flow->RegisterMissionDefinition(Mission));

	Mission->ObjectiveRules[0].RequiredProgress = 0;
	TestFalse(TEXT("Required count zero is invalid"), Mission->ValidateDefinition(Error));
	Mission->ObjectiveRules[0].RequiredProgress = 2;
	Mission->ObjectiveRules[1].ObjectiveId = Recon.ObjectiveId;
	TestFalse(TEXT("Duplicate objective ID is invalid"), Mission->ValidateDefinition(Error));
	Mission->ObjectiveRules[1].ObjectiveId = Delivery.ObjectiveId;
	Mission->ObjectiveRules[0].TimeLimitSeconds = -1.0f;
	TestFalse(TEXT("Negative time limit is invalid"), Mission->ValidateDefinition(Error));
	Mission->ObjectiveRules[0].TimeLimitSeconds = 30.0f;
	Mission->ObjectiveRules[0].StoryFactCondition = EDroneMissionStoryFactCondition::FactPresent;
	TestFalse(TEXT("Conditional objective requires a Story Fact ID"), Mission->ValidateDefinition(Error));
	Mission->ObjectiveRules[0].StoryFactCondition = EDroneMissionStoryFactCondition::Always;
	Mission->StoryFactsRemovedOnSuccess = {TargetStillAtLargeFact};
	TestFalse(TEXT("A Story Fact cannot be granted and removed together"), Mission->ValidateDefinition(Error));
	Mission->StoryFactsRemovedOnSuccess = {TargetEliminatedFact};

	TestTrue(TEXT("Boot enters Opening"), Flow->BeginOpeningTrailer());
	TestTrue(TEXT("Opening enters Lobby"), Flow->EnterLobbyFromOpeningTrailer());
	TestTrue(TEXT("Rule Mission is selected"), Flow->SelectMission(Mission->MissionId));
	TestTrue(TEXT("Rule Mission enters Briefing"), Flow->ConfirmMissionSelection());
	TestTrue(TEXT("Briefing enters Loading"), Flow->NotifyMissionTrailerFinished());
	TestTrue(TEXT("Map ready enters Drone Select"), Flow->NotifyMissionMapReady());
	TestTrue(TEXT("Default Drone is selected"), Flow->SelectDrone(Mission->DefaultDroneId));
	TestTrue(TEXT("Mission start requested"), Flow->RequestMissionStart());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>(Params);
	ADroneMissionDirector* Director = World->SpawnActor<ADroneMissionDirector>(Params);
	AActor* ReconA = World->SpawnActor<AActor>(Params);
	AActor* ReconB = World->SpawnActor<AActor>(Params);
	AActor* WrongTarget = World->SpawnActor<AActor>(Params);
	ADroneJammingVolume* Jammer = World->SpawnActor<ADroneJammingVolume>(Params);
	TestNotNull(TEXT("Rule test Drone spawned"), Drone);
	TestNotNull(TEXT("Rule test Director spawned"), Director);
	TestNotNull(TEXT("Rule test targets spawned"), ReconA && ReconB && WrongTarget ? ReconA : nullptr);
	if (Drone && Director && ReconA && ReconB && WrongTarget && Jammer)
	{
		ReconA->Tags.Add(Recon.TargetId);
		ReconB->Tags.Add(Recon.TargetId);
		Jammer->Tags.Add(Escape.TargetId);
		TestTrue(TEXT("Director initializes five Rule objectives"), Director->InitializeMission(Flow, Mission, Drone));
		TestTrue(TEXT("Director binds the map jammer disable event"), Jammer->OnJammerDisabledNative.IsBound());
		TestEqual(TEXT("First required count comes from Data Asset"), Director->GetCurrentObjective().RequiredProgress, 2);
		TestTrue(TEXT("First objective has an active deadline"), Director->GetCurrentObjectiveTimeRemainingSeconds() > 0.0f);
		TestFalse(TEXT("Wrong event type does not advance"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::PayloadDelivered, ReconA));
		TestFalse(TEXT("Untagged target does not advance"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::ReconScan, WrongTarget));
		TestTrue(TEXT("First tagged Scan advances one count"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::ReconScan, ReconA));
		TestEqual(TEXT("HUD snapshot receives one of two"), Director->GetCurrentObjective().CurrentProgress, 1);
		TestFalse(TEXT("Same Actor cannot count twice"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::ReconScan, ReconA));
		TestTrue(TEXT("Second unique Scan advances objective"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::ReconScan, ReconB));
		TestEqual(TEXT("Next objective is Delivery"), Director->GetCurrentObjective().ObjectiveId, Delivery.ObjectiveId);
		TestTrue(TEXT("Delivered event advances to Jamming escape"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::PayloadDelivered, ReconA));
		TestFalse(TEXT("Wrong zone cannot satisfy tagged escape"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::JammingExited, WrongTarget));
		TestTrue(TEXT("Tagged zone event advances to jammer disable"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::JammingExited, Jammer));
		TestEqual(TEXT("Jammer disable objective is active before broadcast"), Director->GetCurrentObjective().Event, EDroneMissionObjectiveEvent::JammerDisabled);
		TestTrue(TEXT("Disabling bound jammer advances automatically"), Jammer->DisableJammer());
		TestEqual(TEXT("Return objective becomes active"), Director->GetCurrentObjective().Event, EDroneMissionObjectiveEvent::ReturnToBase);
		TestTrue(TEXT("Return event finishes the Mission"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::ReturnToBase, nullptr));
		TestEqual(TEXT("Rule Mission records Success"), Flow->GetSnapshot().LastMissionOutcome, EDroneMissionOutcome::Success);
		TestTrue(TEXT("Decoy branch keeps the target at large for Mission 3"), Flow->HasStoryFact(TargetStillAtLargeFact));
		TestFalse(TEXT("Decoy branch does not mark target eliminated"), Flow->HasStoryFact(TargetEliminatedFact));
		TestEqual(TEXT("Mission finishes only once"), Director->GetFinishEventCount(), 1);
		TestFalse(TEXT("Finished Mission rejects another event"), Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::ReturnToBase, nullptr));

		// 다음 Mission은 같은 Definition에서 Story Fact에 맞는 목표 한 개만 활성화한다.
		UDroneMissionDefinition* Followup = NewObject<UDroneMissionDefinition>(Flow);
		Followup->MissionId = FName(TEXT("Mission.Automation.StoryFollowup"));
		Followup->DisplayName = FText::FromString(TEXT("Story follow-up"));
		Followup->MissionMap = Base->MissionMap;
		Followup->AllowedDroneIds = Base->AllowedDroneIds;
		Followup->DefaultDroneId = Base->DefaultDroneId;
		FDroneMissionObjectiveRule HuntTarget;
		HuntTarget.ObjectiveId = FName(TEXT("EliminateTargetInMission3"));
		HuntTarget.Description = FText::FromString(TEXT("Eliminate the target in Mission 3"));
		HuntTarget.StoryFactCondition = EDroneMissionStoryFactCondition::FactPresent;
		HuntTarget.StoryFactId = TargetStillAtLargeFact;
		FDroneMissionObjectiveRule TargetAlreadyEliminated;
		TargetAlreadyEliminated.ObjectiveId = FName(TEXT("SkipEliminationObjective"));
		TargetAlreadyEliminated.Description = FText::FromString(TEXT("Target was eliminated in Mission 2"));
		TargetAlreadyEliminated.StoryFactCondition = EDroneMissionStoryFactCondition::FactPresent;
		TargetAlreadyEliminated.StoryFactId = TargetEliminatedFact;
		Followup->ObjectiveRules = {HuntTarget, TargetAlreadyEliminated};
		TestTrue(TEXT("Conditional follow-up Mission validates"), Followup->ValidateDefinition(Error));
		TestTrue(TEXT("Conditional follow-up Mission registers"), Flow->RegisterMissionDefinition(Followup));

		auto EnterMission = [this, Flow](UDroneMissionDefinition* Definition)
		{
			return Flow->SelectMission(Definition->MissionId)
				&& Flow->ConfirmMissionSelection()
				&& Flow->NotifyMissionTrailerFinished()
				&& Flow->NotifyMissionMapReady()
				&& Flow->SelectDrone(Definition->DefaultDroneId)
				&& Flow->RequestMissionStart();
		};
		TestTrue(TEXT("Decoy result returns to Lobby"), Flow->RequestReturnToLobby());
		TestTrue(TEXT("Story follow-up starts for decoy branch"), EnterMission(Followup));
		ADroneMissionDirector* DecoyFollowupDirector = World->SpawnActor<ADroneMissionDirector>(Params);
		TestTrue(TEXT("Decoy follow-up initializes"), DecoyFollowupDirector && DecoyFollowupDirector->InitializeMission(Flow, Followup, Drone));
		if (DecoyFollowupDirector)
		{
			TestEqual(TEXT("Decoy path includes Mission 3 target elimination"), DecoyFollowupDirector->GetSnapshot().Objectives.Num(), 1);
			TestEqual(TEXT("Decoy path selects target elimination objective"), DecoyFollowupDirector->GetCurrentObjective().ObjectiveId, HuntTarget.ObjectiveId);
			TestTrue(TEXT("Decoy follow-up completes"), DecoyFollowupDirector->CompleteCurrentObjective());
			DecoyFollowupDirector->Destroy();
		}

		// 반대 선택도 같은 기능 계약으로 검증한다: 차량에 실제 표적이 탑승해 미션 2에서 처리됨.
		UDroneMissionDefinition* ConfirmedVehicle = NewObject<UDroneMissionDefinition>(Flow);
		ConfirmedVehicle->MissionId = FName(TEXT("Mission.Automation.TargetAboard"));
		ConfirmedVehicle->DisplayName = FText::FromString(TEXT("Target aboard vehicle"));
		ConfirmedVehicle->MissionMap = Base->MissionMap;
		ConfirmedVehicle->AllowedDroneIds = Base->AllowedDroneIds;
		ConfirmedVehicle->DefaultDroneId = Base->DefaultDroneId;
		FDroneMissionObjectiveRule ConfirmVehicle;
		ConfirmVehicle.ObjectiveId = FName(TEXT("ConfirmVehicleKill"));
		ConfirmVehicle.Description = FText::FromString(TEXT("Confirm target eliminated with vehicle"));
		ConfirmedVehicle->ObjectiveRules = {ConfirmVehicle};
		ConfirmedVehicle->StoryFactsGrantedOnSuccess = {TargetEliminatedFact};
		ConfirmedVehicle->StoryFactsRemovedOnSuccess = {TargetStillAtLargeFact};
		TestTrue(TEXT("Target-aboard Mission registers"), Flow->RegisterMissionDefinition(ConfirmedVehicle));
		TestTrue(TEXT("Decoy follow-up returns to Lobby"), Flow->RequestReturnToLobby());
		TestTrue(TEXT("Target-aboard Mission starts"), EnterMission(ConfirmedVehicle));
		ADroneMissionDirector* TargetAboardDirector = World->SpawnActor<ADroneMissionDirector>(Params);
		TestTrue(TEXT("Target-aboard Mission initializes"), TargetAboardDirector && TargetAboardDirector->InitializeMission(Flow, ConfirmedVehicle, Drone));
		if (TargetAboardDirector)
		{
			TestTrue(TEXT("Target-aboard Mission completes"), TargetAboardDirector->CompleteCurrentObjective());
			TargetAboardDirector->Destroy();
		}
		TestTrue(TEXT("Target-aboard branch records target eliminated"), Flow->HasStoryFact(TargetEliminatedFact));
		TestFalse(TEXT("Target-aboard branch clears target-at-large fact"), Flow->HasStoryFact(TargetStillAtLargeFact));
		TestTrue(TEXT("Target-aboard result returns to Lobby"), Flow->RequestReturnToLobby());
		TestTrue(TEXT("Story follow-up starts for target-aboard branch"), EnterMission(Followup));
		ADroneMissionDirector* AboardFollowupDirector = World->SpawnActor<ADroneMissionDirector>(Params);
		TestTrue(TEXT("Target-aboard follow-up initializes"), AboardFollowupDirector && AboardFollowupDirector->InitializeMission(Flow, Followup, Drone));
		if (AboardFollowupDirector)
		{
			TestEqual(TEXT("Target-aboard path includes only one conditional objective"), AboardFollowupDirector->GetSnapshot().Objectives.Num(), 1);
			TestEqual(TEXT("Target-aboard path skips Mission 3 elimination"), AboardFollowupDirector->GetCurrentObjective().ObjectiveId, TargetAlreadyEliminated.ObjectiveId);
			TestTrue(TEXT("Target-aboard follow-up completes"), AboardFollowupDirector->CompleteCurrentObjective());
			AboardFollowupDirector->Destroy();
		}
	}
	if (Jammer) Jammer->Destroy();
	if (WrongTarget) WrongTarget->Destroy();
	if (ReconB) ReconB->Destroy();
	if (ReconA) ReconA->Destroy();
	if (Director) Director->Destroy();
	if (Drone) Drone->Destroy();
	return !HasAnyErrors();
}

#endif
