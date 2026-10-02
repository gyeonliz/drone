#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneMissionDefinition.h"
#include "Misc/App.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFrontEndRootWidget.h"
#include "UI/DroneMissionResultWidget.h"
#include "UI/DroneSelectionWidget.h"

/**
 * UI-PAD-01: 패드 키만으로 미션 한 바퀴를 돈다.
 * 타이틀 → 훈련 → Hover 수업 고르기 → [출격](→) → 브리핑 → 미션 맵 기체 선택(카드 이동·선택·↑[출격]) → 비행 시작
 * → (기체 파괴로 실패 유도) → 결과 화면 첫 포커스 [다시 하기] → [로비로] → 로비 복귀 시 Hover 수업에 포커스.
 * 렌더링이 필요하다(-RenderOffScreen). NullRHI에서는 판정하지 않는다.
 */
namespace DroneGamepadMissionFlowPIE
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneFrontEnd");
const FName HoverMissionId(TEXT("Mission.Tutorial.Hover"));
constexpr double StepTimeoutSeconds = 8.0;   // 맵 이동이 들어가는 단계가 있다.
constexpr double RepeatIntervalSeconds = 0.35;

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

void PressPadKey(const FKey& Key)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	Slate.ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
	Slate.ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
}

/** 이번 프레임의 PIE 월드에서 찾은 대상. 맵 이동 뒤에는 다른 컨트롤러·위젯이 된다. */
struct FContext
{
	UWorld* World = nullptr;
	APlayerController* Player = nullptr;
	UDroneGameFlowSubsystem* Flow = nullptr;
	UDroneFrontEndRootWidget* FrontEnd = nullptr;
	ADroneMissionPlayerController* Mission = nullptr;
	UDroneSelectionWidget* Selection = nullptr;
	UDroneMissionResultWidget* Result = nullptr;

	EDroneGameFlowState State() const { return Flow ? Flow->GetSnapshot().State : EDroneGameFlowState::Boot; }
	bool Focused(const UWidget* Widget) const { return Widget && Player && Widget->HasUserFocus(Player); }
	int32 FocusedMissionIndex() const
	{
		for (int32 Index = 0; FrontEnd && Index < FrontEnd->GetNativeMissionButtonCount(); ++Index)
		{
			if (Focused(FrontEnd->GetNativeMissionButton(Index))) return Index;
		}
		return INDEX_NONE;
	}
	int32 FocusedDroneIndex() const
	{
		for (int32 Index = 0; Selection && Index < Selection->GetDroneButtonCount(); ++Index)
		{
			if (Focused(Selection->GetDroneButton(Index))) return Index;
		}
		return INDEX_NONE;
	}
	UWidget* Named(UUserWidget* Owner, const TCHAR* Name) const
	{
		return Owner && Owner->WidgetTree ? Owner->WidgetTree->FindWidget(FName(Name)) : nullptr;
	}
};

FContext ReadContext()
{
	FContext Context;
	Context.World = FindPIEWorld();
	if (!Context.World || !Context.World->HasBegunPlay()) return Context;
	Context.Player = Context.World->GetFirstPlayerController();
	Context.Flow = Context.World->GetGameInstance() ? Context.World->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>() : nullptr;
	if (ADroneFrontEndPlayerController* FrontEndController = Cast<ADroneFrontEndPlayerController>(Context.Player))
	{
		Context.FrontEnd = FrontEndController->GetFrontEndWidget();
	}
	if (ADroneMissionPlayerController* MissionController = Cast<ADroneMissionPlayerController>(Context.Player))
	{
		Context.Mission = MissionController;
		Context.Selection = MissionController->GetDroneSelectionWidget();
		Context.Result = MissionController->GetMissionResultWidget();
	}
	return Context;
}

/**
 * Hover DA는 실패 시 재출격(MISSION-CHECKPOINT-01)이라 기체를 부숴도 결과 화면이 뜨지 않는다.
 * 이 테스트는 결과 화면 패드 이동을 보므로 파괴 직전에 메모리에서만 ShowResult로 바꾸고, 명령이 끝나면 되돌린다(저장하지 않음).
 */
TWeakObjectPtr<UDroneMissionDefinition> EditedFailureMission;
EDroneMissionFailureResponse OriginalFailureResponse = EDroneMissionFailureResponse::ShowResult;

void RestoreFailureResponse()
{
	if (UDroneMissionDefinition* Mission = EditedFailureMission.Get())
	{
		Mission->FailureResponse = OriginalFailureResponse;
	}
	EditedFailureMission.Reset();
}

struct FStep
{
	FString Name;
	/** 단계 시작 때(그리고 Repeat이면 확인 실패 때마다) 누를 키. Invalid면 누르지 않는다. */
	TFunction<FKey(const FContext&)> Key;
	TFunction<bool(const FContext&)> Check;
	bool bRepeatKey = false;
	/** 키 대신 한 번 실행할 게임 쪽 동작(예: 실패 유도). */
	TFunction<void(const FContext&)> Action;
};

FStep PadStep(const TCHAR* Name, const FKey Key, TFunction<bool(const FContext&)> Check)
{
	return FStep{Name, [Key](const FContext&) { return Key; }, MoveTemp(Check)};
}

class FValidateGamepadMissionFlowCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateGamepadMissionFlowCommand(FAutomationTestBase* InTest) : Test(InTest)
	{
		Steps.Add(PadStep(TEXT("Title opens with focus on [시작]"), EKeys::Invalid, [](const FContext& C)
			{ return C.FrontEnd && C.State() == EDroneGameFlowState::OpeningTrailer && C.Focused(C.FrontEnd->GetTitleButton(0)); }));
		Steps.Add(PadStep(TEXT("Down → [훈련]"), EKeys::Gamepad_DPad_Down, [](const FContext& C)
			{ return C.FrontEnd && C.Focused(C.FrontEnd->GetTitleButton(1)); }));
		Steps.Add(PadStep(TEXT("A → Tutorial lobby"), EKeys::Gamepad_FaceButton_Bottom, [](const FContext& C)
			{ return C.FrontEnd && C.State() == EDroneGameFlowState::LobbyMissionSelect && C.FocusedMissionIndex() == 0; }));
		// Hover 수업 위치는 로비 정렬에 따른다(TUT-PROGRESS-01 이후 수업 순서라 맨 위). 이미 Hover면 누르지 않고,
		// 아니면 포커스가 Hover에 닿을 때까지 ↓ 를 누른다.
		Steps.Add(FStep{TEXT("Down until the Hover lesson is focused"),
			[](const FContext& C)
			{
				const TArray<FName> Ids = C.FrontEnd ? C.FrontEnd->GetVisibleMissionIds() : TArray<FName>();
				const int32 Focused = C.FocusedMissionIndex();
				return Ids.IsValidIndex(Focused) && Ids[Focused] == HoverMissionId ? EKeys::Invalid : EKeys::Gamepad_DPad_Down;
			},
			[](const FContext& C)
			{
				const TArray<FName> Ids = C.FrontEnd ? C.FrontEnd->GetVisibleMissionIds() : TArray<FName>();
				const int32 Focused = C.FocusedMissionIndex();
				return Ids.IsValidIndex(Focused) && Ids[Focused] == HoverMissionId;
			}, /*bRepeatKey*/ true});
		Steps.Add(PadStep(TEXT("A selects Hover"), EKeys::Gamepad_FaceButton_Bottom, [](const FContext& C)
			{ return C.Flow && C.Flow->GetSnapshot().SelectedMissionId == HoverMissionId; }));
		Steps.Add(PadStep(TEXT("Right → [출격] (explicit list→Start navigation)"), EKeys::Gamepad_DPad_Right, [](const FContext& C)
			{ return C.FrontEnd && C.Focused(C.Named(C.FrontEnd, TEXT("StartMissionButton"))); }));
		Steps.Add(PadStep(TEXT("A → Briefing with focus on the launch button"), EKeys::Gamepad_FaceButton_Bottom, [](const FContext& C)
			{ return C.FrontEnd && C.State() == EDroneGameFlowState::MissionTrailer
				&& C.Focused(C.Named(C.FrontEnd, TEXT("FinishMissionBriefingButton"))); }));
		Steps.Add(PadStep(TEXT("A → Hover map, Drone selection focused on a Drone card"), EKeys::Gamepad_FaceButton_Bottom, [](const FContext& C)
			{ return C.Selection && C.State() == EDroneGameFlowState::DroneSelect && C.FocusedDroneIndex() != INDEX_NONE; }));
		// [출격]은 기체를 고르기 전까지 비활성이라 포커스가 갈 수 없다. 실제 흐름처럼 카드에서 A로 먼저 고른다.
		Steps.Add(PadStep(TEXT("A selects the focused Drone card"), EKeys::Gamepad_FaceButton_Bottom, [](const FContext& C)
			{
				const int32 Index = C.FocusedDroneIndex();
				return Index != INDEX_NONE && C.Flow && !C.Flow->GetSnapshot().SelectedDroneId.IsNone()
					&& C.Selection->GetDisplayedDroneId(Index) == C.Flow->GetSnapshot().SelectedDroneId;
			}));
		Steps.Add(PadStep(TEXT("Up → [출격] on the selection screen"), EKeys::Gamepad_DPad_Up, [](const FContext& C)
			{ return C.Selection && C.Focused(C.Named(C.Selection, TEXT("LaunchDroneButton"))); }));
		Steps.Add(PadStep(TEXT("Down → back to the selected Drone card"), EKeys::Gamepad_DPad_Down, [](const FContext& C)
			{
				const int32 Index = C.FocusedDroneIndex();
				return Index != INDEX_NONE && C.Flow && C.Selection->GetDisplayedDroneId(Index) == C.Flow->GetSnapshot().SelectedDroneId;
			}));
		Steps.Add(PadStep(TEXT("Up, A → launch and fly"), EKeys::Gamepad_DPad_Up, [](const FContext& C)
			{ return C.Selection && C.Focused(C.Named(C.Selection, TEXT("LaunchDroneButton"))); }));
		Steps.Add(PadStep(TEXT("A on [출격] → in mission with a possessed Drone"), EKeys::Gamepad_FaceButton_Bottom, [](const FContext& C)
			{ return C.Mission && C.State() == EDroneGameFlowState::InMission && C.Mission->GetSpawnedDrone()
				&& C.Mission->GetPawn() == C.Mission->GetSpawnedDrone(); }));
		Steps.Add(FStep{TEXT("Drone destroyed → Failure result with focus on [다시 하기]"),
			[](const FContext&) { return EKeys::Invalid; },
			[](const FContext& C)
			{ return C.Result && C.State() == EDroneGameFlowState::MissionResult && C.Focused(C.Named(C.Result, TEXT("RetryMissionButton"))); },
			false,
			[](const FContext& C)
			{
				UDroneMissionDefinition* Mission = C.Flow ? C.Flow->FindMissionDefinition(C.Flow->GetSnapshot().SelectedMissionId) : nullptr;
				if (Mission && !EditedFailureMission.IsValid())
				{
					EditedFailureMission = Mission;
					OriginalFailureResponse = Mission->FailureResponse;
					Mission->FailureResponse = EDroneMissionFailureResponse::ShowResult;
				}
				if (ADronePrototypePawn* Drone = C.Mission ? C.Mission->GetSpawnedDrone() : nullptr)
				{
					Drone->GetHealthComponent()->ApplyHealthDamage(100000.0f, C.Mission, Drone);
				}
			}});
		// 결과 화면은 [다시 하기] 아래에 [로비로]가 있는 세로 배치다.
		Steps.Add(PadStep(TEXT("Down → [로비로]"), EKeys::Gamepad_DPad_Down, [](const FContext& C)
			{ return C.Result && C.Focused(C.Named(C.Result, TEXT("ReturnToLobbyButton"))); }));
		Steps.Add(PadStep(TEXT("A → back in the Tutorial lobby with focus on the Hover lesson"), EKeys::Gamepad_FaceButton_Bottom, [](const FContext& C)
			{
				const TArray<FName> Ids = C.FrontEnd ? C.FrontEnd->GetVisibleMissionIds() : TArray<FName>();
				const int32 Focused = C.FocusedMissionIndex();
				return C.State() == EDroneGameFlowState::LobbyMissionSelect && Ids.IsValidIndex(Focused) && Ids[Focused] == HoverMissionId;
			}));
	}

	virtual ~FValidateGamepadMissionFlowCommand() override { RestoreFailureResponse(); }

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0) StartedAt = Now;
		const FContext C = ReadContext();
		if (!C.World || !C.Player || !C.Flow)
		{
			if (Now - StartedAt > 30.0)
			{
				Test->AddError(TEXT("PIE did not become ready for the gamepad mission flow"));
				return true;
			}
			return false;
		}
		if (StepIndex >= Steps.Num()) return true;

		FStep& Step = Steps[StepIndex];
		if (!bStepStarted)
		{
			bStepStarted = true;
			StepStartedAt = LastKeyAt = Now;
			if (Step.Action) Step.Action(C);
			const FKey Key = Step.Key ? Step.Key(C) : EKeys::Invalid;
			if (Key.IsValid()) PressPadKey(Key);
			return false;
		}
		if (Step.Check(C))
		{
			Test->AddInfo(FString::Printf(TEXT("[UI-PAD] OK: %s"), *Step.Name));
			++StepIndex;
			bStepStarted = false;
			return StepIndex >= Steps.Num();
		}
		if (Step.bRepeatKey && Now - LastKeyAt > RepeatIntervalSeconds)
		{
			LastKeyAt = Now;
			const FKey Key = Step.Key ? Step.Key(C) : EKeys::Invalid;
			if (Key.IsValid()) PressPadKey(Key);
		}
		if (Now - StepStartedAt > StepTimeoutSeconds)
		{
			const TSharedPtr<SWidget> SlateFocus = FSlateApplication::Get().GetUserFocusedWidget(0);
			UWidget* Highlighted = C.Selection ? C.Selection->GetGamepadHighlightedWidget()
				: C.Result ? C.Result->GetGamepadHighlightedWidget()
				: C.FrontEnd ? C.FrontEnd->GetGamepadHighlightedWidget() : nullptr;
			Test->AddError(FString::Printf(TEXT("[UI-PAD] FAIL: %s (state=%d, player=%s, Slate focus=%s, highlighted=%s, droneCard=%d)"),
				*Step.Name, static_cast<int32>(C.State()), *GetNameSafe(C.Player),
				SlateFocus.IsValid() ? *SlateFocus->ToString() : TEXT("(none)"), *GetNameSafe(Highlighted), C.FocusedDroneIndex()));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test;
	TArray<FStep> Steps;
	double StartedAt = 0.0;
	double StepStartedAt = 0.0;
	double LastKeyAt = 0.0;
	int32 StepIndex = 0;
	bool bStepStarted = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneGamepadMissionFlowPIETest,
	"Drone.Flow.GamepadMissionFlowPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneGamepadMissionFlowPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneGamepadMissionFlowPIE;
	if (!FApp::CanEverRender())
	{
		AddWarning(TEXT("UI-PAD-01 mission flow needs rendered widget layout; run with -RenderOffScreen. Skipped under NullRHI."));
		return true;
	}
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Gamepad mission flow PIE test requires an idle Editor"));
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
	ADD_LATENT_AUTOMATION_COMMAND(FValidateGamepadMissionFlowCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
