#include "UI/DroneSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Mission/DroneDefinition.h"
#include "Mission/DroneMissionDefinition.h"
#include "Styling/CoreStyle.h"

namespace DroneSelectionUI
{
const FName PanelName(TEXT("DroneSelectionPanel"));
const FName MissionNameName(TEXT("MissionNameText"));
const FName DroneNameName(TEXT("DroneNameText"));
const FName DroneDescriptionName(TEXT("DroneDescriptionText"));
const FName DroneProfileName(TEXT("DroneProfileText"));
const FName ControlModeButtonName(TEXT("ControlModeButton"));
const FName ControlModeButtonTextName(TEXT("ControlModeButtonText"));
const FName HandlingPresetButtonName(TEXT("HandlingPresetButton"));
const FName HandlingPresetButtonTextName(TEXT("HandlingPresetButtonText"));
const FName LaunchButtonName(TEXT("LaunchDroneButton"));

FName GetDroneButtonName(const int32 Index)
{
	return FName(*FString::Printf(TEXT("DroneButton%d"), Index));
}

FName GetDroneButtonTextName(const int32 Index)
{
	return FName(*FString::Printf(TEXT("DroneButton%dText"), Index));
}

FText GetControlModeText(const EDroneControlMode Mode)
{
	switch (Mode)
	{
	case EDroneControlMode::ManualRealisticGreybox:
		return FText::FromString(TEXT("조작: 실제 조작형 (제한 자세)"));
	case EDroneControlMode::AcroRateRealisticGreybox:
		return FText::FromString(TEXT("조작: FPV Rate/Acro (그레이박스)"));
	case EDroneControlMode::AssistedEasy:
	default:
		return FText::FromString(TEXT("조작: 쉬운 조작"));
	}
}

FText GetHandlingText(const EDroneHandlingPreset Preset)
{
	switch (Preset)
	{
	case EDroneHandlingPreset::Stable:
		return FText::FromString(TEXT("반응성: 안정"));
	case EDroneHandlingPreset::Agile:
		return FText::FromString(TEXT("반응성: 고기동"));
	case EDroneHandlingPreset::Balanced:
	default:
		return FText::FromString(TEXT("반응성: 균형"));
	}
}
}

void UDroneSelectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildDefaultLayout();

	if (DroneButtons.IsValidIndex(0) && DroneButtons[0])
	{
		DroneButtons[0]->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleDroneButton0Clicked);
	}
	if (DroneButtons.IsValidIndex(1) && DroneButtons[1])
	{
		DroneButtons[1]->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleDroneButton1Clicked);
	}
	if (DroneButtons.IsValidIndex(2) && DroneButtons[2])
	{
		DroneButtons[2]->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleDroneButton2Clicked);
	}
	if (ControlModeButton)
	{
		ControlModeButton->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleControlModeClicked);
	}
	if (HandlingPresetButton)
	{
		HandlingPresetButton->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleHandlingPresetClicked);
	}
	if (LaunchDroneButton)
	{
		LaunchDroneButton->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleLaunchClicked);
	}
	RefreshFromFlow();
}

void UDroneSelectionWidget::NativeDestruct()
{
	if (DroneButtons.IsValidIndex(0) && DroneButtons[0])
	{
		DroneButtons[0]->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleDroneButton0Clicked);
	}
	if (DroneButtons.IsValidIndex(1) && DroneButtons[1])
	{
		DroneButtons[1]->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleDroneButton1Clicked);
	}
	if (DroneButtons.IsValidIndex(2) && DroneButtons[2])
	{
		DroneButtons[2]->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleDroneButton2Clicked);
	}
	if (ControlModeButton)
	{
		ControlModeButton->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleControlModeClicked);
	}
	if (HandlingPresetButton)
	{
		HandlingPresetButton->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleHandlingPresetClicked);
	}
	if (LaunchDroneButton)
	{
		LaunchDroneButton->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleLaunchClicked);
	}
	ClearFlowBinding();
	Super::NativeDestruct();
}

void UDroneSelectionWidget::SetFlowSubsystem(UDroneGameFlowSubsystem* InFlowSubsystem)
{
	if (FlowSubsystem.Get() != InFlowSubsystem)
	{
		ClearFlowBinding();
		FlowSubsystem = InFlowSubsystem;
	}
	if (InFlowSubsystem)
	{
		InFlowSubsystem->OnFlowSnapshotChanged.AddUniqueDynamic(
			this,
			&UDroneSelectionWidget::HandleFlowSnapshotChanged);
	}
	RefreshFromFlow();
}

bool UDroneSelectionWidget::SelectDrone(const FName DroneId)
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	return Flow && Flow->SelectDrone(DroneId);
}

void UDroneSelectionWidget::ToggleControlMode()
{
	switch (SelectedControlMode)
	{
	case EDroneControlMode::AssistedEasy:
		SelectedControlMode = EDroneControlMode::ManualRealisticGreybox;
		break;
	case EDroneControlMode::ManualRealisticGreybox:
		SelectedControlMode = EDroneControlMode::AcroRateRealisticGreybox;
		break;
	case EDroneControlMode::AcroRateRealisticGreybox:
	default:
		SelectedControlMode = EDroneControlMode::AssistedEasy;
		break;
	}
	RefreshControlLabels();
}

void UDroneSelectionWidget::CycleHandlingPreset()
{
	switch (SelectedHandlingPreset)
	{
	case EDroneHandlingPreset::Stable:
		SelectedHandlingPreset = EDroneHandlingPreset::Balanced;
		break;
	case EDroneHandlingPreset::Balanced:
		SelectedHandlingPreset = EDroneHandlingPreset::Agile;
		break;
	case EDroneHandlingPreset::Agile:
	default:
		SelectedHandlingPreset = EDroneHandlingPreset::Stable;
		break;
	}
	RefreshControlLabels();
}

bool UDroneSelectionWidget::ConfirmAndLaunchSelectedDrone()
{
	ADroneMissionPlayerController* MissionController = Cast<ADroneMissionPlayerController>(GetOwningPlayer());
	return MissionController
		&& MissionController->StartSelectedDrone(SelectedControlMode, SelectedHandlingPreset);
}

void UDroneSelectionWidget::HandleFlowSnapshotChanged(const FDroneGameFlowSnapshot& /*Snapshot*/)
{
	RefreshFromFlow();
}

void UDroneSelectionWidget::HandleDroneButton0Clicked()
{
	SelectDisplayedButton(0);
}

void UDroneSelectionWidget::HandleDroneButton1Clicked()
{
	SelectDisplayedButton(1);
}

void UDroneSelectionWidget::HandleDroneButton2Clicked()
{
	SelectDisplayedButton(2);
}

void UDroneSelectionWidget::HandleControlModeClicked()
{
	ToggleControlMode();
}

void UDroneSelectionWidget::HandleHandlingPresetClicked()
{
	CycleHandlingPreset();
}

void UDroneSelectionWidget::HandleLaunchClicked()
{
	ConfirmAndLaunchSelectedDrone();
}

bool UDroneSelectionWidget::TryBindBlueprintLayout()
{
	if (!WidgetTree)
	{
		return false;
	}

	DroneSelectionPanel = WidgetTree->FindWidget(DroneSelectionUI::PanelName);
	MissionNameText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::MissionNameName));
	DroneNameText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::DroneNameName));
	DroneDescriptionText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::DroneDescriptionName));
	DroneProfileText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::DroneProfileName));
	DroneButtons.Reset();
	DroneButtonTexts.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		DroneButtons.Add(Cast<UButton>(WidgetTree->FindWidget(DroneSelectionUI::GetDroneButtonName(Index))));
		DroneButtonTexts.Add(Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::GetDroneButtonTextName(Index))));
	}
	ControlModeButton = Cast<UButton>(WidgetTree->FindWidget(DroneSelectionUI::ControlModeButtonName));
	ControlModeButtonText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::ControlModeButtonTextName));
	HandlingPresetButton = Cast<UButton>(WidgetTree->FindWidget(DroneSelectionUI::HandlingPresetButtonName));
	HandlingPresetButtonText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::HandlingPresetButtonTextName));
	LaunchDroneButton = Cast<UButton>(WidgetTree->FindWidget(DroneSelectionUI::LaunchButtonName));
	return DroneSelectionPanel
		&& MissionNameText
		&& DroneNameText
		&& DroneDescriptionText
		&& DroneProfileText
		&& DroneButtons.Num() == 3
		&& DroneButtons[0]
		&& DroneButtons[1]
		&& DroneButtons[2]
		&& DroneButtonTexts[0]
		&& DroneButtonTexts[1]
		&& DroneButtonTexts[2]
		&& ControlModeButton
		&& ControlModeButtonText
		&& HandlingPresetButton
		&& HandlingPresetButtonText
		&& LaunchDroneButton;
}

void UDroneSelectionWidget::BuildDefaultLayout()
{
	if (!WidgetTree || TryBindBlueprintLayout())
	{
		return;
	}

	bUsingNativeFallbackLayout = true;
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SelectionRoot"));
	WidgetTree->RootWidget = RootCanvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), DroneSelectionUI::PanelName);
	Panel->SetBrushColor(FLinearColor(0.008f, 0.018f, 0.025f, 0.97f));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	PanelSlot->SetOffsets(FMargin(0.0f));
	DroneSelectionPanel = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SelectionColumn"));
	Panel->SetContent(Column);
	MissionNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), DroneSelectionUI::MissionNameName);
	MissionNameText->SetText(FText::FromString(TEXT("기체 선택")));
	MissionNameText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30.0f));
	MissionNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	Column->AddChildToVerticalBox(MissionNameText);

	DroneButtons.Reset();
	DroneButtonTexts.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneSelectionUI::GetDroneButtonName(Index));
		UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			DroneSelectionUI::GetDroneButtonTextName(Index));
		ButtonText->SetText(FText::FromString(TEXT("기체 슬롯")));
		Button->AddChild(ButtonText);
		Column->AddChildToVerticalBox(Button);
		DroneButtons.Add(Button);
		DroneButtonTexts.Add(ButtonText);
	}

	DroneNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), DroneSelectionUI::DroneNameName);
	DroneNameText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 23.0f));
	DroneNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.96f, 0.96f, 1.0f)));
	Column->AddChildToVerticalBox(DroneNameText);
	DroneDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), DroneSelectionUI::DroneDescriptionName);
	DroneDescriptionText->SetAutoWrapText(true);
	DroneDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.85f, 1.0f)));
	Column->AddChildToVerticalBox(DroneDescriptionText);
	DroneProfileText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), DroneSelectionUI::DroneProfileName);
	DroneProfileText->SetAutoWrapText(true);
	DroneProfileText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.90f, 0.83f, 1.0f)));
	Column->AddChildToVerticalBox(DroneProfileText);

	ControlModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneSelectionUI::ControlModeButtonName);
	ControlModeButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), DroneSelectionUI::ControlModeButtonTextName);
	ControlModeButton->AddChild(ControlModeButtonText);
	Column->AddChildToVerticalBox(ControlModeButton);
	HandlingPresetButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), DroneSelectionUI::HandlingPresetButtonName);
	HandlingPresetButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), DroneSelectionUI::HandlingPresetButtonTextName);
	HandlingPresetButton->AddChild(HandlingPresetButtonText);
	Column->AddChildToVerticalBox(HandlingPresetButton);

	LaunchDroneButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneSelectionUI::LaunchButtonName);
	UTextBlock* LaunchText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LaunchDroneButtonText"));
	LaunchText->SetText(FText::FromString(TEXT("선택 기체 출격")));
	LaunchDroneButton->AddChild(LaunchText);
	Column->AddChildToVerticalBox(LaunchDroneButton);
	RefreshControlLabels();
}

void UDroneSelectionWidget::RefreshFromFlow()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	const FDroneGameFlowSnapshot Snapshot = Flow ? Flow->GetSnapshot() : FDroneGameFlowSnapshot();
	if (DroneSelectionPanel)
	{
		DroneSelectionPanel->SetVisibility(
			Snapshot.State == EDroneGameFlowState::DroneSelect
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}

	UDroneMissionDefinition* Mission = Flow ? Flow->FindMissionDefinition(Snapshot.SelectedMissionId) : nullptr;
	if (MissionNameText)
	{
		MissionNameText->SetText(Mission
			? FText::Format(FText::FromString(TEXT("{0} - 기체 선택")), Mission->DisplayName)
			: FText::FromString(TEXT("기체 선택")));
	}

	const TArray<UDroneDefinition*> Definitions = Flow ? Flow->GetAvailableDroneDefinitions() : TArray<UDroneDefinition*>();
	DisplayedDroneButtonIds.Reset();
	for (int32 Index = 0; Index < DroneButtons.Num(); ++Index)
	{
		UDroneDefinition* Definition = Definitions.IsValidIndex(Index) ? Definitions[Index] : nullptr;
		DisplayedDroneButtonIds.Add(Definition ? Definition->DroneId : NAME_None);
		if (DroneButtons[Index])
		{
			DroneButtons[Index]->SetVisibility(Definition ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			DroneButtons[Index]->SetIsEnabled(Definition != nullptr);
		}
		if (DroneButtonTexts.IsValidIndex(Index) && DroneButtonTexts[Index])
		{
			DroneButtonTexts[Index]->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
		}
	}

	UDroneDefinition* SelectedDefinition = Flow ? Flow->GetSelectedDroneDefinition() : nullptr;
	const FName NewDroneId = SelectedDefinition ? SelectedDefinition->DroneId : NAME_None;
	if (NewDroneId != DisplayedDroneId)
	{
		DisplayedDroneId = NewDroneId;
		if (SelectedDefinition)
		{
			SelectedControlMode = SelectedDefinition->FlightProfile.DefaultControlMode;
			SelectedHandlingPreset = SelectedDefinition->FlightProfile.DefaultHandlingPreset;
		}
	}

	if (SelectedDefinition)
	{
		DisplayedDroneName = SelectedDefinition->DisplayName;
		DisplayedDroneDescription = SelectedDefinition->Description;
	}
	else
	{
		DisplayedDroneName = FText::FromString(TEXT("기체를 선택하세요"));
		DisplayedDroneDescription = FText::FromString(TEXT("역할별 기체 카드 중 하나를 선택하면 실제 구현 기능이 표시됩니다."));
	}
	if (DroneNameText)
	{
		DroneNameText->SetText(DisplayedDroneName);
	}
	if (DroneDescriptionText)
	{
		DroneDescriptionText->SetText(DisplayedDroneDescription);
	}
	if (DroneProfileText)
	{
		FString Profile;
		if (SelectedDefinition)
		{
			Profile = FString::Printf(
				TEXT("최대 속도 %.0f cm/s | 기본 체력 %.0f"),
				SelectedDefinition->FlightProfile.MaxSpeedCentimetersPerSecond,
				SelectedDefinition->FlightProfile.MaxHealth);
			for (const FText& Highlight : SelectedDefinition->FlightProfile.FeatureHighlights)
			{
				Profile += FString::Printf(TEXT("\n- %s"), *Highlight.ToString());
			}
		}
		DroneProfileText->SetText(FText::FromString(Profile));
	}
	if (LaunchDroneButton)
	{
		LaunchDroneButton->SetIsEnabled(SelectedDefinition != nullptr && Snapshot.State == EDroneGameFlowState::DroneSelect);
	}
	RefreshControlLabels();
	ReceiveDroneSelectionRefreshed(SelectedDefinition);
}

void UDroneSelectionWidget::RefreshControlLabels()
{
	if (ControlModeButtonText)
	{
		ControlModeButtonText->SetText(DroneSelectionUI::GetControlModeText(SelectedControlMode));
	}
	if (HandlingPresetButtonText)
	{
		HandlingPresetButtonText->SetText(DroneSelectionUI::GetHandlingText(SelectedHandlingPreset));
	}
}

void UDroneSelectionWidget::SelectDisplayedButton(const int32 ButtonIndex)
{
	if (DisplayedDroneButtonIds.IsValidIndex(ButtonIndex)
		&& !DisplayedDroneButtonIds[ButtonIndex].IsNone())
	{
		SelectDrone(DisplayedDroneButtonIds[ButtonIndex]);
	}
}

void UDroneSelectionWidget::ClearFlowBinding()
{
	if (UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get())
	{
		Flow->OnFlowSnapshotChanged.RemoveDynamic(this, &UDroneSelectionWidget::HandleFlowSnapshotChanged);
	}
	FlowSubsystem.Reset();
}
