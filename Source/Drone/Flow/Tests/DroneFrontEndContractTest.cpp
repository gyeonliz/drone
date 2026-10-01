#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Flow/DroneFrontEndGameMode.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "GameFramework/SpectatorPawn.h"
#include "UI/DroneFrontEndRootWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFrontEndContractTest,
	"Drone.Flow.FrontEndContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneFrontEndContractTest::RunTest(const FString& Parameters)
{
	const ADroneFrontEndGameMode* GameModeDefaults = GetDefault<ADroneFrontEndGameMode>();
	const ADroneFrontEndPlayerController* ControllerDefaults = GetDefault<ADroneFrontEndPlayerController>();
	TestNotNull(TEXT("Front-end GameMode defaults exist"), GameModeDefaults);
	TestNotNull(TEXT("Front-end Controller defaults exist"), ControllerDefaults);
	if (!GameModeDefaults || !ControllerDefaults)
	{
		return false;
	}

	TestTrue(
		TEXT("Front-end uses only a non-Drone Spectator before selection"),
		GameModeDefaults->DefaultPawnClass
			&& GameModeDefaults->DefaultPawnClass->IsChildOf(ASpectatorPawn::StaticClass()));
	TestTrue(
		TEXT("Front-end uses its dedicated PlayerController"),
		GameModeDefaults->PlayerControllerClass
			&& GameModeDefaults->PlayerControllerClass->IsChildOf(ADroneFrontEndPlayerController::StaticClass()));
	TestTrue(
		TEXT("Controller creates a Blueprint-ready Front-end Root Widget"),
		ControllerDefaults->GetFrontEndWidgetClass()
			&& ControllerDefaults->GetFrontEndWidgetClass()->IsChildOf(UDroneFrontEndRootWidget::StaticClass()));

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* Flow = NewObject<UDroneGameFlowSubsystem>(GameInstance);
	TestTrue(TEXT("Default Catalog loads for the Front-end"), Flow && Flow->EnsureDefaultCatalog());
	TestEqual(TEXT("Default Catalog has five functional Drone profiles"), Flow ? Flow->GetRegisteredDroneCount() : 0, 5);
	TestEqual(TEXT("Default Catalog has nine Tutorial, one Racing, four Story Missions"), Flow ? Flow->GetRegisteredMissionCount() : 0, 14);
	TestTrue(TEXT("Opening Trailer begins once"), Flow && Flow->BeginOpeningTrailer());
	TestFalse(TEXT("Opening Trailer cannot be started twice"), Flow && Flow->BeginOpeningTrailer());
	TestTrue(TEXT("Opening Trailer can enter Lobby once"), Flow && Flow->EnterLobbyFromOpeningTrailer());
	TestEqual(
		TEXT("Front-end reaches Lobby Mission Select"),
		Flow ? Flow->GetSnapshot().State : EDroneGameFlowState::Boot,
		EDroneGameFlowState::LobbyMissionSelect);
	TestFalse(TEXT("Lobby transition cannot be repeated"), Flow && Flow->EnterLobbyFromOpeningTrailer());
	UDroneFrontEndRootWidget* Widget = NewObject<UDroneFrontEndRootWidget>(GameInstance);
	Widget->SetFlowSubsystem(Flow);
	Widget->SetSettingsVisible(true);
	TestFalse(TEXT("Settings cannot cover the mission selection lobby"), Widget->IsSettingsVisible());
	Widget->SetSettingsVisible(false);
	TestFalse(TEXT("Settings can close"), Widget->IsSettingsVisible());
	TestEqual(TEXT("Tutorial tab has nine entries"), Widget->GetVisibleMissionIds().Num(), 9);
	TestTrue(TEXT("Tutorial selection succeeds"), Widget->SelectLobbyMission(FName(TEXT("Mission.Tutorial.Heading"))));
	TestTrue(TEXT("Racing tab switches"), Widget->SetLobbyCategory(EDroneMissionCategory::Racing));
	TestEqual(TEXT("Racing tab has one entry"), Widget->GetVisibleMissionIds().Num(), 1);
	TestFalse(TEXT("Hidden Tutorial selection cannot start in Racing tab"), Widget->ConfirmSelectedMission());
	TestFalse(TEXT("Other tab button cannot select a hidden mission"), Widget->SelectLobbyMission(FName(TEXT("Mission.Tutorial.Hover"))));
	TestTrue(TEXT("Mission tab switches"), Widget->SetLobbyCategory(EDroneMissionCategory::Mission));
	TestEqual(TEXT("Story tab has four entries"), Widget->GetVisibleMissionIds().Num(), 4);
	TestFalse(TEXT("Auto is not a tab"), Widget->SetLobbyCategory(EDroneMissionCategory::Auto));
	const UDroneFrontEndRootWidget* ArtworkDefaults = GetDefault<UDroneFrontEndRootWidget>(
		LoadClass<UDroneFrontEndRootWidget>(nullptr, TEXT("/Game/Drone/FrontEnd/UI/WBP_DroneFrontEndRoot.WBP_DroneFrontEndRoot_C")));
	TestNotNull(TEXT("Title WBP class defaults exist"), ArtworkDefaults);
	if (ArtworkDefaults)
	{
		TestNotNull(TEXT("Provided Background is assigned"), ArtworkDefaults->TitleBackgroundTexture.Get());
		TestNotNull(TEXT("Provided Overlay is assigned"), ArtworkDefaults->TitleOverlayTexture.Get());
		TestNotNull(TEXT("Provided Logo is assigned"), ArtworkDefaults->TitleLogoTexture.Get());
		TestNotNull(TEXT("Provided Normal button is assigned"), ArtworkDefaults->ButtonNormalTexture.Get());
		TestNotNull(TEXT("Provided Hovered button is assigned"), ArtworkDefaults->ButtonHoveredTexture.Get());
		TestNotNull(TEXT("Provided Pressed button is assigned"), ArtworkDefaults->ButtonPressedTexture.Get());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneBackNavigationContractTest,
	"Drone.Flow.BackNavigationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneBackNavigationContractTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* Flow = NewObject<UDroneGameFlowSubsystem>(Instance);
	TestTrue(TEXT("Back test catalog loads"), Flow->EnsureDefaultCatalog());
	UDroneFrontEndRootWidget* Widget = NewObject<UDroneFrontEndRootWidget>(Instance);
	Widget->SetFlowSubsystem(Flow);
	TestFalse(TEXT("Boot has no previous screen"), Flow->RequestBackNavigation());
	TestTrue(TEXT("Title starts"), Flow->BeginOpeningTrailer());
	Widget->SetSettingsVisible(true);
	TestTrue(TEXT("Back closes only Settings"), Widget->NavigateBack());
	TestFalse(TEXT("Settings is closed"), Widget->IsSettingsVisible());
	TestEqual(TEXT("Settings back keeps title"), Flow->GetSnapshot().State, EDroneGameFlowState::OpeningTrailer);
	TestFalse(TEXT("Title back does not quit"), Widget->NavigateBack());
	TestTrue(TEXT("Title enters lobby"), Widget->FinishOpeningTrailer());
	const FName MissionId(TEXT("Mission.Racing.Circuit.Test"));
	TestTrue(TEXT("Racing tab opens"), Widget->SetLobbyCategory(EDroneMissionCategory::Racing));
	TestTrue(TEXT("Racing mission selects"), Widget->SelectLobbyMission(MissionId));
	TestTrue(TEXT("Racing briefing opens"), Widget->ConfirmSelectedMission());
	TestTrue(TEXT("Briefing back succeeds"), Widget->NavigateBack());
	TestEqual(TEXT("Briefing returns to lobby"), Flow->GetSnapshot().State, EDroneGameFlowState::LobbyMissionSelect);
	TestEqual(TEXT("Briefing keeps mission"), Flow->GetSnapshot().SelectedMissionId, MissionId);
	TestTrue(TEXT("Lobby back succeeds"), Widget->NavigateBack());
	TestEqual(TEXT("Lobby returns to title"), Flow->GetSnapshot().State, EDroneGameFlowState::OpeningTrailer);
	TestTrue(TEXT("Title return clears mission"), Flow->GetSnapshot().SelectedMissionId.IsNone());
	TestTrue(TEXT("Title return clears allowed drones"), Flow->GetSnapshot().AvailableDroneIds.IsEmpty());

	TestTrue(TEXT("Title can enter lobby again"), Widget->FinishOpeningTrailer());
	TestTrue(TEXT("Mission can select again"), Widget->SelectLobbyMission(MissionId));
	TestTrue(TEXT("Mission can confirm again"), Widget->ConfirmSelectedMission());
	TestTrue(TEXT("Mission starts loading"), Flow->NotifyMissionTrailerFinished());
	TestFalse(TEXT("Back refuses pending map load"), Flow->RequestBackNavigation());
	TestTrue(TEXT("Mission map becomes ready"), Flow->NotifyMissionMapReady());
	const TArray<UDroneDefinition*> Available = Flow->GetAvailableDroneDefinitions();
	TestFalse(TEXT("Racing has playable drones"), Available.IsEmpty());
	if (Available.IsEmpty()) return false;
	const FName DroneId = Flow->GetSnapshot().AvailableDroneIds[0];
	TestTrue(TEXT("Drone selects"), Flow->SelectDrone(DroneId));
	TestTrue(TEXT("Drone selection can go back"), Flow->RequestBackNavigation());
	TestEqual(TEXT("Drone back reaches briefing"), Flow->GetSnapshot().State, EDroneGameFlowState::MissionTrailer);
	TestTrue(TEXT("Drone back clears selected drone"), Flow->GetSnapshot().SelectedDroneId.IsNone());
	TestEqual(TEXT("Drone back keeps mission"), Flow->GetSnapshot().SelectedMissionId, MissionId);
	// 실제 Map Travel에서 새로 생성되는 FrontEnd Widget도 Racing 탭을 복원해야 한다.
	UDroneFrontEndRootWidget* ReturnedWidget = NewObject<UDroneFrontEndRootWidget>(Instance);
	ReturnedWidget->SetFlowSubsystem(Flow);
	TestTrue(TEXT("Returned briefing can go to lobby"), ReturnedWidget->NavigateBack());
	TestEqual(TEXT("Returned lobby restores selected mission tab"), ReturnedWidget->GetLobbyCategory(), EDroneMissionCategory::Racing);
	TestTrue(TEXT("Returned selection remains visible"), ReturnedWidget->GetVisibleMissionIds().Contains(MissionId));
	TestTrue(TEXT("Returned mission can confirm"), ReturnedWidget->ConfirmSelectedMission());
	TestTrue(TEXT("Returned mission can load"), Flow->NotifyMissionTrailerFinished());
	TestTrue(TEXT("Returned mission can ready"), Flow->NotifyMissionMapReady());
	TestTrue(TEXT("Returned drone can select"), Flow->SelectDrone(DroneId));
	TestTrue(TEXT("Mission can launch"), Flow->RequestMissionStart());
	TestFalse(TEXT("Back cannot abort flight"), Flow->RequestBackNavigation());
	TestEqual(TEXT("Flight stays in mission"), Flow->GetSnapshot().State, EDroneGameFlowState::InMission);
	TestTrue(TEXT("Rejected back preserves start request"), Flow->GetSnapshot().bMissionStartRequested);
	const FName Fact(TEXT("Test.BackNavigation.Completed"));
	TestTrue(TEXT("Mission result still works"), Flow->CompleteMissionWithStoryFacts(EDroneMissionOutcome::Success, {Fact}, {}));
	TestFalse(TEXT("Result uses explicit result buttons, not preflight back"), Flow->RequestBackNavigation());
	TestTrue(TEXT("Result return still works"), Flow->RequestReturnToLobby());
	TestTrue(TEXT("Result lobby can return to title"), Flow->RequestBackNavigation());
	TestTrue(TEXT("Back keeps Story Facts"), Flow->HasStoryFact(Fact));
	TestEqual(TEXT("Back keeps mission catalog"), Flow->GetRegisteredMissionCount(), 14);
	TestEqual(TEXT("Back keeps drone catalog"), Flow->GetRegisteredDroneCount(), 5);
	return !HasAnyErrors();
}

#endif
