#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Flow/DroneFrontEndGameMode.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
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

	TestNull(TEXT("Front-end does not spawn a Pawn before Drone selection"), GameModeDefaults->DefaultPawnClass);
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
	TestEqual(TEXT("Default Catalog has one Drone"), Flow ? Flow->GetRegisteredDroneCount() : 0, 1);
	TestEqual(TEXT("Default Catalog has one Mission"), Flow ? Flow->GetRegisteredMissionCount() : 0, 1);
	TestTrue(TEXT("Opening Trailer begins once"), Flow && Flow->BeginOpeningTrailer());
	TestFalse(TEXT("Opening Trailer cannot be started twice"), Flow && Flow->BeginOpeningTrailer());
	TestTrue(TEXT("Opening Trailer can enter Lobby once"), Flow && Flow->EnterLobbyFromOpeningTrailer());
	TestEqual(
		TEXT("Front-end reaches Lobby Mission Select"),
		Flow ? Flow->GetSnapshot().State : EDroneGameFlowState::Boot,
		EDroneGameFlowState::LobbyMissionSelect);
	TestFalse(TEXT("Lobby transition cannot be repeated"), Flow && Flow->EnterLobbyFromOpeningTrailer());

	return true;
}

#endif
