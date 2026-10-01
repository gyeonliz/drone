#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/Button.h"
#include "Components/Slider.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFrontEndRootWidget.h"
#include "UI/DroneSettingsWidget.h"

/**
 * UI-PAD-01: 마우스 없이 패드 키만으로 타이틀 → 훈련 로비 → 미션 고르기 → 탭 전환(LB/RB) → 뒤로 → 설정 열고 닫기를 검증한다.
 * 실제 패드와 같은 경로를 타도록 Slate에 키 이벤트(D-Pad·A·B·LB·RB)를 넣는다.
 * 방향 이동은 화면에 배치된 위젯 위치로 계산되므로 렌더링이 필요하다(-RenderOffScreen). NullRHI에서는 판정하지 않는다.
 */
namespace DroneGamepadNavigationPIE
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneFrontEnd");
constexpr double StepTimeoutSeconds = 3.0;

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

/** 실제 패드처럼 눌렀다 뗀다. A는 떼는 순간 버튼 Click이 실행된다. */
void PressPadKey(const FKey& Key)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	const FKeyEvent Down(Key, FModifierKeysState(), /*UserIndex*/ 0, /*bIsRepeat*/ false, 0, 0);
	const FKeyEvent Up(Key, FModifierKeysState(), /*UserIndex*/ 0, /*bIsRepeat*/ false, 0, 0);
	Slate.ProcessKeyDownEvent(Down);
	Slate.ProcessKeyUpEvent(Up);
}

FString DescribeSlateFocus()
{
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
	return Focused.IsValid() ? Focused->ToString() : TEXT("(none)");
}

class FValidateGamepadNavigationCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateGamepadNavigationCommand(FAutomationTestBase* InTest) : Test(InTest) {}

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
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Front-end did not become ready for gamepad navigation PIE"));
				return true;
			}
			return false;
		}

		// 각 단계: 아직 키를 안 눌렀으면 누르고(Action), 기대 상태가 될 때까지 기다린다(Check).
		struct FStep
		{
			const TCHAR* Name;
			FKey Key; // EKeys::Invalid면 누르지 않고 기다리기만 한다.
			TFunction<bool()> Check;
		};
		UButton* Title0 = Widget->GetTitleButton(0);
		UButton* Title1 = Widget->GetTitleButton(1);
		UButton* Title2 = Widget->GetTitleButton(2);
		const TArray<FStep> Steps = {
			{TEXT("Title opens with pad focus on [시작]"), EKeys::Invalid, [=]
				{ return Widget->GetDisplayedState() == EDroneGameFlowState::OpeningTrailer && Title0 && Title0->HasUserFocus(Controller)
					&& Widget->GetGamepadHighlightedWidget() == Title0
					&& FMath::IsNearlyEqual(Title0->GetRenderTransform().Scale.X, Widget->GamepadFocusScale); }},
			{TEXT("D-Pad Down moves focus to [훈련]"), EKeys::Gamepad_DPad_Down, [=]
				{ return Title1 && Title1->HasUserFocus(Controller) && Widget->GetGamepadHighlightedWidget() == Title1
					&& FMath::IsNearlyEqual(Title0->GetRenderTransform().Scale.X, 1.0f); }},
			{TEXT("A on [훈련] opens the Tutorial lobby with focus on the first lesson"), EKeys::Gamepad_FaceButton_Bottom, [=]
				{ return Widget->GetDisplayedState() == EDroneGameFlowState::LobbyMissionSelect && Widget->IsTrainingLobby()
					&& Widget->GetNativeMissionButton(0) && Widget->GetNativeMissionButton(0)->HasUserFocus(Controller); }},
			{TEXT("D-Pad Down moves to the second lesson"), EKeys::Gamepad_DPad_Down, [=]
				{ return Widget->GetNativeMissionButton(1) && Widget->GetNativeMissionButton(1)->HasUserFocus(Controller); }},
			{TEXT("A selects the second lesson and keeps focus on it"), EKeys::Gamepad_FaceButton_Bottom, [=]
				{ const TArray<FName> Ids = Widget->GetVisibleMissionIds();
				  return Ids.IsValidIndex(1) && Flow->GetSnapshot().SelectedMissionId == Ids[1]
					&& Widget->GetNativeMissionButton(1)->HasUserFocus(Controller); }},
			{TEXT("RB switches to the Racing tab with focus on its first mission"), EKeys::Gamepad_RightShoulder, [=]
				{ return Widget->GetDisplayedState() == EDroneGameFlowState::LobbyMissionSelect
					&& Widget->GetVisibleMissionIds().Num() >= 1 && Widget->GetNativeMissionButton(0)
					&& Widget->GetNativeMissionButton(0)->HasUserFocus(Controller)
					&& Flow->FindMissionDefinition(Widget->GetVisibleMissionIds()[0])
					&& Flow->FindMissionDefinition(Widget->GetVisibleMissionIds()[0])->GetLobbyCategory() == EDroneMissionCategory::Racing; }},
			{TEXT("LB returns to the Tutorial tab with focus back on the lesson chosen before"), EKeys::Gamepad_LeftShoulder, [=]
				{ const TArray<FName> Ids = Widget->GetVisibleMissionIds();
				  const int32 SelectedIndex = Ids.IndexOfByKey(Flow->GetSnapshot().SelectedMissionId);
				  return Ids.Num() >= 9 && SelectedIndex == 1 && Widget->GetNativeMissionButton(SelectedIndex)
					&& Widget->GetNativeMissionButton(SelectedIndex)->HasUserFocus(Controller); }},
			{TEXT("B returns to the title with focus restored on [훈련]"), EKeys::Gamepad_FaceButton_Right, [=]
				{ return Widget->GetDisplayedState() == EDroneGameFlowState::OpeningTrailer && Title1 && Title1->HasUserFocus(Controller); }},
			{TEXT("D-Pad Down moves focus to [설정]"), EKeys::Gamepad_DPad_Down, [=]
				{ return Title2 && Title2->HasUserFocus(Controller); }},
			{TEXT("A opens Settings with focus on the volume slider"), EKeys::Gamepad_FaceButton_Bottom, [=]
				{ UDroneSettingsWidget* Settings = Widget->GetSettingsWidget();
				  const TArray<UWidget*> Controls = Settings ? Settings->GetGamepadControls() : TArray<UWidget*>();
				  return Widget->IsSettingsVisible() && Controls.Num() > 0 && Controls[0]->HasUserFocus(Controller)
					&& Settings->GetGamepadHighlightedWidget() == Controls[0]; }},
			{TEXT("B closes Settings and returns focus to [설정]"), EKeys::Gamepad_FaceButton_Right, [=]
				{ return !Widget->IsSettingsVisible() && Title2 && Title2->HasUserFocus(Controller); }},
		};

		if (StepIndex >= Steps.Num())
		{
			return true;
		}
		const FStep& Step = Steps[StepIndex];
		if (!bStepKeySent)
		{
			bStepKeySent = true;
			StepStartedAt = Now;
			if (Step.Key.IsValid())
			{
				PressPadKey(Step.Key);
			}
			return false; // 키 처리 후 다음 프레임에 확인한다.
		}
		if (Step.Check())
		{
			Test->AddInfo(FString::Printf(TEXT("[UI-PAD] OK: %s"), Step.Name));
			++StepIndex;
			bStepKeySent = false;
			return StepIndex >= Steps.Num();
		}
		if (Now - StepStartedAt > StepTimeoutSeconds)
		{
			Test->AddError(FString::Printf(TEXT("[UI-PAD] FAIL: %s (state=%d, Slate focus=%s, highlighted=%s)"),
				Step.Name, static_cast<int32>(Widget->GetDisplayedState()), *DescribeSlateFocus(),
				*GetNameSafe(Widget->GetGamepadHighlightedWidget())));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test;
	double StartedAt = 0.0;
	double StepStartedAt = 0.0;
	int32 StepIndex = 0;
	bool bStepKeySent = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneGamepadNavigationPIETest,
	"Drone.Flow.GamepadNavigationPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneGamepadNavigationPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneGamepadNavigationPIE;
	if (!FApp::CanEverRender())
	{
		AddWarning(TEXT("UI-PAD-01 navigation needs rendered widget layout; run with -RenderOffScreen. Skipped under NullRHI."));
		return true;
	}
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Gamepad navigation PIE test requires an idle Editor"));
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
	ADD_LATENT_AUTOMATION_COMMAND(FValidateGamepadNavigationCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
