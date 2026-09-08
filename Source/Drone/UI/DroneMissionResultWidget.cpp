#include "UI/DroneMissionResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Styling/CoreStyle.h"

namespace DroneMissionResultUI
{
const FName PanelName(TEXT("MissionResultPanel"));
const FName TitleName(TEXT("MissionResultTitleText"));
const FName RetryName(TEXT("RetryMissionButton"));
const FName LobbyName(TEXT("ReturnToLobbyButton"));
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

void UDroneMissionResultWidget::HandleRetryClicked()
{
	RequestRetry();
}

void UDroneMissionResultWidget::HandleReturnToLobbyClicked()
{
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

	RetryMissionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneMissionResultUI::RetryName);
	UTextBlock* RetryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RetryMissionButtonText"));
	RetryText->SetText(FText::FromString(TEXT("같은 미션 재도전")));
	RetryMissionButton->AddChild(RetryText);
	Column->AddChildToVerticalBox(RetryMissionButton);
	ReturnToLobbyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneMissionResultUI::LobbyName);
	UTextBlock* LobbyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReturnToLobbyButtonText"));
	LobbyText->SetText(FText::FromString(TEXT("작전 로비로 복귀")));
	ReturnToLobbyButton->AddChild(LobbyText);
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
	if (bHasResult)
	{
		ReceiveMissionResultDisplayed(DisplayedOutcome);
	}
}
