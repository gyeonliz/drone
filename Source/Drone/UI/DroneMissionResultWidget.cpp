#include "UI/DroneMissionResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Mission/DroneMissionDefinition.h"
#include "Styling/CoreStyle.h"

namespace DroneMissionResultUI
{
const FName PanelName(TEXT("MissionResultPanel"));
const FName TitleName(TEXT("MissionResultTitleText"));
const FName RetryName(TEXT("RetryMissionButton"));
const FName LobbyName(TEXT("ReturnToLobbyButton"));
const FName DetailName(TEXT("MissionResultDetailText"));
const FName NextName(TEXT("NextMissionButton"));
const FName NextTextName(TEXT("NextMissionButtonText"));

/** 83.456초 → "01:23.46" */
FString FormatElapsed(const double Seconds)
{
	const int32 Centis = FMath::RoundToInt(FMath::Max(0.0, Seconds) * 100.0);
	return FString::Printf(TEXT("%02d:%02d.%02d"), Centis / 6000, (Centis / 100) % 60, Centis % 100);
}
}

void UDroneMissionResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildDefaultLayout();
	if (RetryMissionButton)
	{
		RetryMissionButton->OnClicked.AddUniqueDynamic(this, &UDroneMissionResultWidget::HandleRetryClicked);
	}
	if (ReturnToLobbyButton)
	{
		ReturnToLobbyButton->OnClicked.AddUniqueDynamic(this, &UDroneMissionResultWidget::HandleReturnToLobbyClicked);
	}
	if (NextMissionButton)
	{
		NextMissionButton->OnClicked.AddUniqueDynamic(this, &UDroneMissionResultWidget::HandleNextMissionClicked);
	}
	RefreshResultDisplay();
}

void UDroneMissionResultWidget::NativeDestruct()
{
	if (RetryMissionButton)
	{
		RetryMissionButton->OnClicked.RemoveDynamic(this, &UDroneMissionResultWidget::HandleRetryClicked);
	}
	if (ReturnToLobbyButton)
	{
		ReturnToLobbyButton->OnClicked.RemoveDynamic(this, &UDroneMissionResultWidget::HandleReturnToLobbyClicked);
	}
	if (NextMissionButton)
	{
		NextMissionButton->OnClicked.RemoveDynamic(this, &UDroneMissionResultWidget::HandleNextMissionClicked);
	}
	MissionController.Reset();
	Super::NativeDestruct();
}

void UDroneMissionResultWidget::ConfigureResult(
	ADroneMissionPlayerController* InMissionController,
	const EDroneMissionOutcome InOutcome)
{
	MissionController = InMissionController;
	DisplayedOutcome = InOutcome;
	RefreshResultDisplay();
}

bool UDroneMissionResultWidget::RequestRetry()
{
	ADroneMissionPlayerController* Controller = MissionController.Get();
	return Controller && Controller->RetrySelectedMission();
}

bool UDroneMissionResultWidget::RequestReturnToLobby()
{
	ADroneMissionPlayerController* Controller = MissionController.Get();
	return Controller && Controller->ReturnToFrontEndLobby();
}

bool UDroneMissionResultWidget::RequestNextMission()
{
	ADroneMissionPlayerController* Controller = MissionController.Get();
	return bNextMissionAvailable && !bTutorialAllComplete && Controller && Controller->StartNextMission();
}

bool UDroneMissionResultWidget::RequestContinueToMissions()
{
	ADroneMissionPlayerController* Controller = MissionController.Get();
	return bTutorialAllComplete && Controller && Controller->ContinueToMissionLobby();
}

bool UDroneMissionResultWidget::RequestReturnToTitle()
{
	ADroneMissionPlayerController* Controller = MissionController.Get();
	return Controller && Controller->ReturnToTitleMenu();
}

void UDroneMissionResultWidget::HandleNextMissionClicked()
{
	// 전체 완료 화면에서는 같은 버튼이 [미션 진행]이다.
	if (bTutorialAllComplete)
	{
		RequestContinueToMissions();
		return;
	}
	RequestNextMission();
}

void UDroneMissionResultWidget::SetButtonLabel(UTextBlock* Label, const FText& Text) const
{
	if (Label && !Text.IsEmpty())
	{
		Label->SetText(Text);
	}
}

UDroneGameFlowSubsystem* UDroneMissionResultWidget::GetFlowSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UDroneGameFlowSubsystem>() : nullptr;
}

TArray<UWidget*> UDroneMissionResultWidget::GetOrderedButtons() const
{
	// 화면 위→아래 순서. 숨긴 버튼([다음]이 없거나 전체 완료 화면의 [다시하기])은 뺀다.
	TArray<UWidget*> Buttons;
	if (NextMissionButton && bNextMissionAvailable) Buttons.Add(NextMissionButton);
	if (RetryMissionButton && !bTutorialAllComplete) Buttons.Add(RetryMissionButton);
	if (ReturnToLobbyButton) Buttons.Add(ReturnToLobbyButton);
	return Buttons;
}

void UDroneMissionResultWidget::RefreshProgressionDisplay()
{
	bNextMissionAvailable = false;
	bSequenceComplete = false;
	bTutorialAllComplete = false;
	NextMissionButtonLabel = FText::GetEmpty();
	if (DefaultRetryButtonLabel.IsEmpty() && RetryMissionButtonText) DefaultRetryButtonLabel = RetryMissionButtonText->GetText();
	if (DefaultLobbyButtonLabel.IsEmpty() && ReturnToLobbyButtonText) DefaultLobbyButtonLabel = ReturnToLobbyButtonText->GetText();
	TArray<FString> Lines;
	bool bTutorial = false;
	const UDroneGameFlowSubsystem* Flow = GetFlowSubsystem();
	if (Flow && DisplayedOutcome != EDroneMissionOutcome::None)
	{
		const bool bSuccess = DisplayedOutcome == EDroneMissionOutcome::Success;
		const FDroneGameFlowSnapshot& Snapshot = Flow->GetSnapshot();
		const UDroneMissionDefinition* Mission = Flow->FindMissionDefinition(Snapshot.SelectedMissionId);
		bTutorial = Mission && Mission->GetLobbyCategory() == EDroneMissionCategory::Tutorial;
		const TCHAR* StepWord = bTutorial ? TEXT("수업") : TEXT("미션");
		int32 Number = 0;
		int32 Count = 0;
		int32 Completed = 0;
		const bool bInSequence = Flow->GetMissionSequencePosition(Snapshot.SelectedMissionId, Number, Count, Completed);
		bSequenceComplete = bSuccess && bInSequence && Completed >= Count;
		bTutorialAllComplete = bSequenceComplete && bTutorial;

		if (bTutorialAllComplete)
		{
			Lines.Add(TutorialAllCompleteMessage.ToString());
		}
		else if (bTutorial && bSuccess && Mission)
		{
			// Figma S48: 미션 제목 줄
			Lines.Add(Mission->DisplayName.ToString());
		}
		if (Snapshot.LastMissionElapsedSeconds >= 0.0)
		{
			// 튜토리얼은 Figma S48 표기 "시간", 미션은 "클리어 시간"/"진행 시간".
			const TCHAR* TimeWord = !bSuccess ? TEXT("진행 시간") : bTutorial ? TEXT("시간") : TEXT("클리어 시간");
			Lines.Add(FString::Printf(TEXT("%s %s"), TimeWord, *DroneMissionResultUI::FormatElapsed(Snapshot.LastMissionElapsedSeconds)));
		}
		if (bInSequence)
		{
			Lines.Add(bSequenceComplete
				? FString::Printf(TEXT("%s %d/%d 모두 완료"), StepWord, Completed, Count)
				: FString::Printf(TEXT("%s %d/%d  |  완료 %d/%d"), StepWord, Number, Count, Completed, Count));
		}

		const UDroneMissionDefinition* NextMission = Flow->FindMissionDefinition(Flow->GetNextMissionId());
		if (bTutorialAllComplete)
		{
			// [미션 진행]: 미션 탭에 고를 미션이 있을 때만.
			bNextMissionAvailable = MissionController.IsValid()
				&& !Flow->GetMissionIdsInLobbyOrder(EDroneMissionCategory::Mission).IsEmpty();
			NextMissionButtonLabel = ContinueToMissionsButtonLabel;
		}
		else
		{
			bNextMissionAvailable = bSuccess && NextMission && MissionController.IsValid();
			if (NextMission)
			{
				NextMissionButtonLabel = bTutorial
					? NextLessonButtonLabel
					: FText::Format(FText::FromString(TEXT("다음 미션: {0}")), NextMission->DisplayName);
			}
			else if (bSuccess && bInSequence && !bSequenceComplete)
			{
				// 마지막 수업은 끝냈지만 앞에서 안 한 수업이 있다(로비에서 골라 들어간 경우).
				Lines.Add(FString::Printf(TEXT("남은 %s은 로비에서 고를 수 있습니다"), StepWord));
			}
		}
	}
	SetButtonLabel(RetryMissionButtonText, bTutorial ? TutorialRetryButtonLabel : DefaultRetryButtonLabel);
	SetButtonLabel(ReturnToLobbyButtonText, bTutorialAllComplete ? TitleMenuButtonLabel : DefaultLobbyButtonLabel);
	if (RetryMissionButton)
	{
		RetryMissionButton->SetVisibility(bTutorialAllComplete ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	ResultDetailText = FText::FromString(FString::Join(Lines, TEXT("\n")));
	if (MissionResultDetailText)
	{
		MissionResultDetailText->SetText(ResultDetailText);
		MissionResultDetailText->SetVisibility(Lines.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (NextMissionButtonText)
	{
		NextMissionButtonText->SetText(NextMissionButtonLabel);
	}
	if (NextMissionButton)
	{
		NextMissionButton->SetVisibility(bNextMissionAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		NextMissionButton->SetIsEnabled(bNextMissionAvailable);
	}
	if (bTutorial && DisplayedOutcome == EDroneMissionOutcome::Success && !TrainingCompleteTitle.IsEmpty())
	{
		// Figma S48·S49 제목
		ResultDisplayText = TrainingCompleteTitle;
		if (MissionResultTitleText)
		{
			MissionResultTitleText->SetText(ResultDisplayText);
		}
	}
}

void UDroneMissionResultWidget::HandleRetryClicked()
{
	RequestRetry();
}

void UDroneMissionResultWidget::HandleReturnToLobbyClicked()
{
	// 전체 완료 화면에서는 같은 버튼이 [시작 메뉴]다.
	if (bTutorialAllComplete)
	{
		RequestReturnToTitle();
		return;
	}
	RequestReturnToLobby();
}

bool UDroneMissionResultWidget::TryBindBlueprintLayout()
{
	if (!WidgetTree)
	{
		return false;
	}
	MissionResultPanel = WidgetTree->FindWidget(DroneMissionResultUI::PanelName);
	MissionResultTitleText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneMissionResultUI::TitleName));
	RetryMissionButton = Cast<UButton>(WidgetTree->FindWidget(DroneMissionResultUI::RetryName));
	ReturnToLobbyButton = Cast<UButton>(WidgetTree->FindWidget(DroneMissionResultUI::LobbyName));
	MissionResultDetailText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneMissionResultUI::DetailName));
	NextMissionButton = Cast<UButton>(WidgetTree->FindWidget(DroneMissionResultUI::NextName));
	NextMissionButtonText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneMissionResultUI::NextTextName));
	RetryMissionButtonText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("RetryMissionButtonText")));
	ReturnToLobbyButtonText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ReturnToLobbyButtonText")));
	return MissionResultPanel && MissionResultTitleText && RetryMissionButton && ReturnToLobbyButton;
}

void UDroneMissionResultWidget::BuildDefaultLayout()
{
	if (!WidgetTree || TryBindBlueprintLayout())
	{
		return;
	}
	bUsingNativeFallbackLayout = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResultRoot"));
	WidgetTree->RootWidget = Root;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), DroneMissionResultUI::PanelName);
	Panel->SetBrushColor(FLinearColor(0.006f, 0.015f, 0.021f, 0.96f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.25f, 0.25f, 0.75f, 0.75f));
	PanelSlot->SetOffsets(FMargin(0.0f));
	MissionResultPanel = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultColumn"));
	Panel->SetContent(Column);
	MissionResultTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), DroneMissionResultUI::TitleName);
	MissionResultTitleText->SetJustification(ETextJustify::Center);
	MissionResultTitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 34.0f));
	Column->AddChildToVerticalBox(MissionResultTitleText);

	MissionResultDetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), DroneMissionResultUI::DetailName);
	MissionResultDetailText->SetJustification(ETextJustify::Center);
	MissionResultDetailText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f));
	MissionResultDetailText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.9f, 0.92f, 1.0f)));
	Column->AddChildToVerticalBox(MissionResultDetailText);

	NextMissionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneMissionResultUI::NextName);
	NextMissionButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), DroneMissionResultUI::NextTextName);
	NextMissionButton->AddChild(NextMissionButtonText);
	Column->AddChildToVerticalBox(NextMissionButton);

	RetryMissionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneMissionResultUI::RetryName);
	UTextBlock* RetryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RetryMissionButtonText"));
	RetryText->SetText(FText::FromString(TEXT("같은 미션 재도전")));
	RetryMissionButton->AddChild(RetryText);
	RetryMissionButtonText = RetryText;
	Column->AddChildToVerticalBox(RetryMissionButton);
	ReturnToLobbyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneMissionResultUI::LobbyName);
	UTextBlock* LobbyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReturnToLobbyButtonText"));
	LobbyText->SetText(FText::FromString(TEXT("작전 로비로 복귀")));
	ReturnToLobbyButton->AddChild(LobbyText);
	ReturnToLobbyButtonText = LobbyText;
	Column->AddChildToVerticalBox(ReturnToLobbyButton);
}

void UDroneMissionResultWidget::RefreshResultDisplay()
{
	switch (DisplayedOutcome)
	{
	case EDroneMissionOutcome::Success:
		ResultDisplayText = FText::FromString(TEXT("미션 성공"));
		break;
	case EDroneMissionOutcome::Failure:
		ResultDisplayText = FText::FromString(TEXT("미션 실패"));
		break;
	case EDroneMissionOutcome::None:
	default:
		ResultDisplayText = FText::FromString(TEXT("미션 결과 대기"));
		break;
	}
	if (MissionResultTitleText)
	{
		MissionResultTitleText->SetText(ResultDisplayText);
		MissionResultTitleText->SetColorAndOpacity(FSlateColor(
			DisplayedOutcome == EDroneMissionOutcome::Success
				? FLinearColor(0.20f, 0.95f, 0.55f, 1.0f)
				: FLinearColor(0.95f, 0.30f, 0.25f, 1.0f)));
	}
	RefreshProgressionDisplay();
	if (MissionResultPanel)
	{
		MissionResultPanel->SetVisibility(
			DisplayedOutcome == EDroneMissionOutcome::None
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible);
	}
	const bool bHasResult = DisplayedOutcome != EDroneMissionOutcome::None;
	if (RetryMissionButton)
	{
		RetryMissionButton->SetIsEnabled(bHasResult && MissionController.IsValid());
	}
	if (ReturnToLobbyButton)
	{
		ReturnToLobbyButton->SetIsEnabled(bHasResult && MissionController.IsValid());
	}
	// 패드(UI-PAD-01): 보이는 버튼을 위아래로 명시해 잇는다. 결과 화면은 비행 HUD 위에 새로 뜨는 위젯이라
	// 위치 계산 이동이 첫 프레임에 다른 위젯을 고르지 않게 한다.
	const TArray<UWidget*> Ordered = GetOrderedButtons();
	for (int32 Index = 0; Index + 1 < Ordered.Num(); ++Index)
	{
		Ordered[Index]->SetNavigationRuleExplicit(EUINavigation::Down, Ordered[Index + 1]);
		Ordered[Index + 1]->SetNavigationRuleExplicit(EUINavigation::Up, Ordered[Index]);
	}
	if (bHasResult)
	{
		// 실패하면 바로 다시 할 수 있게 [다시 하기], 성공하면 [다음]/[미션 진행](없으면 [로비로])에 패드 포커스를 둔다.
		TArray<UWidget*> FocusOrder = DisplayedOutcome == EDroneMissionOutcome::Failure
			? TArray<UWidget*>{RetryMissionButton, ReturnToLobbyButton}
			: TArray<UWidget*>{ReturnToLobbyButton, RetryMissionButton};
		if (bNextMissionAvailable)
		{
			FocusOrder.Insert(NextMissionButton, 0);
		}
		GamepadFocus.RequestFocus(FocusOrder);
		ReceiveMissionResultDisplayed(DisplayedOutcome);
	}
}

void UDroneMissionResultWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	GamepadFocus.FocusScale = GamepadFocusScale;
	GamepadFocus.FocusTint = GamepadFocusTint;
	GamepadFocus.Tick(GetOwningPlayer(), GetOrderedButtons());
}
