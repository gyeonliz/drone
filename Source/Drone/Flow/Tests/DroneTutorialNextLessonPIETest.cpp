#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionPlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Mission/DroneMissionDefinition.h"
#include "Mission/DroneMissionDirector.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFrontEndRootWidget.h"
#include "UI/DroneMissionResultWidget.h"
#include "UI/DroneSelectionWidget.h"

/**
 * TUT-PROGRESS-01: 호버 수업 맵에서 출격 → 목표 완료 → 결과 화면에
 * 클리어 시간·"수업 1/8"·[다음 수업: 전진]이 나오고, [다음 수업]이 FrontEnd의 전진 수업 브리핑을 여는지 확인한다.
 * 패드 포커스 강조는 위젯이 그려질 때만 확인된다(-RenderOffScreen 실행에서 확인, NullRHI에서는 건너뜀).
 *
 * Drone.Flow.TutorialCompletePIE: 나머지 7개 수업을 테스트용으로 완료 처리한 뒤 호버를 클리어해
 * Figma S49 전체 완료 화면("훈련 완료", 안내 문구, [미션 진행] [시작 메뉴], [다시하기] 숨김)과
 * [미션 진행] → 로비 미션 탭의 첫 미션(M1)을 확인한다.
 */
namespace DroneTutorialNextLessonPIE
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/TestMap/Tutorial/Lvl_Tutorial_Hover_Test");
const FName ScoutId(TEXT("Drone.Scout.Greybox"));
const FName HoverId(TEXT("Mission.Tutorial.Hover"));
const FName ForwardId(TEXT("Mission.Tutorial.Forward"));
const FName FirstStoryId(TEXT("Mission.Story.GoldenTime.Test"));
constexpr double StepTimeoutSeconds = 12.0;

UWorld* FindPIEWorld()
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World()) return Context.World();
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

class FValidateNextLessonCommand final : public IAutomationLatentCommand
{
public:
	FValidateNextLessonCommand(FAutomationTestBase* InTest, const bool bInAllComplete) : Test(InTest), bAllComplete(bInAllComplete) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (PhaseStartedAt == 0.0) PhaseStartedAt = Now;
		UWorld* World = FindPIEWorld();
		UDroneGameFlowSubsystem* Flow = World && World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>() : nullptr;
		if (!World || !World->HasBegunPlay() || !Flow)
		{
			return Timeout(Now, TEXT("PIE world is not ready"));
		}
		ADroneMissionPlayerController* Mission = World->GetFirstPlayerController<ADroneMissionPlayerController>();

		switch (Phase)
		{
		case 0: // Scout으로 출격
		{
			UDroneSelectionWidget* Selection = Mission ? Mission->GetDroneSelectionWidget() : nullptr;
			if (!Selection || Flow->GetSnapshot().State != EDroneGameFlowState::DroneSelect)
			{
				return Timeout(Now, TEXT("Hover map did not reach Drone selection"));
			}
			Test->TestEqual(TEXT("Hover lesson is selected"), Flow->GetSnapshot().SelectedMissionId, HoverId);
			if (bAllComplete)
			{
				for (const FName Lesson : Flow->GetMissionSequence(HoverId))
				{
					if (Lesson != HoverId) Flow->MarkMissionCompletedForTesting(Lesson);
				}
			}
			Test->TestTrue(TEXT("Scout launches"), Selection->SelectDrone(ScoutId) && Selection->ConfirmAndLaunchSelectedDrone());
			return Next(Now);
		}
		case 1: // 잠깐 비행한 뒤 모든 목표 완료
		{
			ADroneMissionDirector* Director = Mission ? Mission->GetMissionDirector() : nullptr;
			if (!Director || !Director->IsMissionActive())
			{
				return Timeout(Now, TEXT("Hover mission did not start"));
			}
			if (Now - PhaseStartedAt < 1.0)
			{
				return false;
			}
			Test->TestTrue(TEXT("Director measures elapsed time while active"), Director->GetMissionElapsedSeconds() > 0.5);
			for (int32 Guard = 0; Guard < 16 && Director->IsMissionActive(); ++Guard)
			{
				Director->CompleteCurrentObjective();
			}
			return Next(Now);
		}
		case 2: // 결과 화면
		{
			UDroneMissionResultWidget* Result = Mission ? Mission->GetMissionResultWidget() : nullptr;
			if (!Result || Flow->GetSnapshot().State != EDroneGameFlowState::MissionResult)
			{
				return Timeout(Now, TEXT("Success result did not open"));
			}
			Test->TestEqual(TEXT("Outcome is Success"), Result->GetDisplayedOutcome(), EDroneMissionOutcome::Success);
			if (bAllComplete)
			{
				return CheckAllCompleteScreen(*Result, Now);
			}
			Test->TestTrue(TEXT("Flow keeps the clear time"), Flow->GetSnapshot().LastMissionElapsedSeconds > 0.5);
			// Figma S48: 훈련 완료 / 미션 제목 / 시간 / [다음] [다시하기]
			const FString Detail = Result->GetResultDetailText().ToString();
			const UDroneMissionDefinition* Hover = Flow->FindMissionDefinition(HoverId);
			Test->TestEqual(TEXT("Lesson result title follows Figma"), Result->GetResultDisplayText().ToString(), FString(TEXT("훈련 완료")));
			Test->TestTrue(FString::Printf(TEXT("Result names the lesson (%s)"), *Detail), Hover && Detail.Contains(Hover->DisplayName.ToString()));
			Test->TestTrue(FString::Printf(TEXT("Result shows the time (%s)"), *Detail), Detail.Contains(TEXT("시간 00:0")));
			Test->TestTrue(FString::Printf(TEXT("Result shows lesson 1/8 with 1 completed (%s)"), *Detail), Detail.Contains(TEXT("수업 1/8  |  완료 1/8")));
			Test->TestTrue(TEXT("Next lesson is offered"), Result->IsNextMissionAvailable());
			Test->TestEqual(TEXT("Next button reads [다음]"), Result->GetNextMissionButtonLabel().ToString(), FString(TEXT("다음")));
			UTextBlock* RetryText = Cast<UTextBlock>(Result->WidgetTree->FindWidget(TEXT("RetryMissionButtonText")));
			Test->TestTrue(TEXT("Retry button reads [다시하기]"), RetryText && RetryText->GetText().ToString() == TEXT("다시하기"));
			Test->TestFalse(TEXT("A single lesson is not the whole Tutorial"), Result->IsSequenceComplete());
			UButton* NextButton = Cast<UButton>(Result->WidgetTree->FindWidget(TEXT("NextMissionButton")));
			Test->TestTrue(TEXT("Next button is visible"), NextButton && NextButton->IsVisible());
			// 위젯이 그려지는 실행에서만 포커스 강조가 붙는다.
			if (FApp::CanEverRender())
			{
				if (Result->GetGamepadHighlightedWidget() != NextButton)
				{
					return Timeout(Now, TEXT("Gamepad focus did not land on the Next lesson button"));
				}
				Test->TestTrue(TEXT("Gamepad focus starts on the Next lesson button"), true);
			}
			Test->TestTrue(TEXT("Next lesson request is accepted"), Result->RequestNextMission());
			return Next(Now);
		}
		case 3: // FrontEnd의 전진 수업 브리핑 / 전체 완료면 로비 미션 탭
		{
			if (bAllComplete)
			{
				return CheckMissionLobby(*World, *Flow, Now);
			}
			ADroneFrontEndPlayerController* FrontEnd = World->GetFirstPlayerController<ADroneFrontEndPlayerController>();
			UDroneFrontEndRootWidget* Root = FrontEnd ? FrontEnd->GetFrontEndWidget() : nullptr;
			if (!Root || Root->GetDisplayedState() != EDroneGameFlowState::MissionTrailer)
			{
				return Timeout(Now, TEXT("Front-end briefing for the next lesson did not open"));
			}
			Test->TestEqual(TEXT("Flow is on the next lesson briefing"), Flow->GetSnapshot().State, EDroneGameFlowState::MissionTrailer);
			Test->TestEqual(TEXT("Forward lesson is selected"), Flow->GetSnapshot().SelectedMissionId, ForwardId);
			const UDroneMissionDefinition* Forward = Flow->FindMissionDefinition(ForwardId);
			Test->TestTrue(TEXT("Briefing shows the Forward lesson"),
				Forward && Root->GetDisplayedBriefingTitle().ToString() == Forward->DisplayName.ToString());
			Test->TestTrue(TEXT("Hover stays completed"), Flow->IsMissionCompleted(HoverId));
			return true;
		}
		default:
			return true;
		}
	}

private:
	bool CheckAllCompleteScreen(UDroneMissionResultWidget& Result, const double Now)
	{
		// Figma S49: 훈련 완료 / 이제 운용 할 준비가 되었습니다. / [미션 진행] [시작 메뉴]
		const FString Detail = Result.GetResultDetailText().ToString();
		Test->TestTrue(TEXT("All 8 lessons complete"), Result.IsTutorialAllComplete() && Result.IsSequenceComplete());
		Test->TestEqual(TEXT("All-complete title follows Figma"), Result.GetResultDisplayText().ToString(), FString(TEXT("훈련 완료")));
		Test->TestTrue(FString::Printf(TEXT("All-complete message follows Figma (%s)"), *Detail), Detail.Contains(TEXT("이제 운용 할 준비가 되었습니다.")));
		Test->TestTrue(FString::Printf(TEXT("All-complete shows 8/8 (%s)"), *Detail), Detail.Contains(TEXT("수업 8/8 모두 완료")));
		Test->TestEqual(TEXT("First button reads [미션 진행]"), Result.GetNextMissionButtonLabel().ToString(), FString(TEXT("미션 진행")));
		UTextBlock* LobbyText = Cast<UTextBlock>(Result.WidgetTree->FindWidget(TEXT("ReturnToLobbyButtonText")));
		Test->TestTrue(TEXT("Second button reads [시작 메뉴]"), LobbyText && LobbyText->GetText().ToString() == TEXT("시작 메뉴"));
		UButton* Retry = Cast<UButton>(Result.WidgetTree->FindWidget(TEXT("RetryMissionButton")));
		Test->TestTrue(TEXT("[다시하기] is hidden on the all-complete screen"), Retry && !Retry->IsVisible());
		UButton* NextButton = Cast<UButton>(Result.WidgetTree->FindWidget(TEXT("NextMissionButton")));
		if (FApp::CanEverRender() && Result.GetGamepadHighlightedWidget() != NextButton)
		{
			return Timeout(Now, TEXT("Gamepad focus did not land on [미션 진행]"));
		}
		Test->TestFalse(TEXT("[다음 수업] request is refused on the all-complete screen"), Result.RequestNextMission());
		Test->TestTrue(TEXT("[미션 진행] request is accepted"), Result.RequestContinueToMissions());
		return Next(Now);
	}

	bool CheckMissionLobby(UWorld& World, UDroneGameFlowSubsystem& Flow, const double Now)
	{
		ADroneFrontEndPlayerController* FrontEnd = World.GetFirstPlayerController<ADroneFrontEndPlayerController>();
		UDroneFrontEndRootWidget* Root = FrontEnd ? FrontEnd->GetFrontEndWidget() : nullptr;
		if (!Root || Root->GetDisplayedState() != EDroneGameFlowState::LobbyMissionSelect)
		{
			return Timeout(Now, TEXT("Lobby did not open after [미션 진행]"));
		}
		Test->TestEqual(TEXT("Lobby focuses the first story mission (M1)"), Flow.GetLastLobbyMissionId(), FirstStoryId);
		const TArray<FName> Visible = Root->GetVisibleMissionIds();
		Test->TestTrue(TEXT("Lobby opens the Mission tab with M1 on top"), !Visible.IsEmpty() && Visible[0] == FirstStoryId);
		Test->TestTrue(TEXT("Tutorial completion survives the map change"), Flow.IsMissionCompleted(HoverId) && Flow.IsMissionCompleted(ForwardId));
		return true;
	}

	bool Next(const double Now)
	{
		++Phase;
		PhaseStartedAt = Now;
		return false;
	}

	bool Timeout(const double Now, const TCHAR* Message)
	{
		if (Now - PhaseStartedAt > StepTimeoutSeconds)
		{
			Test->AddError(FString::Printf(TEXT("[TUT-PROGRESS] phase %d: %s"), Phase, Message));
			return true;
		}
		return false;
	}

	FAutomationTestBase* Test;
	bool bAllComplete = false;
	int32 Phase = 0;
	double PhaseStartedAt = 0.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTutorialNextLessonPIETest,
	"Drone.Flow.TutorialNextLessonPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTutorialNextLessonPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneTutorialNextLessonPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Next lesson PIE requires an idle Editor"));
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateNextLessonCommand(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTutorialCompletePIETest,
	"Drone.Flow.TutorialCompletePIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTutorialCompletePIETest::RunTest(const FString& Parameters)
{
	using namespace DroneTutorialNextLessonPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Tutorial complete PIE requires an idle Editor"));
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateNextLessonCommand(this, true));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
