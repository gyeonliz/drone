#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionGameMode.h"
#include "Flow/DroneMissionPlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Mission/DroneMissionDefinition.h"
#include "UI/DroneFrontEndRootWidget.h"
#include "UI/DroneSelectionWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMissionEntryContractTest,
	"Drone.Flow.MissionEntryContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneMissionEntryContractTest::RunTest(const FString& Parameters)
{
	const ADroneMissionGameMode* GameModeDefaults = GetDefault<ADroneMissionGameMode>();
	const ADroneMissionPlayerController* ControllerDefaults = GetDefault<ADroneMissionPlayerController>();
	TestNotNull(TEXT("Mission GameMode defaults exist"), GameModeDefaults);
	TestNotNull(TEXT("Mission PlayerController defaults exist"), ControllerDefaults);
	if (!GameModeDefaults || !ControllerDefaults)
	{
		return false;
	}

	TestTrue(
		TEXT("Mission entry does not auto-spawn a Drone Pawn"),
		GameModeDefaults->DefaultPawnClass
			&& GameModeDefaults->DefaultPawnClass->IsChildOf(ASpectatorPawn::StaticClass()));
	TestTrue(
		TEXT("Mission entry uses a non-Drone Spectator until selection"),
		GameModeDefaults->SpectatorClass
			&& GameModeDefaults->SpectatorClass->IsChildOf(ASpectatorPawn::StaticClass()));
	TestTrue(
		TEXT("Mission entry uses its selection-aware PlayerController"),
		GameModeDefaults->PlayerControllerClass
			&& GameModeDefaults->PlayerControllerClass->IsChildOf(ADroneMissionPlayerController::StaticClass()));
	TestTrue(
		TEXT("Mission Controller has a Blueprint-ready Selection Widget"),
		ControllerDefaults->GetDroneSelectionWidgetClass()
			&& ControllerDefaults->GetDroneSelectionWidgetClass()->IsChildOf(UDroneSelectionWidget::StaticClass()));

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* Flow = NewObject<UDroneGameFlowSubsystem>(GameInstance);
	UDroneFrontEndRootWidget* FrontEndWidget = NewObject<UDroneFrontEndRootWidget>(GameInstance);
	TestTrue(TEXT("Default Catalog loads for Briefing contract"), Flow && Flow->EnsureDefaultCatalog());
	if (!Flow || !FrontEndWidget)
	{
		return false;
	}

	const FName MissionId(TEXT("Mission.Tutorial.Training"));
	FrontEndWidget->SetFlowSubsystem(Flow);
	TestTrue(TEXT("Opening starts"), Flow->BeginOpeningTrailer());
	TestTrue(TEXT("Opening enters Lobby"), FrontEndWidget->FinishOpeningTrailer());
	TestTrue(TEXT("Tutorial Mission is selected"), FrontEndWidget->SelectLobbyMission(MissionId));
	TestTrue(TEXT("Mission selection enters static Briefing"), FrontEndWidget->ConfirmSelectedMission());
	UDroneMissionDefinition* Mission = Flow->FindMissionDefinition(MissionId);
	TestNotNull(TEXT("Selected Mission Definition exists"), Mission);
	if (Mission)
	{
		TestEqual(
			TEXT("Briefing title uses the selected Definition"),
			FrontEndWidget->GetDisplayedBriefingTitle().ToString(),
			Mission->DisplayName.ToString());
	}
	TestTrue(TEXT("Briefing completion enters Loading and emits the Map boundary"), FrontEndWidget->FinishMissionBriefing());
	TestEqual(TEXT("Flow waits for the Mission Map"), Flow->GetSnapshot().State, EDroneGameFlowState::LoadingMissionMap);
	TestEqual(TEXT("Mission selection survives the Map boundary"), Flow->GetSnapshot().SelectedMissionId, MissionId);
	TestFalse(TEXT("Briefing completion cannot run twice"), FrontEndWidget->FinishMissionBriefing());
	FrontEndWidget->SetFlowSubsystem(nullptr);
	return true;
}

#endif
