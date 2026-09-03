#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Flow/DroneGameFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Mission/DroneDefinition.h"
#include "Mission/DroneMissionDefinition.h"
#include "Prototype/DronePrototypePawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneGameFlowContractTest,
	"Drone.Flow.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneGameFlowContractTest::RunTest(const FString& Parameters)
{
	UGameInstance* CatalogGameInstance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* CatalogFlow = NewObject<UDroneGameFlowSubsystem>(CatalogGameInstance);
	TestTrue(TEXT("Default Catalog registers from saved assets"), CatalogFlow && CatalogFlow->EnsureDefaultCatalog());
	TestEqual(TEXT("Default Catalog contains one Drone"), CatalogFlow ? CatalogFlow->GetRegisteredDroneCount() : 0, 1);
	TestEqual(TEXT("Default Catalog contains one Mission"), CatalogFlow ? CatalogFlow->GetRegisteredMissionCount() : 0, 1);
	TestEqual(TEXT("Default Catalog exposes one sorted Mission ID"), CatalogFlow ? CatalogFlow->GetRegisteredMissionIds().Num() : 0, 1);
	if (CatalogFlow && CatalogFlow->GetRegisteredMissionIds().Num() == 1)
	{
		TestEqual(
			TEXT("Default Catalog exposes the Tutorial Mission ID"),
			CatalogFlow->GetRegisteredMissionIds()[0],
			FName(TEXT("Mission.Tutorial.Training")));
	}
	TestTrue(TEXT("Default Catalog registration is idempotent"), CatalogFlow && CatalogFlow->EnsureDefaultCatalog());
	TestEqual(TEXT("Idempotent registration keeps one Drone"), CatalogFlow ? CatalogFlow->GetRegisteredDroneCount() : 0, 1);
	TestEqual(TEXT("Idempotent registration keeps one Mission"), CatalogFlow ? CatalogFlow->GetRegisteredMissionCount() : 0, 1);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* Flow = NewObject<UDroneGameFlowSubsystem>(GameInstance);
	UDroneDefinition* ScoutDrone = LoadObject<UDroneDefinition>(
		nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_Scout_Greybox.DA_Drone_Scout_Greybox"));
	UDroneMissionDefinition* TutorialMission = LoadObject<UDroneMissionDefinition>(
		nullptr,
		TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Training.DA_Mission_Tutorial_Training"));
	TestNotNull(TEXT("Flow test GameInstance exists"), GameInstance);
	TestNotNull(TEXT("Flow Subsystem test instance exists inside GameInstance"), Flow);
	TestNotNull(TEXT("Scout Drone Definition exists"), ScoutDrone);
	TestNotNull(TEXT("Tutorial Mission Definition exists"), TutorialMission);
	if (!Flow || !ScoutDrone || !TutorialMission)
	{
		return false;
	}

	const FName ScoutDroneId(TEXT("Drone.Scout.Greybox"));
	const FName TutorialMissionId(TEXT("Mission.Tutorial.Training"));
	TestEqual(TEXT("Saved Scout Drone uses the stable ID"), ScoutDrone->DroneId, ScoutDroneId);
	TestEqual(TEXT("Saved Tutorial Mission uses the stable ID"), TutorialMission->MissionId, TutorialMissionId);
	UClass* ScoutPawnClass = ScoutDrone->PawnClass.LoadSynchronous();
	TestTrue(
		TEXT("Saved Scout Drone uses a project Prototype Pawn subclass"),
		ScoutPawnClass && ScoutPawnClass->IsChildOf(ADronePrototypePawn::StaticClass()));
	TestEqual(TEXT("Saved Tutorial Mission points at Training Map"), TutorialMission->MissionMap.ToSoftObjectPath(), FSoftObjectPath(TEXT("/Game/Drone/Maps/Lvl_DroneTraining.Lvl_DroneTraining")));

	TestTrue(TEXT("Valid Drone Definition registers"), Flow->RegisterDroneDefinition(ScoutDrone));
	TestFalse(TEXT("Duplicate Drone ID is rejected"), Flow->RegisterDroneDefinition(ScoutDrone));
	TestTrue(TEXT("Valid Mission with a registered Drone registers"), Flow->RegisterMissionDefinition(TutorialMission));
	TestFalse(TEXT("Duplicate Mission ID is rejected"), Flow->RegisterMissionDefinition(TutorialMission));

	UDroneMissionDefinition* UnknownDroneMission = NewObject<UDroneMissionDefinition>(Flow);
	UnknownDroneMission->MissionId = FName(TEXT("Mission.Invalid.UnknownDrone"));
	UnknownDroneMission->DisplayName = FText::FromString(TEXT("잘못된 Mission"));
	UnknownDroneMission->MissionMap = TutorialMission->MissionMap;
	UnknownDroneMission->AllowedDroneIds = {FName(TEXT("Drone.DoesNotExist"))};
	UnknownDroneMission->DefaultDroneId = UnknownDroneMission->AllowedDroneIds[0];
	TestFalse(TEXT("Mission referencing an unknown Drone ID is rejected"), Flow->RegisterMissionDefinition(UnknownDroneMission));

	TestEqual(TEXT("Flow starts at Boot"), Flow->GetSnapshot().State, EDroneGameFlowState::Boot);
	TestFalse(TEXT("Mission selection before Lobby is rejected"), Flow->SelectMission(TutorialMissionId));
	TestTrue(TEXT("Boot enters Opening Trailer once"), Flow->BeginOpeningTrailer());
	TestFalse(TEXT("Duplicate Opening Trailer request is rejected"), Flow->BeginOpeningTrailer());
	TestTrue(TEXT("Opening Trailer enters Lobby"), Flow->EnterLobbyFromOpeningTrailer());
	TestEqual(TEXT("Flow is in Lobby Mission Select"), Flow->GetSnapshot().State, EDroneGameFlowState::LobbyMissionSelect);

	TestFalse(TEXT("Unknown Mission ID is rejected without changing selection"), Flow->SelectMission(FName(TEXT("Mission.Unknown"))));
	TestTrue(TEXT("Tutorial Mission can be selected"), Flow->SelectMission(TutorialMissionId));
	TestEqual(TEXT("Selected Mission ID is preserved"), Flow->GetSnapshot().SelectedMissionId, TutorialMissionId);
	TestEqual(TEXT("Mission exposes exactly one allowed Drone"), Flow->GetSnapshot().AvailableDroneIds.Num(), 1);
	if (Flow->GetSnapshot().AvailableDroneIds.Num() == 1)
	{
		TestEqual(TEXT("Allowed Drone ID matches the Mission Definition"), Flow->GetSnapshot().AvailableDroneIds[0], ScoutDroneId);
	}
	TestFalse(TEXT("Drone cannot be selected before Mission Map is ready"), Flow->SelectDrone(ScoutDroneId));

	TestTrue(TEXT("Confirmed Mission enters Mission Trailer"), Flow->ConfirmMissionSelection());
	TestFalse(TEXT("Duplicate Mission confirmation is rejected"), Flow->ConfirmMissionSelection());
	TestTrue(TEXT("Mission Trailer completion enters Loading"), Flow->NotifyMissionTrailerFinished());
	TestTrue(TEXT("Loaded Mission Map enters Drone Select"), Flow->NotifyMissionMapReady());
	TestFalse(TEXT("Drone outside the allowed list is rejected"), Flow->SelectDrone(FName(TEXT("Drone.Unknown"))));
	TestTrue(TEXT("Allowed Scout Drone can be selected"), Flow->SelectDrone(ScoutDroneId));
	TestEqual(TEXT("Selected Drone ID is preserved"), Flow->GetSnapshot().SelectedDroneId, ScoutDroneId);

	TestTrue(TEXT("Selected Drone permits exactly one Mission start"), Flow->RequestMissionStart());
	TestEqual(TEXT("Flow enters In Mission"), Flow->GetSnapshot().State, EDroneGameFlowState::InMission);
	TestFalse(TEXT("Duplicate Mission start request is rejected"), Flow->RequestMissionStart());
	TestTrue(TEXT("Mission Director consumes the start request once"), Flow->ConsumeMissionStartRequest());
	TestFalse(TEXT("Consumed Mission start request cannot be consumed twice"), Flow->ConsumeMissionStartRequest());

	TestTrue(TEXT("In Mission accepts a Success result"), Flow->CompleteMission(EDroneMissionOutcome::Success));
	TestEqual(TEXT("Success enters Mission Result"), Flow->GetSnapshot().State, EDroneGameFlowState::MissionResult);
	TestEqual(TEXT("Success outcome is preserved"), Flow->GetSnapshot().LastMissionOutcome, EDroneMissionOutcome::Success);
	TestFalse(TEXT("Duplicate Mission completion is rejected"), Flow->CompleteMission(EDroneMissionOutcome::Failure));

	TestTrue(TEXT("Result can request a retry of the same Mission"), Flow->RequestRetry());
	TestEqual(TEXT("Retry returns to Loading Mission Map"), Flow->GetSnapshot().State, EDroneGameFlowState::LoadingMissionMap);
	TestEqual(TEXT("Retry preserves Mission ID"), Flow->GetSnapshot().SelectedMissionId, TutorialMissionId);
	TestTrue(TEXT("Retry clears the previous Drone selection"), Flow->GetSnapshot().SelectedDroneId.IsNone());
	TestEqual(TEXT("Retry clears the previous outcome"), Flow->GetSnapshot().LastMissionOutcome, EDroneMissionOutcome::None);

	TestTrue(TEXT("Retried Map enters Drone Select"), Flow->NotifyMissionMapReady());
	TestTrue(TEXT("Retry can select the allowed Drone again"), Flow->SelectDrone(ScoutDroneId));
	TestTrue(TEXT("Retry can start the Mission again"), Flow->RequestMissionStart());
	TestTrue(TEXT("Retry Mission accepts a Failure result"), Flow->CompleteMission(EDroneMissionOutcome::Failure));
	TestTrue(TEXT("Result can request return to Lobby"), Flow->RequestReturnToLobby());
	TestEqual(TEXT("Return request enters Lobby"), Flow->GetSnapshot().State, EDroneGameFlowState::LobbyMissionSelect);
	TestTrue(TEXT("Lobby return clears Mission selection"), Flow->GetSnapshot().SelectedMissionId.IsNone());
	TestTrue(TEXT("Lobby return clears Drone list"), Flow->GetSnapshot().AvailableDroneIds.IsEmpty());
	TestTrue(TEXT("Lobby return request is exposed once"), Flow->GetSnapshot().bLobbyReturnRequested);
	TestTrue(TEXT("Front-end consumes Lobby return request once"), Flow->ConsumeLobbyReturnRequest());
	TestFalse(TEXT("Consumed Lobby return request cannot be consumed twice"), Flow->ConsumeLobbyReturnRequest());

	return true;
}

#endif
