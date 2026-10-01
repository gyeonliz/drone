#include "UI/DroneFrontEndRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Mission/DroneMissionDefinition.h"
#include "Styling/CoreStyle.h"
#include "UI/DroneMissionSelectionButton.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"

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
	RefreshArtwork();
	if (LobbyBackButton) LobbyBackButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleBackClicked);
	if (BriefingBackButton) BriefingBackButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleBackClicked);
	if (TutorialTabButton) TutorialTabButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleTutorialTabClicked);
	if (RacingTabButton) RacingTabButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleRacingTabClicked);
	if (MissionTabButton) MissionTabButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleMissionTabClicked);
	if (ExitButton) ExitButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleExitClicked);
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
	if (LobbyBackButton) LobbyBackButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleBackClicked);
	if (BriefingBackButton) BriefingBackButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleBackClicked);
	if (TutorialTabButton) TutorialTabButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleTutorialTabClicked);
	if (RacingTabButton) RacingTabButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleRacingTabClicked);
	if (MissionTabButton) MissionTabButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleMissionTabClicked);
	if (ExitButton) ExitButton->OnClicked.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleExitClicked);
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
	return Flow && GetVisibleMissionIds().Contains(MissionId) && Flow->SelectMission(MissionId);
}

bool UDroneFrontEndRootWidget::ConfirmSelectedMission()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	// 탭 변경 전의 선택은 Flow에 남아 있어도 현재 탭에서 보이지 않으면 실행하지 않는다.
	return Flow && GetVisibleMissionIds().Contains(Flow->GetSnapshot().SelectedMissionId)
		&& Flow->ConfirmMissionSelection();
}

bool UDroneFrontEndRootWidget::SetLobbyCategory(const EDroneMissionCategory Category)
{
	if (Category == EDroneMissionCategory::Auto)
	{
		return false;
	}
	ActiveLobbyCategory = Category;
	RefreshLobbyContent();
	ReceiveLobbyCategoryChanged(Category);
	return true;
}

TArray<FName> UDroneFrontEndRootWidget::GetVisibleMissionIds() const
{
	TArray<FName> Result;
	if (const UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get())
	{
		for (const FName Id : Flow->GetRegisteredMissionIds())
		{
			const UDroneMissionDefinition* Definition = Flow->FindMissionDefinition(Id);
			if (Definition && Definition->GetLobbyCategory() == ActiveLobbyCategory) Result.Add(Id);
		}
	}
	return Result;
}

void UDroneFrontEndRootWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime())
	{
		BuildDefaultLayout();
		ApplyDisplayedState(EDroneGameFlowState::OpeningTrailer);
	}
	RefreshArtwork();
}

void UDroneFrontEndRootWidget::RefreshArtwork()
{
	auto Apply = [](UImage* Image, UTexture2D* Texture)
	{
		if (!Image) return;
		Image->SetBrushFromTexture(Texture);
		Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	};
	Apply(TitleBackgroundImage, TitleBackgroundTexture);
	Apply(TitleOverlayImage, TitleOverlayTexture);
	Apply(TitleLogoImage, TitleLogoTexture);
	if (OpeningTitleText) OpeningTitleText->SetVisibility(TitleLogoTexture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ApplyTitleButtonStyle(ContinueButton);
	ApplyTitleButtonStyle(ExitButton);
	// 시작 화면의 바로가기 버튼도 같은 Class Defaults를 사용한다.
	if (WidgetTree)
	{
		ApplyTitleButtonStyle(Cast<UButton>(WidgetTree->FindWidget(TEXT("TitleTutorialButton"))));
		ApplyTitleButtonStyle(Cast<UButton>(WidgetTree->FindWidget(TEXT("TitleSettingsButton"))));
	}
}

void UDroneFrontEndRootWidget::ApplyTitleButtonStyle(UButton* Button)
{
	if (!Button || !ButtonNormalTexture || !ButtonHoveredTexture || !ButtonPressedTexture) return;
	FButtonStyle Style = Button->GetStyle();
	auto Brush = [this](UTexture2D* Texture, const bool bNormal)
	{
		FSlateBrush Result;
		Result.SetResourceObject(Texture);
		Result.DrawAs = ESlateBrushDrawType::Image;
		Result.ImageSize = FVector2D(390.0, 90.0);
		if (bUseProvidedButtonAtlasRegions)
		{
			// PNG 자체를 훼손하지 않고 원본의 투명 여백만 UV로 제외한다.
			const float Top = bNormal ? 131.f : 171.f;
			const float Bottom = bNormal ? 218.f : 258.f;
			const float Left = bNormal ? 189.f : 173.f;
			Result.SetUVRegion(FBox2f(FVector2f(Left / 734.f, Top / 429.f), FVector2f((Left + 387.f) / 734.f, Bottom / 429.f)));
		}
		return Result;
	};
	// Unselect의 밑줄은 버튼 하단에 오도록 다른 세로 UV를 사용한다.
	Style.SetNormal(Brush(ButtonNormalTexture, true));
	Style.SetHovered(Brush(ButtonHoveredTexture, false));
	Style.SetPressed(Brush(ButtonPressedTexture, false));
	// 시안 텍스트는 오른쪽 장식 날개를 제외한 직사각형 부분의 중앙에 놓인다.
	Style.SetNormalPadding(FMargin(16.f, 10.f, 48.f, 10.f));
	Style.SetPressedPadding(FMargin(16.f, 12.f, 48.f, 8.f));
	Button->SetStyle(Style);
	Button->SetBackgroundColor(FLinearColor::White);
	Button->OnHovered.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleTitleHovered);
	Button->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleTitleClickFeedback);
}

void UDroneFrontEndRootWidget::HandleTitleHovered()
{
	if (ButtonHoverSound) UGameplayStatics::PlaySound2D(this, ButtonHoverSound);
}
void UDroneFrontEndRootWidget::HandleTitleClickFeedback()
{
	if (ButtonClickSound) UGameplayStatics::PlaySound2D(this, ButtonClickSound);
}
void UDroneFrontEndRootWidget::HandleSettingsClicked() { SetSettingsVisible(true); }
void UDroneFrontEndRootWidget::HandleSettingsBackClicked() { NavigateBack(); }
void UDroneFrontEndRootWidget::HandleLowQualityClicked() { ApplyGraphicsQuality(0); }
void UDroneFrontEndRootWidget::HandleMediumQualityClicked() { ApplyGraphicsQuality(1); }
void UDroneFrontEndRootWidget::HandleHighQualityClicked() { ApplyGraphicsQuality(2); }
void UDroneFrontEndRootWidget::SetSettingsVisible(const bool bVisible)
{
	bSettingsVisible = bVisible && DisplayedState == EDroneGameFlowState::OpeningTrailer;
	if (SettingsPanel) SettingsPanel->SetVisibility(bSettingsVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
void UDroneFrontEndRootWidget::ApplyGraphicsQuality(const int32 Quality)
{
	if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(Quality, 0, 3));
		Settings->ApplySettings(false);
	}
}

void UDroneFrontEndRootWidget::HandleTutorialTabClicked()
{
	SetLobbyCategory(EDroneMissionCategory::Tutorial);
	if (DisplayedState == EDroneGameFlowState::OpeningTrailer) FinishOpeningTrailer();
}
void UDroneFrontEndRootWidget::HandleRacingTabClicked()
{
	SetLobbyCategory(EDroneMissionCategory::Racing);
	if (DisplayedState == EDroneGameFlowState::OpeningTrailer) FinishOpeningTrailer();
}
void UDroneFrontEndRootWidget::HandleMissionTabClicked()
{
	SetLobbyCategory(EDroneMissionCategory::Mission);
}
void UDroneFrontEndRootWidget::HandleExitClicked()
{
	// PIE에서는 게임 종료 요청만 한다. Editor 프로세스 자체는 종료하지 않는다.
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UDroneFrontEndRootWidget::HandleBackClicked() { NavigateBack(); }

bool UDroneFrontEndRootWidget::NavigateBack()
{
	if (bSettingsVisible)
	{
		SetSettingsVisible(false);
		if (IsInViewport() && GetOwningPlayer()) SetUserFocus(GetOwningPlayer());
		return true;
	}
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	if (!Flow || (Flow->GetSnapshot().State != EDroneGameFlowState::MissionTrailer
		&& Flow->GetSnapshot().State != EDroneGameFlowState::LobbyMissionSelect)) return false;
	if (Flow->GetSnapshot().State == EDroneGameFlowState::MissionTrailer)
	{
		// 맵에서 돌아온 새 Widget도 선택한 미션의 탭을 보여야 선택이 숨지 않는다.
		if (const UDroneMissionDefinition* Mission = Flow->FindMissionDefinition(Flow->GetSnapshot().SelectedMissionId))
			SetLobbyCategory(Mission->GetLobbyCategory());
	}
	return Flow->RequestBackNavigation();
}

FReply UDroneFrontEndRootWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Virtual_Gamepad_Back.GetVirtualKey())
	{
		// 키를 누르고 있어도 여러 화면을 한 번에 건너뛰지 않는다. 자식 버튼 Focus에도 적용된다.
		if (InKeyEvent.IsRepeat() || NavigateBack()) return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
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

void UDroneFrontEndRootWidget::HandleMissionButtonSelected(const FName MissionId)
{
	SelectLobbyMission(MissionId);
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
	TitleBackgroundImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("TitleBackgroundImage")));
	TitleOverlayImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("TitleOverlayImage")));
	TitleLogoImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("TitleLogoImage")));
	MissionThumbnailImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("MissionThumbnailImage")));
	TutorialTabButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("TutorialTabButton")));
	RacingTabButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("RacingTabButton")));
	MissionTabButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("MissionTabButton")));
	ExitButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("ExitButton")));
	SettingsPanel = WidgetTree->FindWidget(TEXT("SettingsPanel"));
	LobbyBackButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("LobbyBackButton")));
	BriefingBackButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BriefingBackButton")));
	MissionButtonsColumn = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("MissionButtonsColumn")));
	return OpeningPanel
		&& LobbyPanel
		&& MissionBriefingPanel
		&& ContinueButton
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
	NativeOpeningPanel->SetPadding(FMargin(0.f));
	// 1920x1080 시안을 비율 유지해 축소한다. 다른 해상도에서도 로고/버튼 위치가 유지된다.
	UScaleBox* TitleScale = WidgetTree->ConstructWidget<UScaleBox>();
	TitleScale->SetStretch(EStretch::ScaleToFit);
	NativeOpeningPanel->SetContent(TitleScale);
	USizeBox* TitleSize = WidgetTree->ConstructWidget<USizeBox>();
	TitleSize->SetWidthOverride(1920.f);
	TitleSize->SetHeightOverride(1080.f);
	TitleScale->SetContent(TitleSize);
	UCanvasPanel* TitleCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	TitleSize->SetContent(TitleCanvas);
	auto AddTitleImage = [this, TitleCanvas](const FName Name, const FVector2D Position, const FVector2D Size)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		UCanvasPanelSlot* Slot = TitleCanvas->AddChildToCanvas(Image);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Image;
	};
	TitleBackgroundImage = AddTitleImage(TEXT("TitleBackgroundImage"), FVector2D::ZeroVector, FVector2D(1920, 1080));
	TitleOverlayImage = AddTitleImage(TEXT("TitleOverlayImage"), FVector2D::ZeroVector, FVector2D(1920, 1080));
	TitleLogoImage = AddTitleImage(TEXT("TitleLogoImage"), FVector2D(0, 0), FVector2D(734, 429));
	UVerticalBox* OpeningColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("OpeningColumn"));
	UCanvasPanelSlot* OpeningColumnSlot = TitleCanvas->AddChildToCanvas(OpeningColumn);
	OpeningColumnSlot->SetPosition(FVector2D(190, 417));
	OpeningColumnSlot->SetSize(FVector2D(385, 555));

	OpeningTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::OpeningTitleName);
	OpeningTitleText->SetText(FText::FromString(TEXT("PROJECT DRONER")));
	OpeningTitleText->SetJustification(ETextJustify::Center);
	OpeningTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	OpeningTitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 48.0f));
	OpeningColumn->AddChildToVerticalBox(OpeningTitleText);

	auto AddTitleButton = [this, OpeningColumn](const FName Name, const TCHAR* Caption)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Caption));
		Label->SetJustification(ETextJustify::Center);
		// Slate의 포인트 단위: 36pt는 기준 DPI에서 참고 이미지의 약 48px 글자에 해당한다.
		Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 36.f));
		Button->AddChild(Label);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetHeightOverride(90.f);
		Size->SetContent(Button);
		OpeningColumn->AddChildToVerticalBox(Size)->SetPadding(FMargin(0, 0, 0, 45));
		return Button;
	};
	ContinueButton = AddTitleButton(DroneFrontEndUI::ContinueButtonName, TEXT("Start"));
	UButton* TitleTutorialButton = AddTitleButton(TEXT("TitleTutorialButton"), TEXT("Training"));
	TitleTutorialButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleTutorialTabClicked);
	UButton* TitleSettingsButton = AddTitleButton(TEXT("TitleSettingsButton"), TEXT("Setting"));
	TitleSettingsButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleSettingsClicked);
	ExitButton = AddTitleButton(TEXT("ExitButton"), TEXT("Exit"));

	UBorder* NativeLobbyPanel = AddFullScreenPanel(
		DroneFrontEndUI::LobbyPanelName,
		FLinearColor(0.012f, 0.025f, 0.032f, 1.0f));
	LobbyPanel = NativeLobbyPanel;
	NativeLobbyPanel->SetPadding(FMargin(64.0f, 42.0f));
	UVerticalBox* LobbyColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("LobbyColumn"));
	NativeLobbyPanel->SetContent(LobbyColumn);
	LobbyBackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LobbyBackButton"));
	UTextBlock* LobbyBackText = WidgetTree->ConstructWidget<UTextBlock>();
	LobbyBackText->SetText(FText::FromString(TEXT("← 시작 화면  ·  Esc / 패드 B")));
	LobbyBackButton->SetContent(LobbyBackText);
	LobbyColumn->AddChildToVerticalBox(LobbyBackButton)->SetPadding(FMargin(0, 0, 0, 12));

	LobbyTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::LobbyTitleName);
	LobbyTitleText->SetText(FText::FromString(TEXT("MISSION CONTROL  /  작전 선택")));
	LobbyTitleText->SetJustification(ETextJustify::Left);
	LobbyTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	LobbyTitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 34.0f));
	if (UVerticalBoxSlot* TitleSlot = LobbyColumn->AddChildToVerticalBox(LobbyTitleText))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	LobbyStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::LobbyStatusName);
	LobbyStatusText->SetText(FText::FromString(
		TEXT("미션을 선택해 상세 정보를 확인하세요.")));
	LobbyStatusText->SetJustification(ETextJustify::Left);
	LobbyStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.85f, 1.0f)));
	if (UVerticalBoxSlot* StatusSlot = LobbyColumn->AddChildToVerticalBox(LobbyStatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));
	}
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	LobbyColumn->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0, 0, 0, 20));
	auto AddTab = [this, Tabs](const FName Name, const TCHAR* Caption)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Caption));
		Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24.f));
		Button->AddChild(Label);
		UHorizontalBoxSlot* Slot = Tabs->AddChildToHorizontalBox(Button);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(0, 0, 12, 0));
		return Button;
	};
	TutorialTabButton = AddTab(TEXT("TutorialTabButton"), TEXT("튜토리얼"));
	RacingTabButton = AddTab(TEXT("RacingTabButton"), TEXT("레이싱"));
	MissionTabButton = AddTab(TEXT("MissionTabButton"), TEXT("미션"));

	UHorizontalBox* MissionWorkspace = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("MissionWorkspace"));
	if (UVerticalBoxSlot* WorkspaceSlot = LobbyColumn->AddChildToVerticalBox(MissionWorkspace))
	{
		WorkspaceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	auto AddWorkspacePanel = [this, MissionWorkspace](const FName Name, const FLinearColor Color)
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Panel->SetBrushColor(Color);
		Panel->SetPadding(FMargin(24.0f));
		if (UHorizontalBoxSlot* Slot = MissionWorkspace->AddChildToHorizontalBox(Panel))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		}
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		return Column;
	};

	UVerticalBox* MissionListColumn = AddWorkspacePanel(
		TEXT("MissionListPanel"), FLinearColor(0.018f, 0.045f, 0.055f, 0.98f));
	UTextBlock* MissionListHeader = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("MissionListHeader"));
	MissionListHeader->SetText(FText::FromString(TEXT("작전 목록")));
	MissionListHeader->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18.0f));
	MissionListHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	MissionListColumn->AddChildToVerticalBox(MissionListHeader);

	MissionButtonsColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("MissionButtonsColumn"));
	UScrollBox* MissionScroll = WidgetTree->ConstructWidget<UScrollBox>();
	MissionScroll->AddChild(MissionButtonsColumn);
	if (UVerticalBoxSlot* MissionButtonSlot = MissionListColumn->AddChildToVerticalBox(MissionScroll))
	{
		MissionButtonSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
		MissionButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* MissionCardColumn = AddWorkspacePanel(
		TEXT("MissionCardPanel"), FLinearColor(0.025f, 0.055f, 0.065f, 0.98f));
	UTextBlock* MissionCardHeader = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("MissionCardHeader"));
	MissionCardHeader->SetText(FText::FromString(TEXT("선택 작전")));
	MissionCardHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.90f, 0.83f, 1.0f)));
	MissionCardColumn->AddChildToVerticalBox(MissionCardHeader);
	MissionThumbnailImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MissionThumbnailImage"));
	USizeBox* ThumbnailSize = WidgetTree->ConstructWidget<USizeBox>();
	ThumbnailSize->SetHeightOverride(180.f);
	UScaleBox* ThumbnailScale = WidgetTree->ConstructWidget<UScaleBox>();
	ThumbnailScale->SetStretch(EStretch::ScaleToFit);
	ThumbnailScale->SetContent(MissionThumbnailImage);
	ThumbnailSize->SetContent(ThumbnailScale);
	MissionCardColumn->AddChildToVerticalBox(ThumbnailSize);

	MissionNameText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionNameName);
	MissionNameText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24.0f));
	MissionNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.96f, 0.96f, 1.0f)));
	if (UVerticalBoxSlot* NameSlot = MissionCardColumn->AddChildToVerticalBox(MissionNameText))
	{
		NameSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 10.0f));
	}

	MissionMetaText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionMetaName);
	MissionMetaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	MissionCardColumn->AddChildToVerticalBox(MissionMetaText);

	UVerticalBox* MissionDetailColumn = AddWorkspacePanel(
		TEXT("MissionDetailPanel"), FLinearColor(0.014f, 0.034f, 0.043f, 0.98f));
	UTextBlock* MissionDetailHeader = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("MissionDetailHeader"));
	MissionDetailHeader->SetText(FText::FromString(TEXT("작전 개요")));
	MissionDetailHeader->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18.0f));
	MissionDetailHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.90f, 0.83f, 1.0f)));
	MissionDetailColumn->AddChildToVerticalBox(MissionDetailHeader);

	MissionDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionDescriptionName);
	MissionDescriptionText->SetAutoWrapText(true);
	MissionDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.85f, 1.0f)));
	if (UVerticalBoxSlot* DescriptionSlot = MissionDetailColumn->AddChildToVerticalBox(MissionDescriptionText))
	{
		DescriptionSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
		DescriptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	StartMissionButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		DroneFrontEndUI::StartMissionButtonName);
	UTextBlock* StartMissionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("StartMissionButtonText"));
	StartMissionText->SetText(FText::FromString(TEXT("미션 시작")));
	StartMissionText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18.0f));
	StartMissionButton->AddChild(StartMissionText);
	StartMissionButton->SetBackgroundColor(FLinearColor(0.08f, 0.65f, 0.56f, 1.0f));
	if (UVerticalBoxSlot* StartSlot = LobbyColumn->AddChildToVerticalBox(StartMissionButton))
	{
		StartSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
		StartSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// FLOW-04의 영상 Asset이 없어도 전체 진입 흐름을 시험할 수 있는 정적 Briefing 안전망이다.
	UBorder* NativeBriefingPanel = AddFullScreenPanel(
		DroneFrontEndUI::MissionBriefingPanelName,
		FLinearColor(0.008f, 0.018f, 0.025f, 1.0f));
	MissionBriefingPanel = NativeBriefingPanel;
	NativeBriefingPanel->SetPadding(FMargin(96.0f, 72.0f));
	UVerticalBox* BriefingColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("MissionBriefingColumn"));
	NativeBriefingPanel->SetContent(BriefingColumn);
	BriefingBackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BriefingBackButton"));
	UTextBlock* BriefingBackText = WidgetTree->ConstructWidget<UTextBlock>();
	BriefingBackText->SetText(FText::FromString(TEXT("← 미션 선택  ·  Esc / 패드 B")));
	BriefingBackButton->SetContent(BriefingBackText);
	BriefingColumn->AddChildToVerticalBox(BriefingBackButton)->SetPadding(FMargin(0, 0, 0, 20));

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

	// 시안의 Setting을 다른 메뉴로 바꾸지 않는다. 최소 기능은 UE 그래픽 품질 설정이다.
	UBorder* NativeSettingsPanel = AddFullScreenPanel(TEXT("SettingsPanel"), FLinearColor(0.008f, 0.018f, 0.025f, 0.97f));
	SettingsPanel = NativeSettingsPanel;
	NativeSettingsPanel->SetPadding(FMargin(140.f, 100.f));
	UVerticalBox* SettingsColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	NativeSettingsPanel->SetContent(SettingsColumn);
	UTextBlock* SettingsTitle = WidgetTree->ConstructWidget<UTextBlock>();
	SettingsTitle->SetText(FText::FromString(TEXT("그래픽 품질 — 동일한 맵에서 성능을 비교하세요")));
	SettingsTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28.f));
	SettingsColumn->AddChildToVerticalBox(SettingsTitle);
	auto AddSettingButton = [this, SettingsColumn](const FName Name, const TCHAR* Caption)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Caption));
		Button->SetContent(Label);
		SettingsColumn->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 16, 0, 16));
		return Button;
	};
	AddSettingButton(TEXT("LowQualityButton"), TEXT("낮음"))->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleLowQualityClicked);
	AddSettingButton(TEXT("MediumQualityButton"), TEXT("중간"))->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleMediumQualityClicked);
	AddSettingButton(TEXT("HighQualityButton"), TEXT("높음"))->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleHighQualityClicked);
	AddSettingButton(TEXT("SettingsBackButton"), TEXT("돌아가기"))->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleSettingsBackClicked);
	SetSettingsVisible(false);
	RefreshLobbyContent();
	RefreshMissionBriefingContent();
}

void UDroneFrontEndRootWidget::ApplyDisplayedState(const EDroneGameFlowState State)
{
	DisplayedState = State;
	if (State != EDroneGameFlowState::OpeningTrailer) SetSettingsVisible(false);
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
	// 사라진 화면의 버튼에 Focus가 남지 않게 한다. 다음 Esc도 새 화면에서 처리한다.
	if (IsInViewport() && GetOwningPlayer()
		&& (State == EDroneGameFlowState::OpeningTrailer || State == EDroneGameFlowState::LobbyMissionSelect
			|| State == EDroneGameFlowState::MissionTrailer)) SetUserFocus(GetOwningPlayer());
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
		if (!Mission->ObjectiveRules.IsEmpty())
		{
			Body += TEXT("\n\n목표 순서");
			for (const FDroneMissionObjectiveRule& Rule : Mission->ObjectiveRules)
			{
				Body += FString::Printf(TEXT("\n- %s (제한 %.0f초)"), *Rule.Description.ToString(), Rule.TimeLimitSeconds);
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
	const TArray<FName> MissionIds = GetVisibleMissionIds();
	FirstDisplayedMissionId = MissionIds.IsEmpty() ? NAME_None : MissionIds[0];
	if (MissionButtonsColumn)
	{
		RebuildNativeMissionButtons(MissionIds);
	}
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
	if (!MissionIds.Contains(SelectedMissionId)) SelectedMission = nullptr;
	const FLinearColor SelectedTabColor(0.05f, 0.65f, 0.6f);
	const FLinearColor OtherTabColor(0.06f, 0.15f, 0.2f);
	if (TutorialTabButton) TutorialTabButton->SetBackgroundColor(ActiveLobbyCategory == EDroneMissionCategory::Tutorial ? SelectedTabColor : OtherTabColor);
	if (RacingTabButton) RacingTabButton->SetBackgroundColor(ActiveLobbyCategory == EDroneMissionCategory::Racing ? SelectedTabColor : OtherTabColor);
	if (MissionTabButton) MissionTabButton->SetBackgroundColor(ActiveLobbyCategory == EDroneMissionCategory::Mission ? SelectedTabColor : OtherTabColor);
	if (LobbyStatusText) LobbyStatusText->SetText(FText::FromString(FString::Printf(TEXT("현재 탭: %d개 항목 · 목록에서 선택 후 시작"), MissionIds.Num())));
	if (MissionThumbnailImage)
	{
		UTexture2D* Thumbnail = SelectedMission ? SelectedMission->Thumbnail.LoadSynchronous() : nullptr;
		MissionThumbnailImage->SetBrushFromTexture(Thumbnail, true);
		MissionThumbnailImage->SetVisibility(Thumbnail ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
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

void UDroneFrontEndRootWidget::RebuildNativeMissionButtons(const TArray<FName>& MissionIds)
{
	if (!WidgetTree || !MissionButtonsColumn)
	{
		return;
	}

	MissionButtonsColumn->ClearChildren();
	NativeMissionButtons.Reset();
	MissionSelectButton = nullptr;
	MissionSelectButtonText = nullptr;

	if (MissionIds.IsEmpty())
	{
		MissionSelectButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), DroneFrontEndUI::MissionSelectButtonName);
		MissionSelectButtonText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), DroneFrontEndUI::MissionSelectButtonTextName);
		MissionSelectButtonText->SetText(FText::FromString(TEXT("등록된 미션 없음")));
		MissionSelectButton->SetIsEnabled(false);
		MissionSelectButton->AddChild(MissionSelectButtonText);
		MissionButtonsColumn->AddChildToVerticalBox(MissionSelectButton);
		return;
	}

	for (int32 Index = 0; Index < MissionIds.Num(); ++Index)
	{
		const FName MissionId = MissionIds[Index];
		UDroneMissionDefinition* Mission = FlowSubsystem.IsValid()
			? FlowSubsystem->FindMissionDefinition(MissionId)
			: nullptr;
		UDroneMissionSelectionButton* Button = WidgetTree->ConstructWidget<UDroneMissionSelectionButton>(
			UDroneMissionSelectionButton::StaticClass(),
			FName(*FString::Printf(TEXT("MissionSelectButton_%d"), Index)));
		Button->InitializeMissionSelection(MissionId);
		Button->OnMissionSelectionRequested.AddUniqueDynamic(
			this, &UDroneFrontEndRootWidget::HandleMissionButtonSelected);
		Button->SetBackgroundColor(FlowSubsystem.IsValid() && FlowSubsystem->GetSnapshot().SelectedMissionId == MissionId
			? FLinearColor(0.08f, 0.5f, 0.44f, 1.f) : FLinearColor(0.04f, 0.16f, 0.18f, 1.0f));

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("MissionSelectButtonText_%d"), Index)));
		Label->SetText(Mission ? Mission->DisplayName : FText::FromName(MissionId));
		Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16.0f));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.97f, 0.97f, 1.0f)));
		Button->AddChild(Label);
		if (UVerticalBoxSlot* ButtonSlot = MissionButtonsColumn->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
		NativeMissionButtons.Add(Button);
		if (Index == 0)
		{
			MissionSelectButton = Button;
			MissionSelectButtonText = Label;
		}
	}
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
