#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Mission/DroneMissionDefinition.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFrontEndRootWidget.h"
#include "UI/DroneSettingsWidget.h"
#include "UI/DroneAudioSettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "UObject/UObjectIterator.h"

namespace DroneFrontEndPIE
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneFrontEnd");
constexpr const TCHAR* ControllerClassPath =
	TEXT("/Game/Drone/FrontEnd/Blueprints/BP_DroneFrontEndPlayerController.BP_DroneFrontEndPlayerController_C");
constexpr const TCHAR* WidgetClassPath =
	TEXT("/Game/Drone/FrontEnd/UI/WBP_DroneFrontEndRoot.WBP_DroneFrontEndRoot_C");

UWorld* FindPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			return Context.World();
		}
	}
	return nullptr;
}

FRequestPlaySessionParams MakePlayParams()
{
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(1);
	Settings->bLaunchSeparateServer = false;
	Settings->AddToRoot();

	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.EditorPlaySettings = Settings;
	Params.bAllowOnlineSubsystem = false;
	return Params;
}

class FValidateFrontEndPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateFrontEndPIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		if (!PIEWorld || !PIEWorld->HasBegunPlay())
		{
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Front-end PIE World did not begin play within 20 seconds"));
				return true;
			}
			return false;
		}

		UClass* ControllerClass = LoadClass<ADroneFrontEndPlayerController>(nullptr, ControllerClassPath);
		UClass* WidgetClass = LoadClass<UDroneFrontEndRootWidget>(nullptr, WidgetClassPath);
		ADroneFrontEndPlayerController* Controller = Cast<ADroneFrontEndPlayerController>(
			PIEWorld->GetFirstPlayerController());
		Test->TestNotNull(TEXT("Front-end PIE loads its Controller Blueprint Class"), ControllerClass);
		Test->TestNotNull(TEXT("Front-end PIE loads its Widget Blueprint Class"), WidgetClass);
		Test->TestNotNull(TEXT("Front-end PIE uses its dedicated Controller"), Controller);
		if (!Controller)
		{
			return true;
		}

		Test->TestTrue(
			TEXT("Front-end PIE uses BP_DroneFrontEndPlayerController"),
			ControllerClass && Controller->GetClass() == ControllerClass);
		Test->TestEqual(
			TEXT("Front-end Controller creates its Root Widget exactly once"),
			Controller->GetFrontEndWidgetCreationCount(),
			1);

		UDroneFrontEndRootWidget* FrontEndWidget = Controller->GetFrontEndWidget();
		Test->TestNotNull(TEXT("Front-end Root Widget exists"), FrontEndWidget);
		if (FrontEndWidget)
		{
			FrontEndWidget->SetSettingsVisible(true);
			Test->TestTrue(TEXT("Title Settings opens"), FrontEndWidget->IsSettingsVisible());
			UDroneSettingsWidget* SettingsWidget = FrontEndWidget->WidgetTree
				? Cast<UDroneSettingsWidget>(FrontEndWidget->WidgetTree->FindWidget(TEXT("SettingsWidget"))) : nullptr;
			Test->TestNotNull(TEXT("Root constructs and initializes its Settings child"), SettingsWidget);
			if (SettingsWidget && SettingsWidget->WidgetTree)
			{
				Test->TestFalse(TEXT("PIE Settings cannot change display mode or resolution"), SettingsWidget->AreDisplaySettingsAvailable());
				USlider* VolumeSlider = Cast<USlider>(SettingsWidget->WidgetTree->FindWidget(TEXT("MasterVolumeSlider")));
				UButton* BackButton = Cast<UButton>(SettingsWidget->WidgetTree->FindWidget(TEXT("SettingsBackButton")));
				Test->TestNotNull(TEXT("Settings builds the master volume slider"), VolumeSlider);
				Test->TestNotNull(TEXT("Settings builds the Apply button"), SettingsWidget->WidgetTree->FindWidget(TEXT("ApplySettingsButton")));
				Test->TestNotNull(TEXT("Settings builds the Defaults button"), SettingsWidget->WidgetTree->FindWidget(TEXT("RestoreDefaultsButton")));
				Test->TestNotNull(TEXT("Settings builds the Back button"), BackButton);
				for (const TCHAR* ControlName : {TEXT("DisplayModeCombo"), TEXT("ResolutionCombo"), TEXT("GraphicsCombo"), TEXT("FrameRateCombo"), TEXT("VSyncCheck")})
				{
					Test->TestNotNull(FString::Printf(TEXT("Settings contains %s"), ControlName), SettingsWidget->WidgetTree->FindWidget(ControlName));
				}
				const float InitialVolume = SettingsWidget->GetMasterVolume();
				if (VolumeSlider)
				{
					const float PreviewVolume = InitialVolume > 0.5f ? 0.25f : 0.75f;
					VolumeSlider->OnValueChanged.Broadcast(PreviewVolume);
					Test->TestEqual(TEXT("Actual slider delegate updates volume preview"), SettingsWidget->GetMasterVolume(), PreviewVolume);
				}
				if (BackButton)
				{
					BackButton->OnClicked.Broadcast();
					Test->TestFalse(TEXT("Actual child Back closes the Root Settings panel"), FrontEndWidget->IsSettingsVisible());
					Test->TestEqual(TEXT("Back cancels the unapplied volume preview"), SettingsWidget->GetMasterVolume(), InitialVolume);
					if (const UDroneAudioSettingsSubsystem* Audio = PIEWorld->GetGameInstance()->GetSubsystem<UDroneAudioSettingsSubsystem>())
					{
						Test->TestEqual(TEXT("Back restores the subsystem volume without saving"), Audio->GetMasterVolume(), InitialVolume);
					}
				}
			}
			FrontEndWidget->SetSettingsVisible(false);
			Test->TestFalse(TEXT("Title Settings closes"), FrontEndWidget->IsSettingsVisible());
			Test->TestTrue(TEXT("Front-end Root Widget is in the viewport"), FrontEndWidget->IsInViewport());
			Test->TestTrue(
				TEXT("Front-end PIE uses WBP_DroneFrontEndRoot"),
				WidgetClass && FrontEndWidget->GetClass() == WidgetClass);
			Test->TestTrue(
				TEXT("Empty Greybox WBP uses the C++ fallback layout"),
				FrontEndWidget->IsUsingNativeFallbackLayout());
			Test->TestEqual(
				TEXT("New execution displays Opening Trailer"),
				FrontEndWidget->GetDisplayedState(),
				EDroneGameFlowState::OpeningTrailer);
		}

		int32 FrontEndWidgetCount = 0;
		for (TObjectIterator<UDroneFrontEndRootWidget> It; It; ++It)
		{
			if (It->GetWorld() == PIEWorld && It->IsInViewport())
			{
				++FrontEndWidgetCount;
			}
		}
		Test->TestEqual(TEXT("PIE viewport contains one Front-end Root Widget"), FrontEndWidgetCount, 1);

		int32 DronePawnCount = 0;
		for (TActorIterator<ADronePrototypePawn> It(PIEWorld); It; ++It)
		{
			++DronePawnCount;
		}
		Test->TestEqual(TEXT("Front-end does not spawn a Drone before selection"), DronePawnCount, 0);

		UDroneGameFlowSubsystem* Flow = PIEWorld->GetGameInstance()
			? PIEWorld->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
			: nullptr;
		Test->TestNotNull(TEXT("Front-end PIE has its GameInstance Flow"), Flow);
		if (Flow)
		{
			Test->TestEqual(TEXT("PIE Catalog contains five functional Drone profiles"), Flow->GetRegisteredDroneCount(), 5);
			Test->TestTrue(TEXT("PIE Catalog contains at least fourteen Missions"), Flow->GetRegisteredMissionCount() >= 14);
		}

		if (FrontEndWidget && Flow)
		{
			Test->TestTrue(TEXT("Title Start enters the Story lobby"), FrontEndWidget->FinishOpeningTrailer());
			Test->TestEqual(
				TEXT("Start displays the Lobby in the same Widget"),
				FrontEndWidget->GetDisplayedState(),
				EDroneGameFlowState::LobbyMissionSelect);
			Test->TestTrue(TEXT("Start reuses the same Root Widget"), Controller->GetFrontEndWidget() == FrontEndWidget);
			Test->TestEqual(TEXT("Lobby still has one Root Widget creation"), Controller->GetFrontEndWidgetCreationCount(), 1);
			Test->TestEqual(TEXT("Start opens the Story category"), FrontEndWidget->GetLobbyCategory(), EDroneMissionCategory::Mission);
			Test->TestFalse(TEXT("Start opens outside the Training group"), FrontEndWidget->IsTrainingLobby());
			Test->TestFalse(TEXT("Opening completion cannot run twice"), FrontEndWidget->FinishOpeningTrailer());
			UButton* TutorialTab = Cast<UButton>(FrontEndWidget->WidgetTree->FindWidget(TEXT("TutorialTabButton")));
			UButton* RacingTab = Cast<UButton>(FrontEndWidget->WidgetTree->FindWidget(TEXT("RacingTabButton")));
			UButton* MissionTab = Cast<UButton>(FrontEndWidget->WidgetTree->FindWidget(TEXT("MissionTabButton")));
			UWidget* TrainingTabs = FrontEndWidget->WidgetTree->FindWidget(TEXT("TrainingCategoryTabs"));
			if (FrontEndWidget->IsUsingNativeFallbackLayout())
			{
				Test->TestTrue(TEXT("Native Start exposes at least four Story Mission buttons"), FrontEndWidget->GetNativeMissionButtonCount() >= 4);
				Test->TestNotNull(TEXT("Native lobby has a Tutorial tab"), TutorialTab);
				Test->TestNotNull(TEXT("Native lobby has a Racing tab"), RacingTab);
				Test->TestNotNull(TEXT("Native lobby has a Story tab"), MissionTab);
				Test->TestNotNull(TEXT("Native lobby has a Training tab group"), TrainingTabs);
				if (TutorialTab && RacingTab && MissionTab)
				{
					Test->TestEqual(TEXT("Story hides the Tutorial tab"), TutorialTab->GetVisibility(), ESlateVisibility::Collapsed);
					Test->TestEqual(TEXT("Story hides the Racing tab"), RacingTab->GetVisibility(), ESlateVisibility::Collapsed);
					Test->TestEqual(TEXT("Story uses its dedicated screen without a category tab"), MissionTab->GetVisibility(), ESlateVisibility::Collapsed);
				}
				if (TrainingTabs) Test->TestEqual(TEXT("Story hides the Training tab group"), TrainingTabs->GetVisibility(), ESlateVisibility::Collapsed);
			}

			const FName TutorialMissionId(TEXT("Mission.Tutorial.Training"));
			Test->TestFalse(TEXT("Story cannot select the hidden Tutorial Mission"), FrontEndWidget->SelectLobbyMission(TutorialMissionId));
			Test->TestTrue(TEXT("Story can return to title"), FrontEndWidget->NavigateBack());
			Test->TestTrue(TEXT("Title Training enters the Tutorial lobby"), FrontEndWidget->OpenTrainingLobby());
			Test->TestTrue(TEXT("Tutorial lobby belongs to the Training group"), FrontEndWidget->IsTrainingLobby());
			if (FrontEndWidget->IsUsingNativeFallbackLayout())
			{
				Test->TestTrue(TEXT("Training Tutorial tab exposes at least nine Mission buttons"), FrontEndWidget->GetNativeMissionButtonCount() >= 9);
				if (TutorialTab && RacingTab && MissionTab)
				{
					Test->TestEqual(TEXT("Training displays the Tutorial tab"), TutorialTab->GetVisibility(), ESlateVisibility::Visible);
					Test->TestEqual(TEXT("Training displays the Racing tab"), RacingTab->GetVisibility(), ESlateVisibility::Visible);
					Test->TestEqual(TEXT("Training hides the Story tab"), MissionTab->GetVisibility(), ESlateVisibility::Collapsed);
				}
				if (TrainingTabs) Test->TestEqual(TEXT("Training displays its nested tab group"), TrainingTabs->GetVisibility(), ESlateVisibility::Visible);
				Test->TestTrue(TEXT("Training switches to Racing"), FrontEndWidget->SetLobbyCategory(EDroneMissionCategory::Racing));
				Test->TestTrue(TEXT("Racing stays in the Training group"), FrontEndWidget->IsTrainingLobby());
				Test->TestTrue(TEXT("Training Racing tab exposes at least one Mission button"), FrontEndWidget->GetNativeMissionButtonCount() >= 1);
				Test->TestFalse(TEXT("Racing cannot select the hidden Tutorial Mission"), FrontEndWidget->SelectLobbyMission(TutorialMissionId));
				Test->TestTrue(TEXT("Training switches back to Tutorial"), FrontEndWidget->SetLobbyCategory(EDroneMissionCategory::Tutorial));
			}
			Test->TestFalse(
				TEXT("Unknown Lobby Mission is rejected"),
				FrontEndWidget->SelectLobbyMission(FName(TEXT("Mission.Unknown"))));
			Test->TestTrue(
				TEXT("Lobby selects the registered Tutorial Mission"),
				FrontEndWidget->SelectLobbyMission(TutorialMissionId));
			UDroneMissionDefinition* Mission = Flow->FindMissionDefinition(TutorialMissionId);
			Test->TestNotNull(TEXT("Selected Mission Definition exists"), Mission);
			Test->TestEqual(TEXT("Flow preserves the Lobby Mission selection"), Flow->GetSnapshot().SelectedMissionId, TutorialMissionId);
			if (Mission)
			{
				Test->TestEqual(
					TEXT("Lobby name comes from the Mission Definition"),
					FrontEndWidget->GetDisplayedMissionName().ToString(),
					Mission->DisplayName.ToString());
				Test->TestEqual(
					TEXT("Lobby description comes from the Mission Definition"),
					FrontEndWidget->GetDisplayedMissionDescription().ToString(),
					Mission->LobbyDescription.ToString());
			}
			Test->TestTrue(TEXT("Lobby Start confirms the selected Mission"), FrontEndWidget->ConfirmSelectedMission());
			Test->TestEqual(
				TEXT("Lobby Start enters Mission Trailer before loading a Map"),
				Flow->GetSnapshot().State,
				EDroneGameFlowState::MissionTrailer);
			Test->TestEqual(
				TEXT("Mission Trailer displays the selected Mission briefing"),
				FrontEndWidget->GetDisplayedState(),
				EDroneGameFlowState::MissionTrailer);
			if (Mission)
			{
				Test->TestEqual(
					TEXT("Static Briefing title comes from the selected Mission"),
					FrontEndWidget->GetDisplayedBriefingTitle().ToString(),
					Mission->DisplayName.ToString());
				Test->TestTrue(
					TEXT("Static Briefing contains the Mission description"),
					FrontEndWidget->GetDisplayedBriefingBody().ToString().Contains(
						Mission->LobbyDescription.ToString()));
			}
			Test->TestFalse(TEXT("Mission confirmation cannot run twice"), FrontEndWidget->ConfirmSelectedMission());
			UButton* BriefingBack = Cast<UButton>(FrontEndWidget->WidgetTree->FindWidget(TEXT("BriefingBackButton")));
			UButton* LobbyBack = Cast<UButton>(FrontEndWidget->WidgetTree->FindWidget(TEXT("LobbyBackButton")));
			Test->TestNotNull(TEXT("Briefing has a real Back button"), BriefingBack);
			Test->TestNotNull(TEXT("Lobby has a real Back button"), LobbyBack);
			if (BriefingBack && LobbyBack)
			{
				BriefingBack->OnClicked.Broadcast();
				Test->TestEqual(TEXT("Briefing button returns to Lobby"), FrontEndWidget->GetDisplayedState(), EDroneGameFlowState::LobbyMissionSelect);
				Test->TestEqual(TEXT("Briefing button preserves selection"), Flow->GetSnapshot().SelectedMissionId, TutorialMissionId);
				LobbyBack->OnClicked.Broadcast();
				Test->TestEqual(TEXT("Lobby button returns to Title"), FrontEndWidget->GetDisplayedState(), EDroneGameFlowState::OpeningTrailer);
				Test->TestTrue(TEXT("Title return clears selection"), Flow->GetSnapshot().SelectedMissionId.IsNone());
				Test->TestTrue(TEXT("Can enter Training again after Back"), FrontEndWidget->OpenTrainingLobby());
			}
			Test->TestTrue(TEXT("FLOW-04 still reuses the same Root Widget"), Controller->GetFrontEndWidget() == FrontEndWidget);
			Test->TestEqual(TEXT("FLOW-04 still has one Root Widget creation"), Controller->GetFrontEndWidgetCreationCount(), 1);
		}
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
};
} // namespace DroneFrontEndPIE

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFrontEndPIETest,
	"Drone.Flow.FrontEndPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneFrontEndPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneFrontEndPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Front-end PIE test requires an idle Editor"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapPackage));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateFrontEndPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
