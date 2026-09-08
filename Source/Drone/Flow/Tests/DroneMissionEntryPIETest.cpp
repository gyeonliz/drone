#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionGameMode.h"
#include "Flow/DroneMissionPlayerController.h"
#include "HAL/PlatformTime.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneMissionDirector.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFrontEndRootWidget.h"
#include "UI/DroneMissionObjectiveWidget.h"
#include "UI/DroneMissionResultWidget.h"
#include "UI/DroneSelectionWidget.h"

namespace DroneMissionEntryPIE
{
constexpr const TCHAR* FrontEndMapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneFrontEnd");
constexpr const TCHAR* MissionMapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneTraining");

UWorld* FindFrontEndPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType == EWorldType::PIE
			&& World
			&& World->GetFirstPlayerController<ADroneFrontEndPlayerController>())
		{
			return World;
		}
	}
	return nullptr;
}

UWorld* FindMissionPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType == EWorldType::PIE
			&& World
			&& World->GetFirstPlayerController<ADroneMissionPlayerController>())
		{
			return World;
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

/**
 * Engine의 FStartPIEForAutomationCommand는 생성 즉시 EndPIE Delegate를 구독한다.
 * 세 실행분을 한꺼번에 Queue하면 아직 시작하지 않은 2·3회 명령도 1회 종료를 받아 실패하므로,
 * 실제 차례가 왔을 때 내부 Start 명령을 생성한다.
 */
class FLazyStartPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FLazyStartPIECommand(FRequestPlaySessionParams InRequestParams)
		: RequestParams(MoveTemp(InRequestParams))
	{
	}

	virtual ~FLazyStartPIECommand() override
	{
		// 내부 명령이 생성되지 않은 채 테스트가 중단된 경우 MakePlayParams의 Root를 정리한다.
		if (!InnerCommand)
		{
			if (ULevelEditorPlaySettings* Settings = RequestParams.EditorPlaySettings.Get())
			{
				Settings->RemoveFromRoot();
			}
		}
	}

	virtual bool Update() override
	{
		if (!InnerCommand)
		{
			InnerCommand = MakeUnique<FStartPIEForAutomationCommand>(RequestParams);
		}
		return InnerCommand->Update();
	}

private:
	FRequestPlaySessionParams RequestParams;
	TUniquePtr<FStartPIEForAutomationCommand> InnerCommand;
};

class FTriggerMissionTravelCommand final : public IAutomationLatentCommand
{
public:
	explicit FTriggerMissionTravelCommand(FAutomationTestBase* InTest)
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
		UWorld* World = FindFrontEndPIEWorld();
		if (!World || !World->HasBegunPlay())
		{
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Front-end PIE World did not become ready for Mission travel"));
				return true;
			}
			return false;
		}

		ADroneFrontEndPlayerController* Controller = World->GetFirstPlayerController<ADroneFrontEndPlayerController>();
		UDroneFrontEndRootWidget* Widget = Controller ? Controller->GetFrontEndWidget() : nullptr;
		UDroneGameFlowSubsystem* Flow = World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
			: nullptr;
		Test->TestNotNull(TEXT("Travel test has Front-end Controller"), Controller);
		Test->TestNotNull(TEXT("Travel test has Front-end Widget"), Widget);
		Test->TestNotNull(TEXT("Travel test has Game Flow"), Flow);
		if (!Controller || !Widget || !Flow)
		{
			return true;
		}
		Test->TestEqual(TEXT("Each fresh run creates one Front-end Root Widget"), Controller->GetFrontEndWidgetCreationCount(), 1);

		const FName MissionId(TEXT("Mission.Tutorial.Training"));
		Test->TestTrue(TEXT("Travel path finishes Opening"), Widget->FinishOpeningTrailer());
		Test->TestTrue(TEXT("Travel path selects Tutorial Mission"), Widget->SelectLobbyMission(MissionId));
		Test->TestTrue(TEXT("Travel path confirms Tutorial Mission"), Widget->ConfirmSelectedMission());
		Test->TestTrue(TEXT("Travel path finishes static Briefing"), Widget->FinishMissionBriefing());
		Test->TestEqual(TEXT("Front-end emits one Map load request"), Controller->GetMissionMapLoadRequestCount(), 1);
		Test->TestEqual(
			TEXT("Front-end requests the Definition's Training Map"),
			Controller->GetLastRequestedMissionMap(),
			FSoftObjectPath(TEXT("/Game/Drone/Maps/Lvl_DroneTraining.Lvl_DroneTraining")));
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
};

class FValidateMissionSelectionAndSpawnCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateMissionSelectionAndSpawnCommand(FAutomationTestBase* InTest)
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
		UWorld* World = FindMissionPIEWorld();
		if (!World || !World->HasBegunPlay())
		{
			if (Now - StartedAt > 30.0)
			{
				Test->AddError(TEXT("Mission Map did not load with DroneMissionPlayerController within 30 seconds"));
				return true;
			}
			return false;
		}

		ADroneMissionPlayerController* Controller = World->GetFirstPlayerController<ADroneMissionPlayerController>();
		UDroneGameFlowSubsystem* Flow = World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
			: nullptr;
		UDroneSelectionWidget* SelectionWidget = Controller ? Controller->GetDroneSelectionWidget() : nullptr;
		Test->TestTrue(TEXT("Loaded World is the selected Training Map"), World->GetMapName().Contains(TEXT("Lvl_DroneTraining")));
		Test->TestTrue(TEXT("URL override applies Mission GameMode"), World->GetAuthGameMode<ADroneMissionGameMode>() != nullptr);
		Test->TestNotNull(TEXT("Mission Map uses Mission PlayerController"), Controller);
		Test->TestNotNull(TEXT("Mission Map preserves Game Flow"), Flow);
		Test->TestNotNull(TEXT("Mission Map creates Drone Selection Widget"), SelectionWidget);
		if (!Controller || !Flow || !SelectionWidget)
		{
			return true;
		}

		Test->TestEqual(TEXT("Map ready enters Drone Select"), Flow->GetSnapshot().State, EDroneGameFlowState::DroneSelect);
		Test->TestEqual(
			TEXT("Selected Mission survives OpenLevel"),
			Flow->GetSnapshot().SelectedMissionId,
			FName(TEXT("Mission.Tutorial.Training")));
		Test->TestEqual(TEXT("Three role Drones remain available"), Flow->GetSnapshot().AvailableDroneIds.Num(), 3);
		Test->TestTrue(TEXT("Native Selection fallback is usable without a finished WBP"), SelectionWidget->IsUsingNativeFallbackLayout());

		int32 BeforeSpawnCount = 0;
		for (TActorIterator<ADronePrototypePawn> It(World); It; ++It)
		{
			++BeforeSpawnCount;
		}
		Test->TestEqual(TEXT("No Drone exists before selection"), BeforeSpawnCount, 0);

		const FName ScoutId(TEXT("Drone.Scout.Greybox"));
		Test->TestTrue(TEXT("Selection Widget accepts the Scout role"), SelectionWidget->SelectDrone(ScoutId));
		Test->TestEqual(TEXT("Scout selection is displayed"), SelectionWidget->GetDisplayedDroneId(), ScoutId);
		SelectionWidget->ToggleControlMode();
		SelectionWidget->CycleHandlingPreset();
		Test->TestEqual(
			TEXT("Control mode is independently changed before launch"),
			SelectionWidget->GetSelectedControlMode(),
			EDroneControlMode::ManualRealisticGreybox);
		Test->TestEqual(
			TEXT("Handling preset is independently changed before launch"),
			SelectionWidget->GetSelectedHandlingPreset(),
			EDroneHandlingPreset::Agile);
		Test->TestTrue(TEXT("Confirmed role Drone spawns and is possessed"), SelectionWidget->ConfirmAndLaunchSelectedDrone());

		ADronePrototypePawn* Drone = Controller->GetSpawnedDrone();
		ADroneMissionDirector* Director = Controller->GetMissionDirector();
		UDroneMissionObjectiveWidget* ObjectiveWidget = Controller->GetMissionObjectiveWidget();
		Test->TestNotNull(TEXT("Mission Controller owns the spawned Drone"), Drone);
		Test->TestNotNull(TEXT("Drone launch creates one Mission Director"), Director);
		Test->TestNotNull(TEXT("Drone launch creates one Event-driven Objective Widget"), ObjectiveWidget);
		Test->TestTrue(TEXT("Mission Controller possesses the spawned Drone"), Drone && Controller->GetPawn() == Drone);
		Test->TestEqual(TEXT("Exactly one successful Drone spawn is recorded"), Controller->GetSuccessfulDroneSpawnCount(), 1);
		Test->TestEqual(TEXT("Spawned Pawn receives the selected Definition"), Drone ? Drone->GetAppliedDroneId() : NAME_None, ScoutId);
		Test->TestEqual(
			TEXT("Spawned Pawn receives the selected control mode"),
			Drone ? Drone->GetControlMode() : EDroneControlMode::AssistedEasy,
			EDroneControlMode::ManualRealisticGreybox);
		Test->TestEqual(
			TEXT("Spawned Pawn receives the selected handling preset"),
			Drone ? Drone->GetHandlingPreset() : EDroneHandlingPreset::Stable,
			EDroneHandlingPreset::Agile);
		Test->TestEqual(TEXT("Successful launch enters In Mission"), Flow->GetSnapshot().State, EDroneGameFlowState::InMission);
		Test->TestFalse(TEXT("Mission Director consumes the start request once"), Flow->GetSnapshot().bMissionStartRequested);
		if (Director && ObjectiveWidget)
		{
			Test->TestTrue(TEXT("Mission Director is active only after launch"), Director->IsMissionActive());
			Test->TestTrue(
				TEXT("Objective Widget displays the Definition objective"),
				!ObjectiveWidget->GetObjectiveDisplayText().IsEmpty());
			Test->TestTrue(TEXT("Completing the only Training objective succeeds"), Director->CompleteCurrentObjective());
			Test->TestEqual(TEXT("Objective completion enters Mission Result"), Flow->GetSnapshot().State, EDroneGameFlowState::MissionResult);
			Test->TestEqual(TEXT("Objective completion records Success"), Flow->GetSnapshot().LastMissionOutcome, EDroneMissionOutcome::Success);
			Test->TestEqual(TEXT("Mission finishes exactly once"), Director->GetFinishEventCount(), 1);
			Test->TestFalse(TEXT("Finished objective cannot complete twice"), Director->CompleteCurrentObjective());
			UDroneMissionResultWidget* ResultWidget = Controller->GetMissionResultWidget();
			Test->TestNotNull(TEXT("Mission Success creates the Result Widget"), ResultWidget);
			Test->TestEqual(
				TEXT("Result Widget displays Success"),
				ResultWidget ? ResultWidget->GetDisplayedOutcome() : EDroneMissionOutcome::None,
				EDroneMissionOutcome::Success);
			Test->TestNull(TEXT("Result removes the active Objective Widget"), Controller->GetMissionObjectiveWidget());
		}
		Test->TestFalse(
			TEXT("A second launch cannot create another Drone"),
			Controller->StartSelectedDrone(EDroneControlMode::AssistedEasy, EDroneHandlingPreset::Stable));

		int32 AfterSpawnCount = 0;
		for (TActorIterator<ADronePrototypePawn> It(World); It; ++It)
		{
			++AfterSpawnCount;
		}
		Test->TestEqual(TEXT("Mission contains exactly one player Drone after launch"), AfterSpawnCount, 1);
		int32 DirectorCount = 0;
		for (TActorIterator<ADroneMissionDirector> It(World); It; ++It)
		{
			++DirectorCount;
		}
		Test->TestEqual(TEXT("Mission contains exactly one Director after launch"), DirectorCount, 1);
		UDroneMissionResultWidget* ResultWidget = Controller->GetMissionResultWidget();
		Test->TestTrue(TEXT("Success Result requests the same Mission retry"), ResultWidget && ResultWidget->RequestRetry());
		Test->TestFalse(TEXT("Retry request cannot be submitted twice"), Controller->RetrySelectedMission());
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
};

class FValidateRetryFailureAndLobbyRequestCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateRetryFailureAndLobbyRequestCommand(FAutomationTestBase* InTest)
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
		UWorld* ReadyWorld = nullptr;
		ADroneMissionPlayerController* Controller = nullptr;
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* Candidate = Context.World();
				ADroneMissionPlayerController* CandidateController = Candidate
					? Candidate->GetFirstPlayerController<ADroneMissionPlayerController>()
					: nullptr;
				if (Context.WorldType == EWorldType::PIE
					&& Candidate
					&& Candidate->HasBegunPlay()
					&& CandidateController
					&& CandidateController->GetSuccessfulDroneSpawnCount() == 0
					&& CandidateController->GetDroneSelectionWidget())
				{
					ReadyWorld = Candidate;
					Controller = CandidateController;
					break;
				}
			}
		}
		if (!ReadyWorld || !Controller)
		{
			if (Now - StartedAt > 30.0)
			{
				Test->AddError(TEXT("Retried Mission Map did not return to Drone Select within 30 seconds"));
				return true;
			}
			return false;
		}

		UDroneGameFlowSubsystem* Flow = ReadyWorld->GetGameInstance()
			? ReadyWorld->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
			: nullptr;
		UDroneSelectionWidget* SelectionWidget = Controller->GetDroneSelectionWidget();
		Test->TestNotNull(TEXT("Retry preserves Game Flow"), Flow);
		Test->TestEqual(
			TEXT("Retry returns to Drone Select with no prior Drone selection"),
			Flow ? Flow->GetSnapshot().State : EDroneGameFlowState::Boot,
			EDroneGameFlowState::DroneSelect);
		Test->TestTrue(TEXT("Retry clears the selected Drone"), Flow && Flow->GetSnapshot().SelectedDroneId.IsNone());
		Test->TestEqual(
			TEXT("Retry preserves the selected Mission"),
			Flow ? Flow->GetSnapshot().SelectedMissionId : NAME_None,
			FName(TEXT("Mission.Tutorial.Training")));
		if (!Flow || !SelectionWidget)
		{
			return true;
		}

		const FName DropId(TEXT("Drone.Drop.Greybox"));
		Test->TestTrue(TEXT("Retry can select a different role Drone"), SelectionWidget->SelectDrone(DropId));
		Test->TestTrue(TEXT("Retry launches exactly one Drop Drone"), SelectionWidget->ConfirmAndLaunchSelectedDrone());
		ADronePrototypePawn* Drone = Controller->GetSpawnedDrone();
		ADroneMissionDirector* Director = Controller->GetMissionDirector();
		Test->TestNotNull(TEXT("Retry creates a fresh Drone"), Drone);
		Test->TestNotNull(TEXT("Retry creates a fresh Mission Director"), Director);
		Test->TestTrue(
			TEXT("Destroying the Drone applies the Mission failure rule"),
			Drone && Drone->GetHealthComponent()->ApplyHealthDamage(100.0f, Controller, Drone));
		Test->TestEqual(TEXT("Drone death enters Mission Result"), Flow->GetSnapshot().State, EDroneGameFlowState::MissionResult);
		Test->TestEqual(TEXT("Drone death records Failure"), Flow->GetSnapshot().LastMissionOutcome, EDroneMissionOutcome::Failure);
		Test->TestEqual(TEXT("Failure Director finishes once"), Director ? Director->GetFinishEventCount() : 0, 1);
		UDroneMissionResultWidget* ResultWidget = Controller->GetMissionResultWidget();
		Test->TestNotNull(TEXT("Failure creates a Result Widget"), ResultWidget);
		Test->TestEqual(
			TEXT("Result Widget displays Failure"),
			ResultWidget ? ResultWidget->GetDisplayedOutcome() : EDroneMissionOutcome::None,
			EDroneMissionOutcome::Failure);
		Test->TestTrue(TEXT("Failure Result requests Lobby return"), ResultWidget && ResultWidget->RequestReturnToLobby());
		Test->TestFalse(TEXT("Lobby return request cannot be submitted twice"), Controller->ReturnToFrontEndLobby());
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
};

class FValidateLobbyReturnCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateLobbyReturnCommand(FAutomationTestBase* InTest)
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
		UWorld* World = FindFrontEndPIEWorld();
		ADroneFrontEndPlayerController* Controller = World
			? World->GetFirstPlayerController<ADroneFrontEndPlayerController>()
			: nullptr;
		UDroneFrontEndRootWidget* Widget = Controller ? Controller->GetFrontEndWidget() : nullptr;
		UDroneGameFlowSubsystem* Flow = World && World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
			: nullptr;
		if (!World || !World->HasBegunPlay() || !Controller || !Widget || !Flow
			|| Flow->GetSnapshot().State != EDroneGameFlowState::LobbyMissionSelect)
		{
			if (Now - StartedAt > 30.0)
			{
				Test->AddError(TEXT("Front-end Lobby did not reload after Mission failure within 30 seconds"));
				return true;
			}
			return false;
		}

		Test->TestEqual(TEXT("Returned Front-end displays Lobby"), Widget->GetDisplayedState(), EDroneGameFlowState::LobbyMissionSelect);
		Test->TestEqual(TEXT("Returned Lobby creates one fresh Root Widget"), Controller->GetFrontEndWidgetCreationCount(), 1);
		Test->TestFalse(TEXT("Front-end consumes the Lobby return request"), Flow->GetSnapshot().bLobbyReturnRequested);
		Test->TestTrue(TEXT("Lobby return clears Mission selection"), Flow->GetSnapshot().SelectedMissionId.IsNone());
		Test->TestTrue(TEXT("Lobby return clears available Drone list"), Flow->GetSnapshot().AvailableDroneIds.IsEmpty());
		int32 DroneCount = 0;
		for (TActorIterator<ADronePrototypePawn> It(World); It; ++It)
		{
			++DroneCount;
		}
		Test->TestEqual(TEXT("Returned Lobby contains no Drone"), DroneCount, 0);
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
};

/**
 * FEndPlayMapCommand는 종료를 요청한 직후 완료될 수 있다.
 * 다음 독립 실행을 시작하기 전에 PIE World와 종료 상태가 두 프레임 연속 사라졌는지 확인한다.
 */
class FWaitForPIEIdleCommand final : public IAutomationLatentCommand
{
public:
	explicit FWaitForPIEIdleCommand(FAutomationTestBase* InTest)
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

		const bool bIsIdle = GEditor
			&& !GEditor->IsPlaySessionInProgress()
			&& FindFrontEndPIEWorld() == nullptr
			&& FindMissionPIEWorld() == nullptr;
		if (bIsIdle)
		{
			++ConsecutiveIdleFrames;
			return ConsecutiveIdleFrames >= 2;
		}

		ConsecutiveIdleFrames = 0;
		if (Now - StartedAt > 20.0)
		{
			Test->AddError(TEXT("PIE did not become fully idle before the next FLOW-08 run"));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
	int32 ConsecutiveIdleFrames = 0;
};
} // namespace DroneMissionEntryPIE

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMissionEntryPIETest,
	"Drone.Flow.MissionEntryPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneMissionEntryPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneMissionEntryPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindFrontEndPIEWorld() || FindMissionPIEWorld())
	{
		AddError(TEXT("Mission entry PIE test requires an idle Editor"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(FrontEndMapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != FrontEndMapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), FrontEndMapPackage));
		return false;
	}

	// FLOW-08: 완전히 새 PIE 실행에서 전체 수명주기를 세 번 반복한다.
	// 각 실행은 성공 -> 재도전 -> Drone 파괴 실패 -> 로비 복귀까지 검증하므로,
	// 이전 실행의 Widget/Director/Drone/Delegate가 다음 실행에 남으면 테스트가 실패한다.
	for (int32 RunIndex = 0; RunIndex < 3; ++RunIndex)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FLazyStartPIECommand(MakePlayParams()));
		ADD_LATENT_AUTOMATION_COMMAND(FTriggerMissionTravelCommand(this));
		ADD_LATENT_AUTOMATION_COMMAND(FValidateMissionSelectionAndSpawnCommand(this));
		ADD_LATENT_AUTOMATION_COMMAND(FValidateRetryFailureAndLobbyRequestCommand(this));
		ADD_LATENT_AUTOMATION_COMMAND(FValidateLobbyReturnCommand(this));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEIdleCommand(this));
	}
	return true;
}

#endif
