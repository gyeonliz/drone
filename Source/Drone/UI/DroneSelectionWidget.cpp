#include "UI/DroneSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Mission/DroneDefinition.h"
#include "Mission/DroneMissionDefinition.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "InputCoreTypes.h"

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
constexpr int32 MaximumDroneButtons = 5;
const FLinearColor Accent(0.20f, 0.95f, 0.82f, 1.0f);
const FLinearColor BodyTextColor(0.78f, 0.87f, 0.89f, 1.0f);

FText GetRoleText(const EDroneMissionRole Role)
{
	switch (Role)
	{
	case EDroneMissionRole::DropDelivery: return FText::FromString(TEXT("DROP  /  물자 투하"));
	case EDroneMissionRole::FPVStrike: return FText::FromString(TEXT("FPV  /  충돌 타격"));
	case EDroneMissionRole::FiberOpticStrike: return FText::FromString(TEXT("FIBER  /  광섬유"));
	case EDroneMissionRole::GroundUGV: return FText::FromString(TEXT("UGV  /  지상 주행"));
	case EDroneMissionRole::LongRangeStrike: return FText::FromString(TEXT("STRIKE  /  장거리 타격"));
	case EDroneMissionRole::Reconnaissance:
	default: return FText::FromString(TEXT("RECON  /  정찰"));
	}
}

// Definition에는 2D Thumbnail이 없으므로 기체 역할을 보여 주는 가벼운 도식이다.
// 실제 Pawn을 Spawn하지 않아 선택 화면에서 물리·미션 로직이 실행되지 않는다.
UWidget* BuildRoleSchematic(UWidgetTree* Tree, const bool bGround)
{
	USizeBox* Design = Tree->ConstructWidget<USizeBox>();
	Design->SetWidthOverride(400.0f);
	Design->SetHeightOverride(240.0f);
	UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>();
	Design->SetContent(Canvas);
	auto AddShape = [Tree, Canvas](const FVector2D Position, const FVector2D Size, const float Angle,
		const float Radius, const FLinearColor Fill, const FLinearColor Outline, const float OutlineWidth)
	{
		UBorder* Shape = Tree->ConstructWidget<UBorder>();
		Shape->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth));
		Shape->SetPadding(FMargin(0.0f));
		Shape->SetRenderTransformAngle(Angle);
		Shape->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Shape);
		Slot->SetAlignment(FVector2D(0.5f, 0.5f));
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	};
	const FLinearColor Fill(0.045f, 0.15f, 0.18f, 1.0f);
	const FLinearColor Frame(0.36f, 0.73f, 0.72f, 1.0f);
	if (bGround)
	{
		AddShape(FVector2D(112, 120), FVector2D(50, 194), 0, 20, Fill, Frame, 3);
		AddShape(FVector2D(288, 120), FVector2D(50, 194), 0, 20, Fill, Frame, 3);
		AddShape(FVector2D(200, 130), FVector2D(156, 140), 0, 14, Fill, Frame, 3);
		AddShape(FVector2D(200, 101), FVector2D(66, 66), 0, 33, Fill, Accent, 3);
		AddShape(FVector2D(200, 54), FVector2D(12, 68), 0, 4, Accent, Accent, 0);
	}
	else
	{
		AddShape(FVector2D(200, 120), FVector2D(298, 12), 34, 6, Frame, Frame, 0);
		AddShape(FVector2D(200, 120), FVector2D(298, 12), -34, 6, Frame, Frame, 0);
		for (const FVector2D Rotor : { FVector2D(76, 36), FVector2D(324, 36), FVector2D(76, 204), FVector2D(324, 204) })
		{
			AddShape(Rotor, FVector2D(70, 70), 0, 35, Fill, Accent, 3);
			AddShape(Rotor, FVector2D(42, 5), 0, 2, Frame, Frame, 0);
			AddShape(Rotor, FVector2D(5, 42), 0, 2, Frame, Frame, 0);
		}
		AddShape(FVector2D(200, 120), FVector2D(64, 106), 0, 18, Fill, Accent, 3);
		AddShape(FVector2D(200, 75), FVector2D(30, 14), 0, 5, Accent, Accent, 0);
		AddShape(FVector2D(200, 116), FVector2D(28, 34), 0, 8, Fill, Frame, 2);
	}
	return Design;
}

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
	case EDroneControlMode::AcroRateMode1Greybox:
		return FText::FromString(TEXT("조작 3: FPV Rate/Acro · 송신기 Mode 1"));
	case EDroneControlMode::AcroRateRealisticGreybox:
		return FText::FromString(TEXT("조작 4: FPV Rate/Acro · 송신기 Mode 2"));
	case EDroneControlMode::AssistedEasy:
	default:
		return FText::FromString(TEXT("조작: 쉬운 조작"));
	}
}

}

void UDroneSelectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildDefaultLayout();
	if (SelectionBackButton) SelectionBackButton->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleBackClicked);

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
	if (DroneButtons.IsValidIndex(3) && DroneButtons[3])
	{
		DroneButtons[3]->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleDroneButton3Clicked);
	}
	if (DroneButtons.IsValidIndex(4) && DroneButtons[4])
	{
		DroneButtons[4]->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleDroneButton4Clicked);
	}
	if (ControlModeButton)
	{
		ControlModeButton->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleControlModeClicked);
	}
	if (LaunchDroneButton)
	{
		LaunchDroneButton->OnClicked.AddUniqueDynamic(this, &UDroneSelectionWidget::HandleLaunchClicked);
	}
	RefreshFromFlow();
}

void UDroneSelectionWidget::NativeDestruct()
{
	if (SelectionBackButton) SelectionBackButton->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleBackClicked);
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
	if (DroneButtons.IsValidIndex(3) && DroneButtons[3])
	{
		DroneButtons[3]->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleDroneButton3Clicked);
	}
	if (DroneButtons.IsValidIndex(4) && DroneButtons[4])
	{
		DroneButtons[4]->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleDroneButton4Clicked);
	}
	if (ControlModeButton)
	{
		ControlModeButton->OnClicked.RemoveDynamic(this, &UDroneSelectionWidget::HandleControlModeClicked);
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
		SelectedControlMode = EDroneControlMode::AcroRateMode1Greybox;
		break;
	case EDroneControlMode::AcroRateMode1Greybox:
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
	// 느림/보통/빠름 선택은 폐기했다. Blueprint의 기존 호출은 단일 기본 성능으로 흡수한다.
	SelectedHandlingPreset = EDroneHandlingPreset::Balanced;
	RefreshControlLabels();
}

bool UDroneSelectionWidget::ConfirmAndLaunchSelectedDrone()
{
	ADroneMissionPlayerController* MissionController = Cast<ADroneMissionPlayerController>(GetOwningPlayer());
	return MissionController
		&& MissionController->StartSelectedDrone(SelectedControlMode, EDroneHandlingPreset::Balanced);
}

bool UDroneSelectionWidget::NavigateBack()
{
	ADroneMissionPlayerController* Controller = Cast<ADroneMissionPlayerController>(GetOwningPlayer());
	return Controller && Controller->BackToMissionBriefing();
}

void UDroneSelectionWidget::HandleBackClicked() { NavigateBack(); }

FReply UDroneSelectionWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Virtual_Gamepad_Back.GetVirtualKey())
	{
		if (InKeyEvent.IsRepeat() || NavigateBack()) return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
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

void UDroneSelectionWidget::HandleDroneButton3Clicked()
{
	SelectDisplayedButton(3);
}

void UDroneSelectionWidget::HandleDroneButton4Clicked()
{
	SelectDisplayedButton(4);
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
	for (int32 Index = 0; Index < DroneSelectionUI::MaximumDroneButtons; ++Index)
	{
		DroneButtons.Add(Cast<UButton>(WidgetTree->FindWidget(DroneSelectionUI::GetDroneButtonName(Index))));
		DroneButtonTexts.Add(Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::GetDroneButtonTextName(Index))));
	}
	ControlModeButton = Cast<UButton>(WidgetTree->FindWidget(DroneSelectionUI::ControlModeButtonName));
	ControlModeButtonText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::ControlModeButtonTextName));
	HandlingPresetButton = Cast<UButton>(WidgetTree->FindWidget(DroneSelectionUI::HandlingPresetButtonName));
	HandlingPresetButtonText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneSelectionUI::HandlingPresetButtonTextName));
	if (HandlingPresetButton)
	{
		HandlingPresetButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (HandlingPresetButtonText)
	{
		HandlingPresetButtonText->SetVisibility(ESlateVisibility::Collapsed);
	}
	LaunchDroneButton = Cast<UButton>(WidgetTree->FindWidget(DroneSelectionUI::LaunchButtonName));
	SelectionBackButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("SelectionBackButton")));
	return DroneSelectionPanel
		&& MissionNameText
		&& DroneNameText
		&& DroneDescriptionText
		&& DroneProfileText
		&& DroneButtons.Num() == DroneSelectionUI::MaximumDroneButtons
		&& DroneButtons[0]
		&& DroneButtons[1]
		&& DroneButtons[2]
		&& DroneButtons[3]
		&& DroneButtons[4]
		&& DroneButtonTexts[0]
		&& DroneButtonTexts[1]
		&& DroneButtonTexts[2]
		&& DroneButtonTexts[3]
		&& DroneButtonTexts[4]
		&& ControlModeButton
		&& ControlModeButtonText
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
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.008f, 0.018f, 0.025f, 1.0f));
	UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.0f));
	UScaleBox* ScreenScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("SelectionScreenScale"));
	ScreenScale->SetStretch(EStretch::ScaleToFit);
	UCanvasPanelSlot* ScaleSlot = RootCanvas->AddChildToCanvas(ScreenScale);
	ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ScaleSlot->SetOffsets(FMargin(0.0f));
	USizeBox* ScreenDesign = WidgetTree->ConstructWidget<USizeBox>();
	ScreenDesign->SetWidthOverride(1920.0f);
	ScreenDesign->SetHeightOverride(1080.0f);
	ScreenScale->SetContent(ScreenDesign);
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), DroneSelectionUI::PanelName);
	Panel->SetBrushColor(FLinearColor(0.008f, 0.018f, 0.025f, 1.0f));
	Panel->SetPadding(FMargin(72.0f, 40.0f));
	ScreenDesign->SetContent(Panel);
	DroneSelectionPanel = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SelectionColumn"));
	Panel->SetContent(Column);
	auto MakeText = [this](const FName Name, const FText& Text, const int32 FontSize,
		const FLinearColor Color, const bool bBold = false)
	{
		UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(Text);
		Result->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), FontSize));
		Result->SetColorAndOpacity(FSlateColor(Color));
		Result->SetAutoWrapText(true);
		return Result;
	};
	auto StyleButton = [](UButton* Button, const bool bPrimary)
	{
		FButtonStyle Style = Button->GetStyle();
		const FLinearColor Normal = bPrimary ? FLinearColor(0.08f, 0.65f, 0.56f, 1.0f) : FLinearColor(0.035f, 0.085f, 0.105f, 1.0f);
		Style.SetNormal(FSlateRoundedBoxBrush(Normal, 6.0f));
		Style.SetHovered(FSlateRoundedBoxBrush(bPrimary ? DroneSelectionUI::Accent : FLinearColor(0.06f, 0.20f, 0.22f, 1.0f), 6.0f));
		Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.04f, 0.34f, 0.31f, 1.0f), 6.0f));
		Style.SetNormalPadding(FMargin(20.0f, 14.0f));
		Style.SetPressedPadding(FMargin(20.0f, 16.0f, 20.0f, 12.0f));
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::White);
	};
	UHorizontalBox* NavigationRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(NavigationRow)->SetPadding(FMargin(0, 0, 0, 20));
	SelectionBackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectionBackButton"));
	UTextBlock* BackText = MakeText(NAME_None, FText::FromString(TEXT("← 미션 설명  ·  Esc / 패드 B")), 22, DroneSelectionUI::BodyTextColor);
	SelectionBackButton->SetContent(BackText);
	StyleButton(SelectionBackButton, false);
	NavigationRow->AddChildToHorizontalBox(SelectionBackButton);
	UTextBlock* StepText = MakeText(NAME_None, FText::FromString(TEXT("임무 선택  /  임무 설명  /  기체 선택")), 22, DroneSelectionUI::BodyTextColor);
	UHorizontalBoxSlot* StepSlot = NavigationRow->AddChildToHorizontalBox(StepText);
	StepSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StepSlot->SetHorizontalAlignment(HAlign_Right);
	StepSlot->SetVerticalAlignment(VAlign_Center);
	MissionNameText = MakeText(DroneSelectionUI::MissionNameName, FText::FromString(TEXT("기체 선택")), 38, DroneSelectionUI::Accent, true);
	Column->AddChildToVerticalBox(MissionNameText)->SetPadding(FMargin(0, 0, 0, 24));
	UHorizontalBox* Workspace = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("DroneSelectionWorkspace"));
	UVerticalBoxSlot* WorkspaceSlot = Column->AddChildToVerticalBox(Workspace);
	WorkspaceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	WorkspaceSlot->SetPadding(FMargin(0, 0, 0, 24));
	auto AddSelectionPanel = [this, Workspace](const FName Name, const float Weight, const float RightPadding)
	{
		UBorder* Section = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Section->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.018f, 0.045f, 0.055f, 1.0f), 10.0f,
			FLinearColor(0.06f, 0.17f, 0.19f, 1.0f), 1.0f));
		Section->SetPadding(FMargin(28.0f));
		UHorizontalBoxSlot* Slot = Workspace->AddChildToHorizontalBox(Section);
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Weight;
		Slot->SetSize(Size);
		Slot->SetPadding(FMargin(0, 0, RightPadding, 0));
		UVerticalBox* SectionColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Section->SetContent(SectionColumn);
		return SectionColumn;
	};
	UVerticalBox* PreviewColumn = AddSelectionPanel(TEXT("DronePreviewPanel"), 1.1f, 24.0f);
	UTextBlock* PreviewHeader = MakeText(NAME_None, FText::FromString(TEXT("AIRFRAME  /  기체 역할 프리뷰")), 22, DroneSelectionUI::BodyTextColor, true);
	PreviewColumn->AddChildToVerticalBox(PreviewHeader);
	UOverlay* PreviewLayers = WidgetTree->ConstructWidget<UOverlay>();
	DroneAirframePreview = DroneSelectionUI::BuildRoleSchematic(WidgetTree, false);
	DroneGroundPreview = DroneSelectionUI::BuildRoleSchematic(WidgetTree, true);
	PreviewLayers->AddChild(DroneAirframePreview);
	PreviewLayers->AddChild(DroneGroundPreview);
	UScaleBox* PreviewScale = WidgetTree->ConstructWidget<UScaleBox>();
	PreviewScale->SetStretch(EStretch::ScaleToFit);
	PreviewScale->SetContent(PreviewLayers);
	UVerticalBoxSlot* PreviewSlot = PreviewColumn->AddChildToVerticalBox(PreviewScale);
	PreviewSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	PreviewSlot->SetPadding(FMargin(32, 24, 32, 24));
	DronePreviewRoleText = MakeText(NAME_None, FText::GetEmpty(), 24, DroneSelectionUI::Accent, true);
	DronePreviewRoleText->SetJustification(ETextJustify::Center);
	PreviewColumn->AddChildToVerticalBox(DronePreviewRoleText)->SetPadding(FMargin(0, 0, 0, 16));
	LaunchDroneButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneSelectionUI::LaunchButtonName);
	UTextBlock* LaunchText = MakeText(TEXT("LaunchDroneButtonText"), FText::FromString(TEXT("선택 기체 출격  →")), 26, FLinearColor(0.006f, 0.023f, 0.024f, 1.0f), true);
	LaunchText->SetJustification(ETextJustify::Center);
	LaunchDroneButton->SetContent(LaunchText);
	StyleButton(LaunchDroneButton, true);
	USizeBox* LaunchSize = WidgetTree->ConstructWidget<USizeBox>();
	LaunchSize->SetHeightOverride(66.0f);
	LaunchSize->SetContent(LaunchDroneButton);
	PreviewColumn->AddChildToVerticalBox(LaunchSize);
	UVerticalBox* DroneDetailColumn = AddSelectionPanel(TEXT("DroneDetailPanel"), 1.0f, 0.0f);
	DroneNameText = MakeText(DroneSelectionUI::DroneNameName, FText::GetEmpty(), 34, FLinearColor::White, true);
	DroneDetailColumn->AddChildToVerticalBox(DroneNameText);
	UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DroneDetailScroll"));
	DetailScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	UVerticalBox* DetailBody = WidgetTree->ConstructWidget<UVerticalBox>();
	DetailScroll->AddChild(DetailBody);
	UVerticalBoxSlot* DetailScrollSlot = DroneDetailColumn->AddChildToVerticalBox(DetailScroll);
	DetailScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DetailScrollSlot->SetPadding(FMargin(0, 20, 0, 20));
	DroneDescriptionText = MakeText(DroneSelectionUI::DroneDescriptionName, FText::GetEmpty(), 24, DroneSelectionUI::BodyTextColor);
	DetailBody->AddChildToVerticalBox(DroneDescriptionText)->SetPadding(FMargin(0, 0, 16, 24));
	DroneProfileText = MakeText(DroneSelectionUI::DroneProfileName, FText::GetEmpty(), 22, DroneSelectionUI::Accent);
	DetailBody->AddChildToVerticalBox(DroneProfileText)->SetPadding(FMargin(0, 0, 16, 0));
	UTextBlock* ControlHeader = MakeText(TEXT("DroneControlHeader"), FText::FromString(TEXT("조작 설정")), 22, DroneSelectionUI::BodyTextColor, true);
	DroneDetailColumn->AddChildToVerticalBox(ControlHeader)->SetPadding(FMargin(0, 0, 0, 10));
	ControlModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneSelectionUI::ControlModeButtonName);
	ControlModeButtonText = MakeText(DroneSelectionUI::ControlModeButtonTextName, FText::GetEmpty(), 24, FLinearColor::White, true);
	ControlModeButton->SetContent(ControlModeButtonText);
	StyleButton(ControlModeButton, false);
	DroneDetailColumn->AddChildToVerticalBox(ControlModeButton);
	UTextBlock* ControlHint = MakeText(NAME_None, FText::FromString(TEXT("버튼을 눌러 조작 방식을 변경하세요.")), 20, DroneSelectionUI::BodyTextColor);
	DroneDetailColumn->AddChildToVerticalBox(ControlHint)->SetPadding(FMargin(0, 10, 0, 0));
	UTextBlock* ListHeader = MakeText(TEXT("DroneListHeader"), FText::FromString(TEXT("임무에 사용할 기체 선택")), 24, FLinearColor::White, true);
	Column->AddChildToVerticalBox(ListHeader)->SetPadding(FMargin(0, 0, 0, 14));
	USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
	ListSize->SetHeightOverride(248.0f);
	UHorizontalBox* DroneList = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DroneListPanel"));
	ListSize->SetContent(DroneList);
	Column->AddChildToVerticalBox(ListSize);
	DroneButtons.Reset();
	DroneButtonTexts.Reset();
	DroneCardBorders.Reset();
	DroneCardRoleTexts.Reset();
	DroneCardSelectionTexts.Reset();
	DroneCardAirframes.Reset();
	DroneCardGroundFrames.Reset();
	for (int32 Index = 0; Index < DroneSelectionUI::MaximumDroneButtons; ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), DroneSelectionUI::GetDroneButtonName(Index));
		StyleButton(Button, false);
		FButtonStyle CardButtonStyle = Button->GetStyle();
		CardButtonStyle.SetNormalPadding(FMargin(0));
		CardButtonStyle.SetPressedPadding(FMargin(0));
		Button->SetStyle(CardButtonStyle);
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		Card->SetPadding(FMargin(18.0f, 12.0f));
		Button->SetContent(Card);
		UVerticalBox* CardColumn = WidgetTree->ConstructWidget<UVerticalBox>();
		Card->SetContent(CardColumn);
		UTextBlock* SelectionText = MakeText(NAME_None, FText::FromString(TEXT("선택")), 18, DroneSelectionUI::Accent, true);
		SelectionText->SetJustification(ETextJustify::Right);
		CardColumn->AddChildToVerticalBox(SelectionText);
		UOverlay* CardPreview = WidgetTree->ConstructWidget<UOverlay>();
		UWidget* Airframe = DroneSelectionUI::BuildRoleSchematic(WidgetTree, false);
		UWidget* GroundFrame = DroneSelectionUI::BuildRoleSchematic(WidgetTree, true);
		CardPreview->AddChild(Airframe);
		CardPreview->AddChild(GroundFrame);
		UScaleBox* CardPreviewScale = WidgetTree->ConstructWidget<UScaleBox>();
		CardPreviewScale->SetStretch(EStretch::ScaleToFit);
		CardPreviewScale->SetContent(CardPreview);
		UVerticalBoxSlot* CardPreviewSlot = CardColumn->AddChildToVerticalBox(CardPreviewScale);
		CardPreviewSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardPreviewSlot->SetPadding(FMargin(16, 4, 16, 10));
		UTextBlock* ButtonText = MakeText(DroneSelectionUI::GetDroneButtonTextName(Index), FText::GetEmpty(), 24, FLinearColor::White, true);
		ButtonText->SetJustification(ETextJustify::Center);
		CardColumn->AddChildToVerticalBox(ButtonText);
		UTextBlock* RoleText = MakeText(NAME_None, FText::GetEmpty(), 20, DroneSelectionUI::BodyTextColor);
		RoleText->SetJustification(ETextJustify::Center);
		CardColumn->AddChildToVerticalBox(RoleText)->SetPadding(FMargin(0, 6, 0, 0));
		UHorizontalBoxSlot* ButtonSlot = DroneList->AddChildToHorizontalBox(Button);
		ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ButtonSlot->SetPadding(FMargin(0, 0, Index + 1 < DroneSelectionUI::MaximumDroneButtons ? 16.0f : 0.0f, 0));
		DroneButtons.Add(Button);
		DroneButtonTexts.Add(ButtonText);
		DroneCardBorders.Add(Card);
		DroneCardRoleTexts.Add(RoleText);
		DroneCardSelectionTexts.Add(SelectionText);
		DroneCardAirframes.Add(Airframe);
		DroneCardGroundFrames.Add(GroundFrame);
	}
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
	if (bUsingNativeFallbackLayout && WidgetTree && WidgetTree->RootWidget)
	{
		// Letterbox 배경도 선택 화면과 함께 숨겨 Spawn 실패·뒤로가기 상태에 남지 않게 한다.
		WidgetTree->RootWidget->SetVisibility(Snapshot.State == EDroneGameFlowState::DroneSelect
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
		const bool bSelected = Definition && Definition->DroneId == Snapshot.SelectedDroneId;
		const bool bGround = Definition && Definition->MissionRole == EDroneMissionRole::GroundUGV;
		if (DroneCardBorders.IsValidIndex(Index) && DroneCardBorders[Index])
		{
			DroneCardBorders[Index]->SetBrush(FSlateRoundedBoxBrush(
				bSelected ? FLinearColor(0.035f, 0.15f, 0.17f, 0.90f) : FLinearColor(0.025f, 0.065f, 0.08f, 0.78f),
				6.0f, bSelected ? DroneSelectionUI::Accent : FLinearColor(0.07f, 0.20f, 0.23f, 1.0f), bSelected ? 3.0f : 1.0f));
		}
		if (DroneCardRoleTexts.IsValidIndex(Index) && DroneCardRoleTexts[Index])
		{
			DroneCardRoleTexts[Index]->SetText(Definition ? DroneSelectionUI::GetRoleText(Definition->MissionRole) : FText::GetEmpty());
		}
		if (DroneCardSelectionTexts.IsValidIndex(Index) && DroneCardSelectionTexts[Index])
		{
			DroneCardSelectionTexts[Index]->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		if (DroneCardAirframes.IsValidIndex(Index) && DroneCardAirframes[Index])
		{
			DroneCardAirframes[Index]->SetVisibility(Definition && !bGround ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (DroneCardGroundFrames.IsValidIndex(Index) && DroneCardGroundFrames[Index])
		{
			DroneCardGroundFrames[Index]->SetVisibility(bGround ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	UDroneDefinition* SelectedDefinition = Flow ? Flow->GetSelectedDroneDefinition() : nullptr;
	const FName NewDroneId = SelectedDefinition ? SelectedDefinition->DroneId : NAME_None;
	if (NewDroneId != DisplayedDroneId)
	{
		DisplayedDroneId = NewDroneId;
		if (UScrollBox* DetailScroll = WidgetTree ? Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("DroneDetailScroll"))) : nullptr)
		{
			DetailScroll->ScrollToStart();
		}
		if (SelectedDefinition)
		{
			SelectedControlMode = SelectedDefinition->FlightProfile.DefaultControlMode;
			SelectedHandlingPreset = EDroneHandlingPreset::Balanced;
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
	const bool bSelectedGround = SelectedDefinition && SelectedDefinition->MissionRole == EDroneMissionRole::GroundUGV;
	if (DroneAirframePreview)
	{
		DroneAirframePreview->SetVisibility(SelectedDefinition && !bSelectedGround ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DroneGroundPreview)
	{
		DroneGroundPreview->SetVisibility(bSelectedGround ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DronePreviewRoleText)
	{
		DronePreviewRoleText->SetText(SelectedDefinition ? DroneSelectionUI::GetRoleText(SelectedDefinition->MissionRole) : FText::FromString(TEXT("아래에서 기체를 선택하세요")));
	}
	if (DroneProfileText)
	{
		FString Profile;
		if (SelectedDefinition)
		{
			Profile = FString::Printf(
				TEXT("무적재 최고 속도   %.0f km/h\n기체 질량   %.2f kg   ·   기본 체력   %.0f\n"),
				SelectedDefinition->FlightProfile.MaxSpeedCentimetersPerSecond
					* SelectedDefinition->FlightProfile.PhysicalFlightSettings.UnloadedMaximumSpeedMultiplier * 0.036f,
				SelectedDefinition->FlightProfile.PhysicalFlightSettings.DryMassKilograms,
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
	SelectedHandlingPreset = EDroneHandlingPreset::Balanced;
	if (HandlingPresetButton)
	{
		HandlingPresetButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (HandlingPresetButtonText)
	{
		HandlingPresetButtonText->SetVisibility(ESlateVisibility::Collapsed);
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
