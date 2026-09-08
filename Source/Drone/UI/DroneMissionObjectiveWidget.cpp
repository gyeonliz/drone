#include "UI/DroneMissionObjectiveWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Mission/DroneMissionDirector.h"
#include "Styling/CoreStyle.h"

namespace DroneMissionObjectiveUI
{
const FName PanelName(TEXT("MissionObjectivePanel"));
const FName TitleName(TEXT("MissionObjectiveTitleText"));
const FName ObjectiveName(TEXT("MissionObjectiveText"));
const FName ProgressName(TEXT("MissionObjectiveProgressText"));
}

void UDroneMissionObjectiveWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
	ApplySnapshot(DisplayedSnapshot);
}

void UDroneMissionObjectiveWidget::NativeDestruct()
{
	ClearMissionDirector();
	Super::NativeDestruct();
}

void UDroneMissionObjectiveWidget::SetMissionDirector(ADroneMissionDirector* InMissionDirector)
{
	if (MissionDirector.Get() != InMissionDirector)
	{
		ClearMissionDirector();
		MissionDirector = InMissionDirector;
	}
	if (InMissionDirector)
	{
		InMissionDirector->OnMissionSnapshotChanged.AddUniqueDynamic(
			this,
			&UDroneMissionObjectiveWidget::HandleMissionSnapshotChanged);
		ApplySnapshot(InMissionDirector->GetSnapshot());
	}
	else
	{
		ApplySnapshot(FDroneMissionRuntimeSnapshot());
	}
}

void UDroneMissionObjectiveWidget::ClearMissionDirector()
{
	if (ADroneMissionDirector* Director = MissionDirector.Get())
	{
		Director->OnMissionSnapshotChanged.RemoveDynamic(
			this,
			&UDroneMissionObjectiveWidget::HandleMissionSnapshotChanged);
	}
	MissionDirector.Reset();
}

void UDroneMissionObjectiveWidget::HandleMissionSnapshotChanged(
	const FDroneMissionRuntimeSnapshot& Snapshot)
{
	ApplySnapshot(Snapshot);
}

bool UDroneMissionObjectiveWidget::TryBindBlueprintLayout()
{
	if (!WidgetTree)
	{
		return false;
	}
	MissionObjectivePanel = WidgetTree->FindWidget(DroneMissionObjectiveUI::PanelName);
	MissionObjectiveTitleText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneMissionObjectiveUI::TitleName));
	MissionObjectiveText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneMissionObjectiveUI::ObjectiveName));
	MissionObjectiveProgressText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneMissionObjectiveUI::ProgressName));
	return MissionObjectivePanel
		&& MissionObjectiveTitleText
		&& MissionObjectiveText
		&& MissionObjectiveProgressText;
}

void UDroneMissionObjectiveWidget::BuildDefaultLayout()
{
	if (!WidgetTree || TryBindBlueprintLayout())
	{
		return;
	}
	bUsingNativeFallbackLayout = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ObjectiveRoot"));
	WidgetTree->RootWidget = Root;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), DroneMissionObjectiveUI::PanelName);
	Panel->SetBrushColor(FLinearColor(0.015f, 0.035f, 0.045f, 0.86f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.70f, 0.08f, 0.98f, 0.34f));
	PanelSlot->SetOffsets(FMargin(0.0f));
	MissionObjectivePanel = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObjectiveColumn"));
	Panel->SetContent(Column);
	MissionObjectiveTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), DroneMissionObjectiveUI::TitleName);
	MissionObjectiveTitleText->SetText(FText::FromString(TEXT("현재 미션 목표")));
	MissionObjectiveTitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20.0f));
	MissionObjectiveTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	Column->AddChildToVerticalBox(MissionObjectiveTitleText);
	MissionObjectiveText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), DroneMissionObjectiveUI::ObjectiveName);
	MissionObjectiveText->SetAutoWrapText(true);
	MissionObjectiveText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.96f, 0.96f, 1.0f)));
	Column->AddChildToVerticalBox(MissionObjectiveText);
	MissionObjectiveProgressText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), DroneMissionObjectiveUI::ProgressName);
	MissionObjectiveProgressText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.82f, 0.84f, 1.0f)));
	Column->AddChildToVerticalBox(MissionObjectiveProgressText);
}

void UDroneMissionObjectiveWidget::ApplySnapshot(const FDroneMissionRuntimeSnapshot& Snapshot)
{
	DisplayedSnapshot = Snapshot;
	if (Snapshot.State == EDroneMissionRuntimeState::Active
		&& Snapshot.Objectives.IsValidIndex(Snapshot.CurrentObjectiveIndex))
	{
		const FDroneMissionObjectiveSnapshot& Objective = Snapshot.Objectives[Snapshot.CurrentObjectiveIndex];
		ObjectiveDisplayText = Objective.Description;
		ProgressDisplayText = FText::Format(
			FText::FromString(TEXT("목표 {0}/{1}  |  진행 {2}/{3}")),
			FText::AsNumber(Snapshot.CurrentObjectiveIndex + 1),
			FText::AsNumber(Snapshot.Objectives.Num()),
			FText::AsNumber(Objective.CurrentProgress),
			FText::AsNumber(Objective.RequiredProgress));
	}
	else if (Snapshot.State == EDroneMissionRuntimeState::Finished)
	{
		ObjectiveDisplayText = Snapshot.Outcome == EDroneMissionOutcome::Success
			? FText::FromString(TEXT("미션 성공"))
			: FText::FromString(TEXT("미션 실패"));
		ProgressDisplayText = FText::GetEmpty();
	}
	else
	{
		ObjectiveDisplayText = FText::FromString(TEXT("미션 대기 중"));
		ProgressDisplayText = FText::GetEmpty();
	}

	if (MissionObjectivePanel)
	{
		MissionObjectivePanel->SetVisibility(
			Snapshot.State == EDroneMissionRuntimeState::Inactive
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible);
	}
	if (MissionObjectiveText)
	{
		MissionObjectiveText->SetText(ObjectiveDisplayText);
	}
	if (MissionObjectiveProgressText)
	{
		MissionObjectiveProgressText->SetText(ProgressDisplayText);
	}
	ReceiveObjectiveSnapshotDisplayed(Snapshot);
}
