#include "UI/DroneMissionObjectiveWidget.h"

#include "Abilities/DroneImpactDetonationComponent.h"
#include "Abilities/DronePayloadDropComponent.h"
#include "Abilities/DroneReconScanComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Mission/DroneMissionDirector.h"
#include "Prototype/DronePrototypePawn.h"
#include "Styling/CoreStyle.h"

namespace DroneMissionObjectiveUI
{
const FName PanelName(TEXT("MissionObjectivePanel"));
const FName TitleName(TEXT("MissionObjectiveTitleText"));
const FName ObjectiveName(TEXT("MissionObjectiveText"));
const FName ProgressName(TEXT("MissionObjectiveProgressText"));
const FName RoleInstructionName(TEXT("MissionRoleInstructionText"));
const FName RoleStatusName(TEXT("MissionRoleStatusText"));
}

void UDroneMissionObjectiveWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
	ApplySnapshot(DisplayedSnapshot);
}

void UDroneMissionObjectiveWidget::NativeDestruct()
{
	ClearDronePawn();
	ClearMissionDirector();
	Super::NativeDestruct();
}

void UDroneMissionObjectiveWidget::SetDronePawn(ADronePrototypePawn* InDronePawn)
{
	if (DronePawn.Get() == InDronePawn)
	{
		RefreshRoleDisplay();
		return;
	}
	ClearDronePawn();
	DronePawn = InDronePawn;
	if (!InDronePawn)
	{
		RefreshRoleDisplay();
		return;
	}

	if (UDroneReconScanComponent* Recon = InDronePawn->GetReconScanComponent())
	{
		Recon->OnScanProgress.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandleReconProgress);
		Recon->OnScanCompleted.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandleReconStateChanged);
		Recon->OnScanCanceled.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandleReconStateChanged);
	}
	if (UDroneImpactDetonationComponent* Impact = InDronePawn->GetImpactDetonationComponent())
	{
		Impact->OnImpactDetonationArmed.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandleImpactStateChanged);
		Impact->OnImpactDetonationDisarmed.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandleImpactStateChanged);
		Impact->OnImpactDetonated.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandleImpactDetonated);
	}
	if (UDronePayloadDropComponent* Drop = InDronePawn->GetPayloadDropComponent())
	{
		Drop->OnPayloadPickedUp.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandlePayloadPickedUp);
		Drop->OnPayloadDropped.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandlePayloadDropped);
		Drop->OnPayloadResolved.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandlePayloadResolved);
		Drop->OnDropViewChanged.AddUniqueDynamic(this, &UDroneMissionObjectiveWidget::HandleDropViewChanged);
	}
	RefreshRoleDisplay();
}

void UDroneMissionObjectiveWidget::ClearDronePawn()
{
	if (ADronePrototypePawn* Pawn = DronePawn.Get())
	{
		if (UDroneReconScanComponent* Recon = Pawn->GetReconScanComponent())
		{
			Recon->OnScanProgress.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandleReconProgress);
			Recon->OnScanCompleted.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandleReconStateChanged);
			Recon->OnScanCanceled.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandleReconStateChanged);
		}
		if (UDroneImpactDetonationComponent* Impact = Pawn->GetImpactDetonationComponent())
		{
			Impact->OnImpactDetonationArmed.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandleImpactStateChanged);
			Impact->OnImpactDetonationDisarmed.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandleImpactStateChanged);
			Impact->OnImpactDetonated.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandleImpactDetonated);
		}
		if (UDronePayloadDropComponent* Drop = Pawn->GetPayloadDropComponent())
		{
			Drop->OnPayloadPickedUp.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandlePayloadPickedUp);
			Drop->OnPayloadDropped.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandlePayloadDropped);
			Drop->OnPayloadResolved.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandlePayloadResolved);
			Drop->OnDropViewChanged.RemoveDynamic(this, &UDroneMissionObjectiveWidget::HandleDropViewChanged);
		}
	}
	DronePawn.Reset();
}

void UDroneMissionObjectiveWidget::HandlePayloadPickedUp(ADroneDroppedPayload*)
{
	RefreshRoleDisplay();
}

void UDroneMissionObjectiveWidget::HandleReconProgress(AActor*, const float)
{
	RefreshRoleDisplay();
}

void UDroneMissionObjectiveWidget::HandleReconStateChanged(AActor*)
{
	RefreshRoleDisplay();
}

void UDroneMissionObjectiveWidget::HandleImpactStateChanged()
{
	RefreshRoleDisplay();
}

void UDroneMissionObjectiveWidget::HandleImpactDetonated(FVector, AActor*)
{
	RefreshRoleDisplay();
}

void UDroneMissionObjectiveWidget::HandlePayloadDropped(ADroneDroppedPayload*)
{
	RefreshRoleDisplay();
}

void UDroneMissionObjectiveWidget::HandlePayloadResolved(ADroneDroppedPayload*, AActor*, bool)
{
	RefreshRoleDisplay();
}

void UDroneMissionObjectiveWidget::HandleDropViewChanged(bool)
{
	RefreshRoleDisplay();
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
	MissionRoleInstructionText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneMissionObjectiveUI::RoleInstructionName));
	MissionRoleStatusText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneMissionObjectiveUI::RoleStatusName));
	return MissionObjectivePanel
		&& MissionObjectiveTitleText
		&& MissionObjectiveText
		&& MissionObjectiveProgressText
		&& MissionRoleInstructionText
		&& MissionRoleStatusText;
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
	PanelSlot->SetAnchors(FAnchors(0.68f, 0.08f, 0.98f, 0.44f));
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
	MissionRoleInstructionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), DroneMissionObjectiveUI::RoleInstructionName);
	MissionRoleInstructionText->SetAutoWrapText(true);
	MissionRoleInstructionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.88f, 0.42f, 1.0f)));
	Column->AddChildToVerticalBox(MissionRoleInstructionText);
	MissionRoleStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), DroneMissionObjectiveUI::RoleStatusName);
	MissionRoleStatusText->SetAutoWrapText(true);
	MissionRoleStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.90f, 1.0f, 1.0f)));
	Column->AddChildToVerticalBox(MissionRoleStatusText);
}

void UDroneMissionObjectiveWidget::RefreshRoleDisplay()
{
	RoleInstructionDisplayText = FText::GetEmpty();
	RoleStatusDisplayText = FText::GetEmpty();
	ADronePrototypePawn* Pawn = DronePawn.Get();
	if (Pawn)
	{
		if (const UDroneReconScanComponent* Recon = Pawn->GetReconScanComponent(); Recon && Recon->IsFeatureEnabled())
		{
			RoleInstructionDisplayText = FText::FromString(TEXT("정찰: 좌클릭/RB 스캔 · 우클릭/LB 취소"));
			RoleStatusDisplayText = Recon->IsScanning()
				? FText::Format(FText::FromString(TEXT("스캔 진행 {0}%")), FText::AsNumber(FMath::RoundToInt(Recon->GetScanProgressNormalized() * 100.0f)))
				: FText::Format(FText::FromString(TEXT("스캔 완료 {0}개")), FText::AsNumber(Recon->GetCompletedScanCount()));
		}
		else if (const UDroneImpactDetonationComponent* Impact = Pawn->GetImpactDetonationComponent(); Impact && Impact->IsFeatureEnabled())
		{
			RoleInstructionDisplayText = FText::FromString(TEXT("FPV: 좌클릭/RB 무장 · 우클릭/LB 해제 · 고속 충돌"));
			RoleStatusDisplayText = Impact->HasDetonated()
				? FText::FromString(TEXT("자폭 완료"))
				: (Impact->IsArmed() ? FText::FromString(TEXT("경고: 자폭 무장됨")) : FText::FromString(TEXT("자폭 안전")));
		}
		else if (const UDronePayloadDropComponent* Drop = Pawn->GetPayloadDropComponent(); Drop && Drop->IsFeatureEnabled())
		{
			RoleInstructionDisplayText = Drop->GetRemainingPayloadCount() > 0
				? FText::FromString(TEXT("드랍: 우클릭/LB 탑뷰 · 좌클릭/RB 화물 투하"))
				: FText::FromString(TEXT("드랍: 화물 가까이서 좌클릭/RB 적재 · 우클릭/LB 탑뷰"));
			RoleStatusDisplayText = FText::Format(
				FText::FromString(TEXT("적재 {0}개 · 성공 {1}회 · 탑뷰 {2}")),
				FText::AsNumber(Drop->GetRemainingPayloadCount()),
				FText::AsNumber(Drop->GetSuccessfulDeliveryCount()),
				Pawn->IsDropCameraViewEnabled() ? FText::FromString(TEXT("ON")) : FText::FromString(TEXT("OFF")));
		}
	}
	if (MissionRoleInstructionText)
	{
		MissionRoleInstructionText->SetText(RoleInstructionDisplayText);
	}
	if (MissionRoleStatusText)
	{
		MissionRoleStatusText->SetText(RoleStatusDisplayText);
	}
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
	RefreshRoleDisplay();
}
