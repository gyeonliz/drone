#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Mission/DroneMissionDefinition.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFrontEndRootWidget.h"

/**
 * MISSION-BRIEFING-02: Story 미션 브리핑에서 Figma 허브 대사가 자막으로 차례대로 나오는지 확인한다.
 * - M1: 첫 대사·말하는 사람, 시간에 따른 자동 진행, Y·Tab 건너뛰기, 마지막 대사에서 멈춤, 뒤로 가면 정지
 * - M3: "오마르는 처리됐다"(Story.TargetEliminated 조건)는 새 세션(Fact 없음)에서 빠진다
 */
namespace DroneBriefingLinesPIE
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneFrontEnd");
const FName GoldenTimeId(TEXT("Mission.Story.GoldenTime.Test"));
const FName VeilBreakerId(TEXT("Mission.Story.VeilBreaker.Test"));

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

FString TextOf(UDroneFrontEndRootWidget* Widget, const TCHAR* Name)
{
	const UTextBlock* Text = Widget && Widget->WidgetTree ? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(FName(Name))) : nullptr;
	return Text ? Text->GetText().ToString() : FString();
}

void PressKey(const FKey& Key)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	Slate.ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
	Slate.ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
}

class FValidateBriefingLinesCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateBriefingLinesCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0) StartedAt = Now;
		UWorld* World = FindPIEWorld();
		ADroneFrontEndPlayerController* Controller = World ? World->GetFirstPlayerController<ADroneFrontEndPlayerController>() : nullptr;
		UDroneFrontEndRootWidget* Widget = Controller ? Controller->GetFrontEndWidget() : nullptr;
		UDroneGameFlowSubsystem* Flow = Widget ? Widget->GetFlowSubsystem() : nullptr;
		if (!World || !World->HasBegunPlay() || !Widget || !Flow)
		{
			return Fail(Now, TEXT("Front-end did not become ready"));
		}
		const UDroneMissionDefinition* GoldenTime = Flow->FindMissionDefinition(GoldenTimeId);

		switch (Phase)
		{
		case 0: // 시작 → Story 로비 → M1 선택 → 브리핑
			if (!GoldenTime || GoldenTime->BriefingLines.Num() != 4)
			{
				Test->AddError(TEXT("M1 Definition must hold the four Figma HUB lines"));
				return true;
			}
			Test->TestTrue(TEXT("Start opens the Story lobby"), Widget->FinishOpeningTrailer());
			Test->TestTrue(TEXT("M1 can be selected"), Widget->SelectLobbyMission(GoldenTimeId));
			Test->TestTrue(TEXT("M1 opens its briefing"), Widget->ConfirmSelectedMission());
			return Next(Now);
		case 1: // 첫 대사·말하는 사람
			if (Widget->GetBriefingLineIndex() != 0) return Fail(Now, TEXT("Briefing did not start its first line"));
			Test->TestEqual(TEXT("Subtitle speaker is HUB"), TextOf(Widget, TEXT("MissionBriefingSpeakerText")), GoldenTime->BriefingLines[0].Speaker.ToString());
			Test->TestEqual(TEXT("Subtitle shows the first Figma line"), TextOf(Widget, TEXT("MissionBriefingLineText")), GoldenTime->BriefingLines[0].Text.ToString());
			Test->TestTrue(TEXT("Subtitle panel is shown"), Widget->WidgetTree->FindWidget(TEXT("MissionBriefingSubtitlePanel"))
				&& Widget->WidgetTree->FindWidget(TEXT("MissionBriefingSubtitlePanel"))->GetVisibility() != ESlateVisibility::Collapsed);
			FirstLineShownAt = Now;
			return Next(Now);
		case 2: // 시간이 지나면 자동으로 두 번째 대사
			if (Widget->GetBriefingLineIndex() != 1)
			{
				return Now - FirstLineShownAt > Widget->BriefingMaxLineSeconds + 2.0 ? Fail(Now, TEXT("Briefing did not auto-advance"), true) : false;
			}
			Test->TestTrue(FString::Printf(TEXT("Auto-advance waits the reading time (%.1fs)"), Now - FirstLineShownAt),
				Now - FirstLineShownAt >= Widget->BriefingMinLineSeconds - 0.2);
			PressKey(EKeys::Gamepad_FaceButton_Top);
			return Next(Now);
		case 3: // Y → 세 번째, Tab → 네 번째, 마지막에서 멈춤
			if (Widget->GetBriefingLineIndex() != 2) return Fail(Now, TEXT("Pad Y did not skip to the third line"));
			PressKey(EKeys::Tab);
			return Next(Now);
		case 4:
			if (Widget->GetBriefingLineIndex() != 3) return Fail(Now, TEXT("Tab did not skip to the fourth line"));
			Test->TestFalse(TEXT("Skipping past the last line is refused"), Widget->SkipBriefingLine());
			Test->TestEqual(TEXT("The last line stays on screen"), TextOf(Widget, TEXT("MissionBriefingLineText")), GoldenTime->BriefingLines[3].Text.ToString());
			Test->TestTrue(TEXT("Back leaves the briefing"), Widget->NavigateBack());
			return Next(Now);
		case 5: // 나가면 정지 → M3 브리핑: 조건 대사 제외
			if (Widget->GetDisplayedState() != EDroneGameFlowState::LobbyMissionSelect) return Fail(Now, TEXT("Back did not return to the lobby"));
			Test->TestEqual(TEXT("Leaving the briefing stops the subtitles"), Widget->GetBriefingLineIndex(), static_cast<int32>(INDEX_NONE));
			Test->TestFalse(TEXT("New session has no Story.TargetEliminated fact"), Flow->HasStoryFact(TEXT("Story.TargetEliminated")));
			Test->TestTrue(TEXT("M3 can be selected"), Widget->SelectLobbyMission(VeilBreakerId));
			Test->TestTrue(TEXT("M3 opens its briefing"), Widget->ConfirmSelectedMission());
			return Next(Now);
		case 6:
		{
			const UDroneMissionDefinition* VeilBreaker = Flow->FindMissionDefinition(VeilBreakerId);
			if (Widget->GetBriefingLineIndex() != 0 || !VeilBreaker || VeilBreaker->BriefingLines.Num() != 5)
			{
				return Fail(Now, TEXT("M3 briefing did not start with its five-line Definition"));
			}
			Test->TestEqual(TEXT("M3 skips the 'Omar eliminated' line without the fact"),
				TextOf(Widget, TEXT("MissionBriefingLineText")), VeilBreaker->BriefingLines[1].Text.ToString());
			int32 Shown = 1;
			while (Widget->SkipBriefingLine()) ++Shown;
			Test->TestEqual(TEXT("M3 shows four of five lines in this branch"), Shown, 4);
			return true;
		}
		default:
			return true;
		}
	}

private:
	bool Next(const double Now) { ++Phase; PhaseStartedAt = Now; return false; }
	bool Fail(const double Now, const TCHAR* Message, const bool bImmediate = false)
	{
		if (bImmediate || Now - (PhaseStartedAt == 0.0 ? StartedAt : PhaseStartedAt) > 20.0)
		{
			Test->AddError(FString::Printf(TEXT("[MISSION-BRIEFING] phase %d: %s"), Phase, Message));
			return true;
		}
		return false;
	}

	FAutomationTestBase* Test;
	int32 Phase = 0;
	double StartedAt = 0.0;
	double PhaseStartedAt = 0.0;
	double FirstLineShownAt = 0.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBriefingLinesPIETest,
	"Drone.Flow.BriefingLinesPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneBriefingLinesPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneBriefingLinesPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Briefing lines PIE requires an idle Editor"));
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateBriefingLinesCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
