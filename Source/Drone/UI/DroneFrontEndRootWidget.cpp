#include "UI/DroneFrontEndRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Mission/DroneMissionDefinition.h"
#include "Styling/CoreStyle.h"

namespace DroneFrontEndUI
{
const FName OpeningPanelName(TEXT("OpeningPanel"));
const FName LobbyPanelName(TEXT("LobbyPanel"));
const FName MissionBriefingPanelName(TEXT("MissionBriefingPanel"));
const FName ContinueButtonName(TEXT("ContinueButton"));
const FName OpeningTitleName(TEXT("OpeningTitleText"));
const FName LobbyTitleName(TEXT("LobbyTitleText"));
const FName LobbyStatusName(TEXT("LobbyStatusText"));
const FName MissionSelectButtonName(TEXT("MissionSelectButton"));
const FName MissionSelectButtonTextName(TEXT("MissionSelectButtonText"));
const FName MissionNameName(TEXT("MissionNameText"));
const FName MissionDescriptionName(TEXT("MissionDescriptionText"));
const FName MissionMetaName(TEXT("MissionMetaText"));
const FName StartMissionButtonName(TEXT("StartMissionButton"));
const FName MissionBriefingTitleName(TEXT("MissionBriefingTitleText"));
const FName MissionBriefingBodyName(TEXT("MissionBriefingBodyText"));
const FName FinishMissionBriefingButtonName(TEXT("FinishMissionBriefingButton"));
}

void UDroneFrontEndRootWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// UIOnly 입력 모드가 Root에 안전하게 Focus를 줄 수 있게 한다.
	SetIsFocusable(true);
	BuildDefaultLayout();
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleContinueClicked);
	}
	if (MissionSelectButton)
	{
		MissionSelectButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleFirstMissionClicked);
	}
	if (StartMissionButton)
	{
		StartMissionButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleStartMissionClicked);
	}
	if (FinishMissionBriefingButton)
	{
		FinishMissionBriefingButton->OnClicked.AddUniqueDynamic(
			this,
			&UDroneFrontEndRootWidget::HandleFinishBriefingClicked);
	}
	ApplyDisplayedState(DisplayedState);
}

void UDroneFrontEndRootWidget::NativeDestruct()
{
	if (ContinueButton)
	{
		ContinueButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleContinueClicked);
	}
	if (MissionSelectButton)
	{
		MissionSelectButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleFirstMissionClicked);
	}
	if (StartMissionButton)
	{
		StartMissionButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleStartMissionClicked);
	}
	if (FinishMissionBriefingButton)
	{
		FinishMissionBriefingButton->OnClicked.RemoveDynamic(
			this,
			&UDroneFrontEndRootWidget::HandleFinishBriefingClicked);
	}
	ClearFlowBinding();
	Super::NativeDestruct();
}

void UDroneFrontEndRootWidget::SetFlowSubsystem(UDroneGameFlowSubsystem* InFlowSubsystem)
{
	if (FlowSubsystem.Get() != InFlowSubsystem)
	{
		ClearFlowBinding();
		FlowSubsystem = InFlowSubsystem;
	}

	if (InFlowSubsystem)
	{
		// 같은 Widget을 다시 연결해도 State Delegate는 한 번만 등록한다.
		InFlowSubsystem->OnFlowStateChanged.AddUniqueDynamic(
			this,
			&UDroneFrontEndRootWidget::HandleFlowStateChanged);
		InFlowSubsystem->OnFlowSnapshotChanged.AddUniqueDynamic(
			this,
			&UDroneFrontEndRootWidget::HandleFlowSnapshotChanged);
		ApplyDisplayedState(InFlowSubsystem->GetSnapshot().State);
	}
	else
	{
		ApplyDisplayedState(EDroneGameFlowState::Boot);
	}
}

bool UDroneFrontEndRootWidget::SelectLobbyMission(const FName MissionId)
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	return Flow && Flow->SelectMission(MissionId);
}

bool UDroneFrontEndRootWidget::ConfirmSelectedMission()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	return Flow && Flow->ConfirmMissionSelection();
}

bool UDroneFrontEndRootWidget::FinishOpeningTrailer()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	return Flow && Flow->EnterLobbyFromOpeningTrailer();
}

bool UDroneFrontEndRootWidget::FinishMissionBriefing()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	if (!Flow || Flow->GetSnapshot().State != EDroneGameFlowState::MissionTrailer)
	{
		return false;
	}

	UDroneMissionDefinition* Mission = Flow->FindMissionDefinition(Flow->GetSnapshot().SelectedMissionId);
	// Map 참조를 먼저 검증해 Loading 상태에 진입한 뒤 되돌릴 수 없는 정지 상태를 만들지 않는다.
	if (!Mission || Mission->MissionMap.IsNull() || !Flow->NotifyMissionTrailerFinished())
	{
		return false;
	}

	OnMissionMapLoadRequested.Broadcast(Mission);
	return true;
}

void UDroneFrontEndRootWidget::HandleFlowStateChanged(
	const EDroneGameFlowState /*PreviousState*/,
	const EDroneGameFlowState NewState)
{
	ApplyDisplayedState(NewState);
}

void UDroneFrontEndRootWidget::HandleFlowSnapshotChanged(const FDroneGameFlowSnapshot& Snapshot)
{
	if (Snapshot.State == EDroneGameFlowState::LobbyMissionSelect)
	{
		RefreshLobbyContent();
	}
	else if (Snapshot.State == EDroneGameFlowState::MissionTrailer)
	{
		RefreshMissionBriefingContent();
	}
}

void UDroneFrontEndRootWidget::HandleContinueClicked()
{
	FinishOpeningTrailer();
}

void UDroneFrontEndRootWidget::HandleFirstMissionClicked()
{
	if (!FirstDisplayedMissionId.IsNone())
	{
		SelectLobbyMission(FirstDisplayedMissionId);
	}
}

void UDroneFrontEndRootWidget::HandleStartMissionClicked()
{
	ConfirmSelectedMission();
}

void UDroneFrontEndRootWidget::HandleFinishBriefingClicked()
{
	FinishMissionBriefing();
}

bool UDroneFrontEndRootWidget::TryBindBlueprintLayout()
{
	if (!WidgetTree)
	{
		return false;
	}

	OpeningPanel = WidgetTree->FindWidget(DroneFrontEndUI::OpeningPanelName);
	LobbyPanel = WidgetTree->FindWidget(DroneFrontEndUI::LobbyPanelName);
	MissionBriefingPanel = WidgetTree->FindWidget(DroneFrontEndUI::MissionBriefingPanelName);
	ContinueButton = Cast<UButton>(WidgetTree->FindWidget(DroneFrontEndUI::ContinueButtonName));
	OpeningTitleText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::OpeningTitleName));
	LobbyTitleText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::LobbyTitleName));
	LobbyStatusText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::LobbyStatusName));
	MissionSelectButton = Cast<UButton>(WidgetTree->FindWidget(DroneFrontEndUI::MissionSelectButtonName));
	MissionSelectButtonText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::MissionSelectButtonTextName));
	MissionNameText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::MissionNameName));
	MissionDescriptionText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::MissionDescriptionName));
	MissionMetaText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::MissionMetaName));
	StartMissionButton = Cast<UButton>(WidgetTree->FindWidget(DroneFrontEndUI::StartMissionButtonName));
	MissionBriefingTitleText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::MissionBriefingTitleName));
	MissionBriefingBodyText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFrontEndUI::MissionBriefingBodyName));
	FinishMissionBriefingButton = Cast<UButton>(
		WidgetTree->FindWidget(DroneFrontEndUI::FinishMissionBriefingButtonName));
	return OpeningPanel
		&& LobbyPanel
		&& MissionBriefingPanel
		&& ContinueButton
		&& MissionSelectButton
		&& MissionSelectButtonText
		&& MissionNameText
		&& MissionDescriptionText
		&& MissionMetaText
		&& StartMissionButton
		&& MissionBriefingTitleText
		&& MissionBriefingBodyText
		&& FinishMissionBriefingButton;
}

void UDroneFrontEndRootWidget::BuildDefaultLayout()
{
	if (!WidgetTree || TryBindBlueprintLayout())
	{
		return;
	}

	bUsingNativeFallbackLayout = true;
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("FrontEndRoot"));
	WidgetTree->RootWidget = RootCanvas;

	auto AddFullScreenPanel = [this, RootCanvas](const FName Name, const FLinearColor Color)
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Panel->SetBrushColor(Color);
		UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(Panel);
		Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		Slot->SetOffsets(FMargin(0.0f));
		return Panel;
	};

	UBorder* NativeOpeningPanel = AddFullScreenPanel(
		DroneFrontEndUI::OpeningPanelName,
		FLinearColor(0.005f, 0.012f, 0.018f, 1.0f));
	OpeningPanel = NativeOpeningPanel;
	UVerticalBox* OpeningColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("OpeningColumn"));
	NativeOpeningPanel->SetContent(OpeningColumn);

	OpeningTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::OpeningTitleName);
	OpeningTitleText->SetText(FText::FromString(TEXT("드론 운용 시스템")));
	OpeningTitleText->SetJustification(ETextJustify::Center);
	OpeningTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	OpeningTitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 36.0f));
	OpeningColumn->AddChildToVerticalBox(OpeningTitleText);

	UTextBlock* PlaceholderText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("OpeningPlaceholderText"));
	PlaceholderText->SetText(FText::FromString(
		TEXT("시작 트레일러가 준비되기 전 사용하는 정적 대체 화면입니다.")));
	PlaceholderText->SetJustification(ETextJustify::Center);
	PlaceholderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.85f, 1.0f)));
	OpeningColumn->AddChildToVerticalBox(PlaceholderText);

	ContinueButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		DroneFrontEndUI::ContinueButtonName);
	UTextBlock* ContinueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ContinueButtonText"));
	ContinueText->SetText(FText::FromString(TEXT("계속")));
	ContinueText->SetJustification(ETextJustify::Center);
	ContinueButton->AddChild(ContinueText);
	OpeningColumn->AddChildToVerticalBox(ContinueButton);

	UBorder* NativeLobbyPanel = AddFullScreenPanel(
		DroneFrontEndUI::LobbyPanelName,
		FLinearColor(0.012f, 0.025f, 0.032f, 1.0f));
	LobbyPanel = NativeLobbyPanel;
	UVerticalBox* LobbyColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("LobbyColumn"));
	NativeLobbyPanel->SetContent(LobbyColumn);

	LobbyTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::LobbyTitleName);
	LobbyTitleText->SetText(FText::FromString(TEXT("작전 로비")));
	LobbyTitleText->SetJustification(ETextJustify::Center);
	LobbyTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	LobbyTitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 32.0f));
	LobbyColumn->AddChildToVerticalBox(LobbyTitleText);

	LobbyStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::LobbyStatusName);
	LobbyStatusText->SetText(FText::FromString(
		TEXT("미션을 선택해 상세 정보를 확인하세요.")));
	LobbyStatusText->SetJustification(ETextJustify::Center);
	LobbyStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.85f, 1.0f)));
	LobbyColumn->AddChildToVerticalBox(LobbyStatusText);

	MissionSelectButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		DroneFrontEndUI::MissionSelectButtonName);
	MissionSelectButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionSelectButtonTextName);
	MissionSelectButtonText->SetText(FText::FromString(TEXT("등록된 미션 없음")));
	MissionSelectButton->AddChild(MissionSelectButtonText);
	LobbyColumn->AddChildToVerticalBox(MissionSelectButton);

	MissionNameText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionNameName);
	MissionNameText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24.0f));
	MissionNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.96f, 0.96f, 1.0f)));
	LobbyColumn->AddChildToVerticalBox(MissionNameText);

	MissionDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionDescriptionName);
	MissionDescriptionText->SetAutoWrapText(true);
	MissionDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.85f, 1.0f)));
	LobbyColumn->AddChildToVerticalBox(MissionDescriptionText);

	MissionMetaText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionMetaName);
	MissionMetaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	LobbyColumn->AddChildToVerticalBox(MissionMetaText);

	StartMissionButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		DroneFrontEndUI::StartMissionButtonName);
	UTextBlock* StartMissionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("StartMissionButtonText"));
	StartMissionText->SetText(FText::FromString(TEXT("미션 시작")));
	StartMissionButton->AddChild(StartMissionText);
	LobbyColumn->AddChildToVerticalBox(StartMissionButton);

	// FLOW-04의 영상 Asset이 없어도 전체 진입 흐름을 시험할 수 있는 정적 Briefing 안전망이다.
	UBorder* NativeBriefingPanel = AddFullScreenPanel(
		DroneFrontEndUI::MissionBriefingPanelName,
		FLinearColor(0.008f, 0.018f, 0.025f, 1.0f));
	MissionBriefingPanel = NativeBriefingPanel;
	UVerticalBox* BriefingColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("MissionBriefingColumn"));
	NativeBriefingPanel->SetContent(BriefingColumn);

	MissionBriefingTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionBriefingTitleName);
	MissionBriefingTitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 32.0f));
	MissionBriefingTitleText->SetJustification(ETextJustify::Center);
	MissionBriefingTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	BriefingColumn->AddChildToVerticalBox(MissionBriefingTitleText);

	MissionBriefingBodyText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionBriefingBodyName);
	MissionBriefingBodyText->SetAutoWrapText(true);
	MissionBriefingBodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.87f, 0.89f, 1.0f)));
	BriefingColumn->AddChildToVerticalBox(MissionBriefingBodyText);

	FinishMissionBriefingButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		DroneFrontEndUI::FinishMissionBriefingButtonName);
	UTextBlock* FinishBriefingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("FinishMissionBriefingButtonText"));
	FinishBriefingText->SetText(FText::FromString(TEXT("작전 지역으로 이동")));
	FinishMissionBriefingButton->AddChild(FinishBriefingText);
	BriefingColumn->AddChildToVerticalBox(FinishMissionBriefingButton);
	RefreshLobbyContent();
	RefreshMissionBriefingContent();
}

void UDroneFrontEndRootWidget::ApplyDisplayedState(const EDroneGameFlowState State)
{
	DisplayedState = State;
	if (OpeningPanel)
	{
		OpeningPanel->SetVisibility(
			State == EDroneGameFlowState::OpeningTrailer
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (LobbyPanel)
	{
		LobbyPanel->SetVisibility(
			State == EDroneGameFlowState::LobbyMissionSelect
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (MissionBriefingPanel)
	{
		MissionBriefingPanel->SetVisibility(
			State == EDroneGameFlowState::MissionTrailer
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (State == EDroneGameFlowState::LobbyMissionSelect)
	{
		RefreshLobbyContent();
	}
	else if (State == EDroneGameFlowState::MissionTrailer)
	{
		RefreshMissionBriefingContent();
	}
	ReceiveFrontEndStateDisplayed(State);
}

void UDroneFrontEndRootWidget::RefreshMissionBriefingContent()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	UDroneMissionDefinition* Mission = Flow
		? Flow->FindMissionDefinition(Flow->GetSnapshot().SelectedMissionId)
		: nullptr;
	if (Mission)
	{
		DisplayedBriefingTitle = Mission->DisplayName;
		FString Body = Mission->LobbyDescription.ToString();
		if (!Mission->InitialObjectives.IsEmpty())
		{
			Body += TEXT("\n\n초기 목표");
			for (const FText& Objective : Mission->InitialObjectives)
			{
				Body += FString::Printf(TEXT("\n- %s"), *Objective.ToString());
			}
		}
		DisplayedBriefingBody = FText::FromString(Body);
	}
	else
	{
		DisplayedBriefingTitle = FText::FromString(TEXT("작전 브리핑"));
		DisplayedBriefingBody = FText::FromString(TEXT("선택된 미션 정보가 없습니다."));
	}

	if (MissionBriefingTitleText)
	{
		MissionBriefingTitleText->SetText(DisplayedBriefingTitle);
	}
	if (MissionBriefingBodyText)
	{
		MissionBriefingBodyText->SetText(DisplayedBriefingBody);
	}
	if (FinishMissionBriefingButton)
	{
		FinishMissionBriefingButton->SetIsEnabled(Mission && !Mission->MissionMap.IsNull());
	}
}

void UDroneFrontEndRootWidget::RefreshLobbyContent()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	const TArray<FName> MissionIds = Flow ? Flow->GetRegisteredMissionIds() : TArray<FName>();
	FirstDisplayedMissionId = MissionIds.IsEmpty() ? NAME_None : MissionIds[0];
	UDroneMissionDefinition* FirstMission = Flow
		? Flow->FindMissionDefinition(FirstDisplayedMissionId)
		: nullptr;
	if (MissionSelectButtonText)
	{
		MissionSelectButtonText->SetText(
			FirstMission ? FirstMission->DisplayName : FText::FromString(TEXT("등록된 미션 없음")));
	}
	if (MissionSelectButton)
	{
		MissionSelectButton->SetIsEnabled(FirstMission != nullptr);
	}

	const FName SelectedMissionId = Flow ? Flow->GetSnapshot().SelectedMissionId : NAME_None;
	UDroneMissionDefinition* SelectedMission = Flow
		? Flow->FindMissionDefinition(SelectedMissionId)
		: nullptr;
	if (SelectedMission)
	{
		DisplayedMissionName = SelectedMission->DisplayName;
		DisplayedMissionDescription = SelectedMission->LobbyDescription;
		DisplayedMissionMeta = FText::Format(
			FText::FromString(TEXT("지역: {0}  |  난이도: {1}")),
			SelectedMission->RegionText,
			SelectedMission->DifficultyText);
	}
	else
	{
		DisplayedMissionName = FText::FromString(TEXT("미션을 선택하세요"));
		DisplayedMissionDescription = FText::FromString(TEXT("목록에서 미션을 고르면 설명이 표시됩니다."));
		DisplayedMissionMeta = FText::GetEmpty();
	}

	if (MissionNameText)
	{
		MissionNameText->SetText(DisplayedMissionName);
	}
	if (MissionDescriptionText)
	{
		MissionDescriptionText->SetText(DisplayedMissionDescription);
	}
	if (MissionMetaText)
	{
		MissionMetaText->SetText(DisplayedMissionMeta);
	}
	if (StartMissionButton)
	{
		StartMissionButton->SetIsEnabled(SelectedMission != nullptr);
	}
	ReceiveLobbyMissionSelectionChanged(SelectedMission);
}

void UDroneFrontEndRootWidget::ClearFlowBinding()
{
	if (UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get())
	{
		Flow->OnFlowStateChanged.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleFlowStateChanged);
		Flow->OnFlowSnapshotChanged.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleFlowSnapshotChanged);
	}
	FlowSubsystem.Reset();
}
