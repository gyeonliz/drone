#include "UI/DroneFrontEndRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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
#include "UI/DroneSettingsWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
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

void SetButtonPadding(UButton* Button, const FMargin Padding)
{
	FButtonStyle Style = Button->GetStyle();
	Style.SetNormalPadding(Padding);
	Style.SetPressedPadding(Padding);
	Button->SetStyle(Style);
}
}

void UDroneFrontEndRootWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// UIOnly 입력 모드가 Root에 안전하게 Focus를 줄 수 있게 한다.
	SetIsFocusable(true);
	BuildDefaultLayout();
	RefreshArtwork();
	if (SettingsWidget) SettingsWidget->OnCloseRequested.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleSettingsBackClicked);
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
	StopBriefingLines();
	if (SettingsWidget)
	{
		SettingsWidget->CancelPendingSettings();
		SettingsWidget->OnCloseRequested.RemoveDynamic(this, &UDroneFrontEndRootWidget::HandleSettingsBackClicked);
	}
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
		// 기체 선택에서 돌아온 새 Widget도 원래 훈련 하위 탭/Story 목록을 복원한다.
		const FName ContextMissionId = InFlowSubsystem->GetSnapshot().SelectedMissionId.IsNone()
			? InFlowSubsystem->GetLastLobbyMissionId() : InFlowSubsystem->GetSnapshot().SelectedMissionId;
		if (const UDroneMissionDefinition* Mission = InFlowSubsystem->FindMissionDefinition(ContextMissionId))
		{
			ActiveLobbyCategory = Mission->GetLobbyCategory();
		}
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
	// TUT-PROGRESS-01: NextMissionId로 이어진 미션은 연결 순서대로(튜토리얼 1-1 → 4-2, 스토리 M1 → M4), 나머지는 ID 순.
	const UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	return Flow ? Flow->GetMissionIdsInLobbyOrder(ActiveLobbyCategory) : TArray<FName>();
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
	if (WidgetTree)
	{
		Apply(Cast<UImage>(WidgetTree->FindWidget(TEXT("LobbyBackgroundImage"))), TitleBackgroundTexture);
		Apply(Cast<UImage>(WidgetTree->FindWidget(TEXT("BriefingBackgroundImage"))), TitleBackgroundTexture);
		Apply(Cast<UImage>(WidgetTree->FindWidget(TEXT("SettingsBackgroundImage"))), TitleBackgroundTexture);
	}
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
	const bool bShouldShow = bVisible && DisplayedState == EDroneGameFlowState::OpeningTrailer;
	if (SettingsWidget && bShouldShow != bSettingsVisible)
	{
		if (bShouldShow) SettingsWidget->RefreshFromCurrentSettings();
		else SettingsWidget->CancelPendingSettings();
	}
	const bool bWasVisible = bSettingsVisible;
	bSettingsVisible = bShouldShow;
	if (SettingsPanel) SettingsPanel->SetVisibility(bSettingsVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	// 설정이 열리면 설정 위젯이 첫 조작 항목으로, 닫히면 타이틀의 [설정] 버튼으로 포커스를 돌린다(UI-PAD-01).
	if (bSettingsVisible && !bWasVisible)
	{
		GamepadFocus.CancelPendingFocus();
		if (SettingsWidget) SettingsWidget->RequestGamepadFocus();
	}
	else if (!bSettingsVisible && bWasVisible && DisplayedState == EDroneGameFlowState::OpeningTrailer)
	{
		GamepadFocus.RequestFocus({GetTitleButton(2), LastTitleFocus.Get(), GetTitleButton(0)});
	}
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
	if (DisplayedState == EDroneGameFlowState::OpeningTrailer) { OpenTrainingLobby(); return; }
	SetLobbyCategory(EDroneMissionCategory::Tutorial);
}
void UDroneFrontEndRootWidget::HandleTrainingClicked()
{
	OpenTrainingLobby();
}
void UDroneFrontEndRootWidget::HandleRacingTabClicked()
{
	if (DisplayedState == EDroneGameFlowState::OpeningTrailer && !OpenTrainingLobby()) return;
	SetLobbyCategory(EDroneMissionCategory::Racing);
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
	// 브리핑에서 Y(패드)·Tab으로 지금 대사를 건너뛴다(MISSION-BRIEFING-02).
	if (!InKeyEvent.IsRepeat() && DisplayedState == EDroneGameFlowState::MissionTrailer
		&& (Key == EKeys::Gamepad_FaceButton_Top || Key == EKeys::Tab))
	{
		SkipBriefingLine();
		return FReply::Handled();
	}
	// 훈련 로비에서 LB/RB로 튜토리얼·레이싱 탭을 바꾼다. 목록이 새로 만들어지므로 첫 미션에 다시 포커스한다.
	if (!InKeyEvent.IsRepeat() && DisplayedState == EDroneGameFlowState::LobbyMissionSelect && IsTrainingLobby()
		&& (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder))
	{
		const EDroneMissionCategory Next = Key == EKeys::Gamepad_LeftShoulder
			? EDroneMissionCategory::Tutorial : EDroneMissionCategory::Racing;
		if (Next != ActiveLobbyCategory && SetLobbyCategory(Next))
		{
			RequestScreenFocus(EDroneGameFlowState::LobbyMissionSelect);
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UDroneFrontEndRootWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	GamepadFocus.FocusScale = GamepadFocusScale;
	GamepadFocus.FocusTint = GamepadFocusTint;
	const TArray<UWidget*> Highlightables = GetGamepadHighlightables();
	GamepadFocus.Tick(GetOwningPlayer(), Highlightables);
	if (DisplayedState == EDroneGameFlowState::OpeningTrailer && !bSettingsVisible)
	{
		if (UWidget* Focused = GamepadFocus.GetHighlighted(); Focused && TitleButtons.Contains(Cast<UButton>(Focused)))
		{
			LastTitleFocus = Focused;
		}
	}
}

void UDroneFrontEndRootWidget::RequestScreenFocus(const EDroneGameFlowState State)
{
	TArray<UWidget*> Candidates;
	if (State == EDroneGameFlowState::OpeningTrailer)
	{
		if (bSettingsVisible) return; // 설정 위젯이 자기 포커스를 관리한다.
		Candidates.Add(LastTitleFocus.Get());
		for (UButton* TitleButton : TitleButtons) Candidates.Add(TitleButton);
	}
	else if (State == EDroneGameFlowState::LobbyMissionSelect)
	{
		// 브리핑에서 돌아오면 고르던 미션, 미션을 마치고 돌아오면 방금 한 미션(선택은 비워지고 LastLobbyMissionId에 남는다),
		// 처음이면 목록 첫 미션. 탭 복원(SetFlowSubsystem)과 같은 기준이다.
		FName SelectedId = NAME_None;
		if (FlowSubsystem.IsValid())
		{
			SelectedId = FlowSubsystem->GetSnapshot().SelectedMissionId.IsNone()
				? FlowSubsystem->GetLastLobbyMissionId() : FlowSubsystem->GetSnapshot().SelectedMissionId;
		}
		const int32 SelectedIndex = NativeMissionButtonIds.IndexOfByKey(SelectedId);
		if (SelectedIndex != INDEX_NONE) Candidates.Add(GetNativeMissionButton(SelectedIndex));
		Candidates.Add(GetNativeMissionButton(0));
		Candidates.Add(MissionSelectButton);
		Candidates.Add(ActiveLobbyCategory == EDroneMissionCategory::Tutorial ? TutorialTabButton
			: ActiveLobbyCategory == EDroneMissionCategory::Racing ? RacingTabButton : MissionTabButton);
		Candidates.Add(StartMissionButton);
		Candidates.Add(LobbyBackButton);
	}
	else if (State == EDroneGameFlowState::MissionTrailer)
	{
		Candidates.Add(FinishMissionBriefingButton);
		Candidates.Add(BriefingBackButton);
	}
	GamepadFocus.RequestFocus(Candidates);
}

TArray<UWidget*> UDroneFrontEndRootWidget::GetGamepadHighlightables() const
{
	TArray<UWidget*> Result;
	for (UButton* TitleButton : TitleButtons) Result.Add(TitleButton);
	for (UDroneMissionSelectionButton* MissionButton : NativeMissionButtons) Result.Add(MissionButton);
	for (UWidget* Widget : TArray<UWidget*>{MissionSelectButton, TutorialTabButton, RacingTabButton, MissionTabButton,
		StartMissionButton, LobbyBackButton, FinishMissionBriefingButton, BriefingBackButton})
	{
		Result.Add(Widget);
	}
	Result.RemoveAll([](const UWidget* Widget) { return Widget == nullptr; });
	return Result;
}

void UDroneFrontEndRootWidget::ConfigureMissionScroll(UScrollBox* MissionScroll) const
{
	if (!MissionScroll) return;
	// Story 4개는 넘치지 않고 Training 9개는 넘친다. 넘칠 때만 스크롤바가 생기면 목록 폭이
	// 두께 9 + 여백 2×2 = 13px 줄어들며 버튼 글자 줄바꿈이 바뀐다(UI-LAYOUT-01 EnterTraining 13px).
	// 스크롤바 자리를 항상 확보해 분류를 바꿔도 목록 폭이 같게 한다.
	MissionScroll->SetAlwaysShowScrollbar(true);
	// 패드로 아래 미션을 고르면 목록이 따라 내려가게 한다(UI-PAD-01).
	MissionScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
}

void UDroneFrontEndRootWidget::StartBriefingLines()
{
	StopBriefingLines();
	const UDroneMissionDefinition* Mission = FlowSubsystem.IsValid()
		? FlowSubsystem->FindMissionDefinition(FlowSubsystem->GetSnapshot().SelectedMissionId) : nullptr;
	if (Mission)
	{
		for (const FDroneMissionBriefingLine& Line : Mission->BriefingLines)
		{
			const bool bHasFact = !Line.StoryFactId.IsNone() && FlowSubsystem->HasStoryFact(Line.StoryFactId);
			const bool bVisible = Line.StoryFactCondition == EDroneMissionStoryFactCondition::Always
				|| (Line.StoryFactCondition == EDroneMissionStoryFactCondition::FactPresent && bHasFact)
				|| (Line.StoryFactCondition == EDroneMissionStoryFactCondition::FactAbsent && !bHasFact);
			if (bVisible && !Line.Text.IsEmpty())
			{
				ActiveBriefingLines.Add(Line);
			}
		}
	}
	if (MissionBriefingSubtitlePanel)
	{
		MissionBriefingSubtitlePanel->SetVisibility(ActiveBriefingLines.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (!ActiveBriefingLines.IsEmpty())
	{
		ShowBriefingLine(0);
	}
}

void UDroneFrontEndRootWidget::ShowBriefingLine(const int32 Index)
{
	if (!ActiveBriefingLines.IsValidIndex(Index))
	{
		return;
	}
	const FDroneMissionBriefingLine& Line = ActiveBriefingLines[Index];
	BriefingLineIndex = Index;
	if (MissionBriefingSpeakerText) MissionBriefingSpeakerText->SetText(Line.Speaker);
	if (MissionBriefingLineText) MissionBriefingLineText->SetText(Line.Text);

	if (BriefingVoice)
	{
		BriefingVoice->Stop();
		BriefingVoice = nullptr;
	}
	USoundBase* Voice = Line.Voice.LoadSynchronous();
	if (Voice)
	{
		BriefingVoice = UGameplayStatics::SpawnSound2D(this, Voice);
	}

	// 표시 시간: 지정값 → 음성 길이(+0.3초 여유) → 글자 수. 글자 수 기준은 최소·최대 사이로 자른다.
	if (Line.DurationSeconds > 0.0f)
	{
		BriefingLineDuration = Line.DurationSeconds;
	}
	else if (Voice && Voice->GetDuration() > 0.0f && Voice->GetDuration() < INDEFINITELY_LOOPING_DURATION)
	{
		BriefingLineDuration = Voice->GetDuration() + 0.3f;
	}
	else
	{
		BriefingLineDuration = FMath::Clamp(Line.Text.ToString().Len() * BriefingSecondsPerCharacter,
			BriefingMinLineSeconds, FMath::Max(BriefingMinLineSeconds, BriefingMaxLineSeconds));
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BriefingLineTimer,
			FTimerDelegate::CreateUObject(this, &UDroneFrontEndRootWidget::HandleBriefingLineTimer), BriefingLineDuration, false);
	}
}

void UDroneFrontEndRootWidget::HandleBriefingLineTimer()
{
	if (DisplayedState == EDroneGameFlowState::MissionTrailer)
	{
		SkipBriefingLine();
	}
}

bool UDroneFrontEndRootWidget::SkipBriefingLine()
{
	if (BriefingLineIndex == INDEX_NONE || !ActiveBriefingLines.IsValidIndex(BriefingLineIndex + 1))
	{
		// 마지막 대사는 화면에 남겨 두고 자동 진행만 멈춘다.
		BriefingLineDuration = 0.0f;
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BriefingLineTimer);
		return false;
	}
	ShowBriefingLine(BriefingLineIndex + 1);
	return true;
}

void UDroneFrontEndRootWidget::StopBriefingLines()
{
	if (BriefingVoice)
	{
		BriefingVoice->Stop();
		BriefingVoice = nullptr;
	}
	ActiveBriefingLines.Reset();
	BriefingLineIndex = INDEX_NONE;
	BriefingLineDuration = 0.0f;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BriefingLineTimer);
	if (MissionBriefingSubtitlePanel)
	{
		MissionBriefingSubtitlePanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UButton* UDroneFrontEndRootWidget::GetNativeMissionButton(const int32 Index) const
{
	return NativeMissionButtons.IsValidIndex(Index) ? NativeMissionButtons[Index].Get() : nullptr;
}

bool UDroneFrontEndRootWidget::FinishOpeningTrailer()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	if (!Flow || Flow->GetSnapshot().State != EDroneGameFlowState::OpeningTrailer) return false;
	SetLobbyCategory(EDroneMissionCategory::Mission);
	return Flow->EnterLobbyFromOpeningTrailer();
}

bool UDroneFrontEndRootWidget::OpenTrainingLobby()
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	if (!Flow || Flow->GetSnapshot().State != EDroneGameFlowState::OpeningTrailer) return false;
	SetLobbyCategory(EDroneMissionCategory::Tutorial);
	return Flow->EnterLobbyFromOpeningTrailer();
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
	SettingsWidget = Cast<UDroneSettingsWidget>(WidgetTree->FindWidget(TEXT("SettingsWidget")));
	LobbyBackButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("LobbyBackButton")));
	BriefingBackButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BriefingBackButton")));
	MissionBriefingSubtitlePanel = WidgetTree->FindWidget(TEXT("MissionBriefingSubtitlePanel"));
	MissionBriefingSpeakerText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MissionBriefingSpeakerText")));
	MissionBriefingLineText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MissionBriefingLineText")));
	MissionButtonsColumn = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("MissionButtonsColumn")));
	// WBP 레이아웃도 네이티브와 같은 목록 스크롤 규칙(스크롤바 자리 확보, 패드 포커스 따라 스크롤)을 쓴다.
	ConfigureMissionScroll(MissionButtonsColumn ? Cast<UScrollBox>(MissionButtonsColumn->GetParent()) : nullptr);
	TitleButtons.Reset();
	for (const FName TitleButtonName : {DroneFrontEndUI::ContinueButtonName, FName(TEXT("TitleTutorialButton")),
		FName(TEXT("TitleSettingsButton")), FName(TEXT("ExitButton"))})
	{
		if (UButton* TitleButton = Cast<UButton>(WidgetTree->FindWidget(TitleButtonName))) TitleButtons.Add(TitleButton);
	}
	TrainingCategoryTabs = WidgetTree->FindWidget(TEXT("TrainingCategoryTabs"));
	MissionObjectiveSummaryText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MissionObjectiveSummaryText")));
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
	ContinueButton = AddTitleButton(DroneFrontEndUI::ContinueButtonName, TEXT("시작"));
	UButton* TitleTutorialButton = AddTitleButton(TEXT("TitleTutorialButton"), TEXT("훈련"));
	TitleTutorialButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleTrainingClicked);
	UButton* TitleSettingsButton = AddTitleButton(TEXT("TitleSettingsButton"), TEXT("설정"));
	TitleSettingsButton->OnClicked.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleSettingsClicked);
	ExitButton = AddTitleButton(TEXT("ExitButton"), TEXT("종료"));
	// 패드 포커스 순서와 복귀 대상(UI-PAD-01). 화면 위→아래 순서와 같다.
	TitleButtons = {ContinueButton, TitleTutorialButton, TitleSettingsButton, ExitButton};

	// 시작 화면 PNG를 재수입/교체하지 않고 같은 이미지 위에 읽기 쉬운 시안 패널만 배치한다.
	// 고정 디자인 캔버스를 비율 유지해 축소하므로 1280 화면에서도 세 열/하단 버튼이 잘리지 않는다.
	auto AddWorkspaceCanvas = [this](UBorder* Panel, const FName BackgroundName)
	{
		Panel->SetPadding(FMargin(0.f));
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
		Panel->SetContent(Layers);
		UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), BackgroundName);
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* BackgroundSlot = Layers->AddChildToOverlay(Background);
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
		UBorder* Shade = WidgetTree->ConstructWidget<UBorder>();
		Shade->SetBrushColor(FLinearColor(0.008f, 0.015f, 0.025f, 0.80f));
		Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* ShadeSlot = Layers->AddChildToOverlay(Shade);
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Fill);
		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
		Scale->SetStretch(EStretch::ScaleToFit);
		UOverlaySlot* ScaleSlot = Layers->AddChildToOverlay(Scale);
		ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
		ScaleSlot->SetVerticalAlignment(VAlign_Fill);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(1920.f);
		Size->SetHeightOverride(1080.f);
		Scale->SetContent(Size);
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrushColor(FLinearColor::Transparent);
		Frame->SetPadding(FMargin(100.f, 62.f));
		Size->SetContent(Frame);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Frame->SetContent(Column);
		return Column;
	};

	UBorder* NativeLobbyPanel = AddFullScreenPanel(
		DroneFrontEndUI::LobbyPanelName,
		FLinearColor(0.012f, 0.025f, 0.032f, 1.0f));
	LobbyPanel = NativeLobbyPanel;
	UVerticalBox* LobbyColumn = AddWorkspaceCanvas(NativeLobbyPanel, TEXT("LobbyBackgroundImage"));
	LobbyBackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LobbyBackButton"));
	UTextBlock* LobbyBackText = WidgetTree->ConstructWidget<UTextBlock>();
	LobbyBackText->SetText(FText::FromString(TEXT("← 시작 화면  ·  Esc / 패드 B")));
	LobbyBackButton->SetContent(LobbyBackText);
	UVerticalBoxSlot* LobbyBackSlot = LobbyColumn->AddChildToVerticalBox(LobbyBackButton);
	LobbyBackSlot->SetPadding(FMargin(0, 0, 0, 22));
	LobbyBackSlot->SetHorizontalAlignment(HAlign_Left);

	LobbyTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::LobbyTitleName);
	LobbyTitleText->SetText(FText::FromString(TEXT("미션 선택")));
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
		StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TrainingCategoryTabs"));
	TrainingCategoryTabs = Tabs;
	LobbyColumn->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0, 0, 0, 20));
	auto AddTab = [this, Tabs](const FName Name, const TCHAR* Caption)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Caption));
		Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22.f));
		DroneFrontEndUI::SetButtonPadding(Button, FMargin(24.f, 14.f));
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

	auto AddWorkspacePanel = [this, MissionWorkspace](const FName Name, const FLinearColor Color, const float Weight)
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Panel->SetBrushColor(Color);
		Panel->SetPadding(FMargin(24.0f));
		if (UHorizontalBoxSlot* Slot = MissionWorkspace->AddChildToHorizontalBox(Panel))
		{
			FSlateChildSize WeightedSize(ESlateSizeRule::Fill);
			WeightedSize.Value = Weight;
			Slot->SetSize(WeightedSize);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		}
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		return Column;
	};

	UVerticalBox* MissionListColumn = AddWorkspacePanel(
		TEXT("MissionListPanel"), FLinearColor(0.018f, 0.028f, 0.038f, 0.94f), 0.75f);
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
	ConfigureMissionScroll(MissionScroll);
	MissionScroll->AddChild(MissionButtonsColumn);
	if (UVerticalBoxSlot* MissionButtonSlot = MissionListColumn->AddChildToVerticalBox(MissionScroll))
	{
		MissionButtonSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
		MissionButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* MissionCardColumn = AddWorkspacePanel(
		TEXT("MissionCardPanel"), FLinearColor(0.032f, 0.045f, 0.055f, 0.95f), 1.15f);
	UTextBlock* MissionCardHeader = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("MissionCardHeader"));
	MissionCardHeader->SetText(FText::FromString(TEXT("선택 작전")));
	MissionCardHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.90f, 0.83f, 1.0f)));
	MissionCardColumn->AddChildToVerticalBox(MissionCardHeader);
	MissionThumbnailImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MissionThumbnailImage"));
	USizeBox* ThumbnailSize = WidgetTree->ConstructWidget<USizeBox>();
	ThumbnailSize->SetHeightOverride(330.f);
	UScaleBox* ThumbnailScale = WidgetTree->ConstructWidget<UScaleBox>();
	ThumbnailScale->SetStretch(EStretch::ScaleToFit);
	UOverlay* PreviewLayers = WidgetTree->ConstructWidget<UOverlay>();
	ThumbnailScale->SetContent(PreviewLayers);
	UTextBlock* PreviewPlaceholder = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MissionPreviewPlaceholder"));
	PreviewPlaceholder->SetText(FText::FromString(TEXT("작전 이미지\n미션을 선택하세요")));
	PreviewPlaceholder->SetJustification(ETextJustify::Center);
	PreviewPlaceholder->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.6f, 0.65f)));
	PreviewPlaceholder->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 26.f));
	UOverlaySlot* PlaceholderSlot = PreviewLayers->AddChildToOverlay(PreviewPlaceholder);
	PlaceholderSlot->SetHorizontalAlignment(HAlign_Center);
	PlaceholderSlot->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* PreviewImageSlot = PreviewLayers->AddChildToOverlay(MissionThumbnailImage);
	PreviewImageSlot->SetHorizontalAlignment(HAlign_Fill);
	PreviewImageSlot->SetVerticalAlignment(VAlign_Fill);
	ThumbnailSize->SetContent(ThumbnailScale);
	MissionCardColumn->AddChildToVerticalBox(ThumbnailSize);

	MissionNameText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionNameName);
	MissionNameText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24.0f));
	MissionNameText->SetAutoWrapText(true);
	MissionNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.96f, 0.96f, 1.0f)));
	if (UVerticalBoxSlot* NameSlot = MissionCardColumn->AddChildToVerticalBox(MissionNameText))
	{
		NameSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 10.0f));
	}

	MissionMetaText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionMetaName);
	MissionMetaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	MissionMetaText->SetAutoWrapText(true);
	MissionCardColumn->AddChildToVerticalBox(MissionMetaText);

	UVerticalBox* MissionDetailColumn = AddWorkspacePanel(
		TEXT("MissionDetailPanel"), FLinearColor(0.014f, 0.025f, 0.034f, 0.95f), 1.5f);
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
	MissionDescriptionText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22.f));
	MissionDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.85f, 1.0f)));
	UScrollBox* DescriptionScroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBox* DescriptionContent = WidgetTree->ConstructWidget<UVerticalBox>();
	DescriptionScroll->AddChild(DescriptionContent);
	DescriptionContent->AddChildToVerticalBox(MissionDescriptionText);
	MissionObjectiveSummaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MissionObjectiveSummaryText"));
	MissionObjectiveSummaryText->SetAutoWrapText(true);
	MissionObjectiveSummaryText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20.f));
	MissionObjectiveSummaryText->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.91f, 0.89f)));
	DescriptionContent->AddChildToVerticalBox(MissionObjectiveSummaryText)->SetPadding(FMargin(0, 30, 0, 0));
	if (UVerticalBoxSlot* DescriptionSlot = MissionDetailColumn->AddChildToVerticalBox(DescriptionScroll))
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
	DroneFrontEndUI::SetButtonPadding(StartMissionButton, FMargin(80.f, 16.f));
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
	UVerticalBox* BriefingColumn = AddWorkspaceCanvas(NativeBriefingPanel, TEXT("BriefingBackgroundImage"));
	BriefingBackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BriefingBackButton"));
	UTextBlock* BriefingBackText = WidgetTree->ConstructWidget<UTextBlock>();
	BriefingBackText->SetText(FText::FromString(TEXT("← 미션 선택  ·  Esc / 패드 B")));
	BriefingBackButton->SetContent(BriefingBackText);
	UVerticalBoxSlot* BriefingBackSlot = BriefingColumn->AddChildToVerticalBox(BriefingBackButton);
	BriefingBackSlot->SetPadding(FMargin(0, 0, 0, 28));
	BriefingBackSlot->SetHorizontalAlignment(HAlign_Left);

	MissionBriefingTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionBriefingTitleName);
	MissionBriefingTitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 32.0f));
	MissionBriefingTitleText->SetAutoWrapText(true);
	MissionBriefingTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	BriefingColumn->AddChildToVerticalBox(MissionBriefingTitleText)->SetPadding(FMargin(0, 0, 0, 28));
	UHorizontalBox* BriefingWorkspace = WidgetTree->ConstructWidget<UHorizontalBox>();
	BriefingColumn->AddChildToVerticalBox(BriefingWorkspace)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	USizeBox* BriefingPreviewSize = WidgetTree->ConstructWidget<USizeBox>();
	BriefingPreviewSize->SetWidthOverride(600.f);
	UImage* BriefingPreview = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BriefingThumbnailImage"));
	UScaleBox* BriefingPreviewScale = WidgetTree->ConstructWidget<UScaleBox>();
	BriefingPreviewScale->SetStretch(EStretch::ScaleToFit);
	BriefingPreviewScale->SetContent(BriefingPreview);
	BriefingPreviewSize->SetContent(BriefingPreviewScale);
	BriefingWorkspace->AddChildToHorizontalBox(BriefingPreviewSize)->SetPadding(FMargin(0, 0, 48, 0));

	MissionBriefingBodyText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		DroneFrontEndUI::MissionBriefingBodyName);
	MissionBriefingBodyText->SetAutoWrapText(true);
	MissionBriefingBodyText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 24.f));
	MissionBriefingBodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.87f, 0.89f, 1.0f)));
	UScrollBox* BriefingScroll = WidgetTree->ConstructWidget<UScrollBox>();
	BriefingScroll->AddChild(MissionBriefingBodyText);
	BriefingWorkspace->AddChildToHorizontalBox(BriefingScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// MISSION-BRIEFING-02: 허브 대사 자막. 높이를 고정해 대사 길이가 달라도 아래 버튼이 움직이지 않게 한다.
	USizeBox* SubtitleSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MissionBriefingSubtitlePanel"));
	SubtitleSize->SetHeightOverride(150.f);
	UBorder* SubtitleBorder = WidgetTree->ConstructWidget<UBorder>();
	SubtitleBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	SubtitleBorder->SetPadding(FMargin(28.f, 16.f));
	UVerticalBox* SubtitleColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	MissionBriefingSpeakerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MissionBriefingSpeakerText"));
	MissionBriefingSpeakerText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20.f));
	MissionBriefingSpeakerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.95f, 0.82f, 1.0f)));
	SubtitleColumn->AddChildToVerticalBox(MissionBriefingSpeakerText)->SetPadding(FMargin(0, 0, 0, 6));
	MissionBriefingLineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MissionBriefingLineText"));
	MissionBriefingLineText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22.f));
	MissionBriefingLineText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.97f, 0.97f, 1.0f)));
	MissionBriefingLineText->SetAutoWrapText(true);
	SubtitleColumn->AddChildToVerticalBox(MissionBriefingLineText);
	SubtitleBorder->SetContent(SubtitleColumn);
	SubtitleSize->SetContent(SubtitleBorder);
	BriefingColumn->AddChildToVerticalBox(SubtitleSize)->SetPadding(FMargin(0, 24, 0, 0));
	MissionBriefingSubtitlePanel = SubtitleSize;
	MissionBriefingSubtitlePanel->SetVisibility(ESlateVisibility::Collapsed);

	FinishMissionBriefingButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		DroneFrontEndUI::FinishMissionBriefingButtonName);
	UTextBlock* FinishBriefingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("FinishMissionBriefingButtonText"));
	FinishBriefingText->SetText(FText::FromString(TEXT("작전 지역으로 이동")));
	FinishBriefingText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24.f));
	DroneFrontEndUI::SetButtonPadding(FinishMissionBriefingButton, FMargin(64.f, 16.f));
	FinishMissionBriefingButton->AddChild(FinishBriefingText);
	UVerticalBoxSlot* FinishBriefingSlot = BriefingColumn->AddChildToVerticalBox(FinishMissionBriefingButton);
	FinishBriefingSlot->SetPadding(FMargin(0, 28, 0, 0));
	FinishBriefingSlot->SetHorizontalAlignment(HAlign_Center);

	// 임시 품질 3버튼을 실제 사운드/화면/성능 설정 Widget으로 대체한다.
	UBorder* NativeSettingsPanel = AddFullScreenPanel(TEXT("SettingsPanel"), FLinearColor(0.008f, 0.018f, 0.025f, 0.97f));
	SettingsPanel = NativeSettingsPanel;
	UVerticalBox* SettingsColumn = AddWorkspaceCanvas(NativeSettingsPanel, TEXT("SettingsBackgroundImage"));
	SettingsWidget = WidgetTree->ConstructWidget<UDroneSettingsWidget>(UDroneSettingsWidget::StaticClass(), TEXT("SettingsWidget"));
	if (SettingsWidget)
	{
		SettingsWidget->OnCloseRequested.AddUniqueDynamic(this, &UDroneFrontEndRootWidget::HandleSettingsBackClicked);
		SettingsColumn->AddChildToVerticalBox(SettingsWidget)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
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
	// 대사는 브리핑 화면에 들어올 때 한 번 처음부터 시작한다. 같은 화면에서 Snapshot이 바뀌어도 다시 시작하지 않는다.
	if (State == EDroneGameFlowState::MissionTrailer) StartBriefingLines();
	else StopBriefingLines();
	ReceiveFrontEndStateDisplayed(State);
	// 사라진 화면의 버튼에 Focus가 남지 않게 먼저 루트가 받고(Esc·B 처리 유지),
	// 다음 Tick부터 새 화면의 첫 버튼으로 옮긴다(UI-PAD-01).
	if (IsInViewport() && GetOwningPlayer()
		&& (State == EDroneGameFlowState::OpeningTrailer || State == EDroneGameFlowState::LobbyMissionSelect
			|| State == EDroneGameFlowState::MissionTrailer))
	{
		SetUserFocus(GetOwningPlayer());
		RequestScreenFocus(State);
	}
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
	if (WidgetTree)
	{
		if (UImage* Preview = Cast<UImage>(WidgetTree->FindWidget(TEXT("BriefingThumbnailImage"))))
		{
			UTexture2D* Texture = Mission ? Mission->Thumbnail.LoadSynchronous() : nullptr;
			if (!Texture) Texture = TitleBackgroundTexture;
			Preview->SetBrushFromTexture(Texture);
			Preview->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
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
	const ESlateVisibility TrainingVisibility = IsTrainingLobby() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (TrainingCategoryTabs) TrainingCategoryTabs->SetVisibility(TrainingVisibility);
	if (TutorialTabButton) TutorialTabButton->SetVisibility(TrainingVisibility);
	if (RacingTabButton) RacingTabButton->SetVisibility(TrainingVisibility);
	// 기존 이름/API는 WBP 호환용으로 보존하되 별도의 3번째 훈련 탭으로 표시하지 않는다.
	if (MissionTabButton) MissionTabButton->SetVisibility(ESlateVisibility::Collapsed);
	if (TutorialTabButton) TutorialTabButton->SetBackgroundColor(ActiveLobbyCategory == EDroneMissionCategory::Tutorial ? SelectedTabColor : OtherTabColor);
	if (RacingTabButton) RacingTabButton->SetBackgroundColor(ActiveLobbyCategory == EDroneMissionCategory::Racing ? SelectedTabColor : OtherTabColor);
	if (MissionTabButton) MissionTabButton->SetBackgroundColor(ActiveLobbyCategory == EDroneMissionCategory::Mission ? SelectedTabColor : OtherTabColor);
	if (LobbyTitleText) LobbyTitleText->SetText(FText::FromString(IsTrainingLobby() ? TEXT("훈련 선택") : TEXT("미션 선택")));
	if (LobbyStatusText) LobbyStatusText->SetText(FText::FromString(FString::Printf(
		TEXT("%s  ·  %d개 항목  |  왼쪽 목록 선택 → 설명 확인 → 하단 시작"),
		ActiveLobbyCategory == EDroneMissionCategory::Tutorial ? TEXT("훈련 / 튜토리얼")
		: ActiveLobbyCategory == EDroneMissionCategory::Racing ? TEXT("훈련 / 레이싱") : TEXT("스토리 미션"), MissionIds.Num())));
	if (MissionThumbnailImage)
	{
		UTexture2D* Thumbnail = SelectedMission ? SelectedMission->Thumbnail.LoadSynchronous() : nullptr;
		if (SelectedMission && !Thumbnail) Thumbnail = TitleBackgroundTexture;
		MissionThumbnailImage->SetBrushFromTexture(Thumbnail, true);
		MissionThumbnailImage->SetVisibility(Thumbnail ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (WidgetTree)
		{
			if (UWidget* Placeholder = WidgetTree->FindWidget(TEXT("MissionPreviewPlaceholder")))
				Placeholder->SetVisibility(Thumbnail ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
	if (SelectedMission)
	{
		DisplayedMissionName = SelectedMission->DisplayName;
		DisplayedMissionDescription = SelectedMission->LobbyDescription;
		FString Meta = FText::Format(
			FText::FromString(TEXT("지역: {0}  |  난이도: {1}")),
			SelectedMission->RegionText,
			SelectedMission->DifficultyText).ToString();
		int32 Number = 0;
		int32 Count = 0;
		int32 Completed = 0;
		if (FlowSubsystem.IsValid()
			&& FlowSubsystem->GetMissionSequencePosition(SelectedMission->MissionId, Number, Count, Completed))
		{
			// 이어진 과정(튜토리얼 8개 등) 안 위치와 이번 실행에서 끝낸 수.
			Meta += FString::Printf(TEXT("  |  %s %d/%d (완료 %d)"),
				SelectedMission->GetLobbyCategory() == EDroneMissionCategory::Tutorial ? TEXT("수업") : TEXT("순서"),
				Number, Count, Completed);
		}
		DisplayedMissionMeta = FText::FromString(Meta);
	}
	else
	{
		DisplayedMissionName = FText::FromString(TEXT("미션을 선택하세요"));
		DisplayedMissionDescription = FText::FromString(TEXT("목록에서 미션을 고르면 설명이 표시됩니다."));
		// 빈 문자열이면 줄 높이가 0이었다가 첫 선택에서 처음 채워질 때만 카드가 36px 튄다(UI-LAYOUT-01).
		// 같은 형식의 자리표시 글자로 줄을 미리 잡아 둔다.
		DisplayedMissionMeta = FText::FromString(TEXT("지역: -  |  난이도: -"));
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
	if (MissionObjectiveSummaryText)
	{
		FString Summary;
		if (SelectedMission)
		{
			Summary = TEXT("진행 목표");
			int32 Number = 1;
			if (!SelectedMission->ObjectiveRules.IsEmpty())
			{
				for (const FDroneMissionObjectiveRule& Rule : SelectedMission->ObjectiveRules)
					Summary += FString::Printf(TEXT("\n%d. %s"), Number++, *Rule.Description.ToString());
			}
			else
			{
				for (const FText& Objective : SelectedMission->InitialObjectives)
					Summary += FString::Printf(TEXT("\n%d. %s"), Number++, *Objective.ToString());
			}
		}
		MissionObjectiveSummaryText->SetText(FText::FromString(Summary));
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

	// 선택·Hover처럼 목록 구성은 그대로이고 상태만 바뀐 경우다(UI-LAYOUT-01).
	// 버튼을 지우고 다시 만들면 첫 프레임 줄바꿈 재계산으로 목록이 튀고 스크롤 위치·포커스도 사라지므로 제자리 갱신만 한다.
	if (CanReuseNativeMissionButtons(MissionIds))
	{
		// Blueprint에서 줄바꿈 폭을 바꾼 뒤라면 재생성 경로와 같은 결과가 되도록 기존 라벨에도 다시 적용한다.
		const bool bWrapChanged = !FMath::IsNearlyEqual(AppliedMissionButtonLabelWrapWidth, MissionButtonLabelWrapWidth);
		for (int32 Index = 0; Index < MissionIds.Num(); ++Index)
		{
			if (bWrapChanged)
			{
				ApplyMissionButtonLabelWrap(NativeMissionButtonLabels[Index]);
			}
			RefreshNativeMissionButton(Index, MissionIds[Index]);
		}
		AppliedMissionButtonLabelWrapWidth = MissionButtonLabelWrapWidth;
		return;
	}

	// 여기부터는 목록 구성 자체가 바뀐 경우(분류 전환, 미션 추가/제거)라 새로 만든다.
	MissionButtonsColumn->ClearChildren();
	NativeMissionButtons.Reset();
	NativeMissionButtonLabels.Reset();
	NativeMissionButtonIds.Reset();
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
		UDroneMissionSelectionButton* Button = WidgetTree->ConstructWidget<UDroneMissionSelectionButton>(
			UDroneMissionSelectionButton::StaticClass(),
			FName(*FString::Printf(TEXT("MissionSelectButton_%d"), Index)));
		Button->InitializeMissionSelection(MissionId);
		Button->OnMissionSelectionRequested.AddUniqueDynamic(
			this, &UDroneFrontEndRootWidget::HandleMissionButtonSelected);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("MissionSelectButtonText_%d"), Index)));
		ApplyMissionButtonLabelWrap(Label);
		Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16.0f));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.97f, 0.97f, 1.0f)));
		Button->AddChild(Label);
		DroneFrontEndUI::SetButtonPadding(Button, FMargin(14.f, 18.f));
		if (UVerticalBoxSlot* ButtonSlot = MissionButtonsColumn->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
		NativeMissionButtons.Add(Button);
		NativeMissionButtonLabels.Add(Label);
		NativeMissionButtonIds.Add(MissionId);
		// 글자·선택 색은 재사용 경로와 같은 함수로 채워 두 경로의 결과가 항상 같게 한다.
		RefreshNativeMissionButton(Index, MissionId);
		if (Index == 0)
		{
			MissionSelectButton = Button;
			MissionSelectButtonText = Label;
		}
	}
	AppliedMissionButtonLabelWrapWidth = MissionButtonLabelWrapWidth;
}

bool UDroneFrontEndRootWidget::CanReuseNativeMissionButtons(const TArray<FName>& MissionIds) const
{
	// 빈 목록은 "등록된 미션 없음" 안내 버튼 하나라서 재사용 대상이 아니다.
	if (MissionIds.IsEmpty() || MissionIds != NativeMissionButtonIds
		|| NativeMissionButtons.Num() != MissionIds.Num() || NativeMissionButtonLabels.Num() != MissionIds.Num())
	{
		return false;
	}
	// Layout을 다시 만들면 MissionButtonsColumn이 새 객체가 되므로, 옛 버튼이 아직 지금 열에 붙어 있는지까지 본다.
	for (int32 Index = 0; Index < MissionIds.Num(); ++Index)
	{
		const UDroneMissionSelectionButton* Button = NativeMissionButtons[Index];
		if (!Button || !NativeMissionButtonLabels[Index] || Button->GetParent() != MissionButtonsColumn.Get())
		{
			return false;
		}
	}
	return true;
}

void UDroneFrontEndRootWidget::RefreshNativeMissionButton(const int32 Index, const FName MissionId)
{
	UDroneMissionSelectionButton* Button = NativeMissionButtons.IsValidIndex(Index) ? NativeMissionButtons[Index].Get() : nullptr;
	UTextBlock* Label = NativeMissionButtonLabels.IsValidIndex(Index) ? NativeMissionButtonLabels[Index].Get() : nullptr;
	if (!Button || !Label)
	{
		return;
	}

	const UDroneMissionDefinition* Mission = FlowSubsystem.IsValid()
		? FlowSubsystem->FindMissionDefinition(MissionId)
		: nullptr;
	const bool bSelected = FlowSubsystem.IsValid() && FlowSubsystem->GetSnapshot().SelectedMissionId == MissionId;
	Button->SetBackgroundColor(bSelected
		? FLinearColor(0.08f, 0.5f, 0.44f, 1.f) : FLinearColor(0.04f, 0.16f, 0.18f, 1.0f));

	// 패드(UI-PAD-01): 목록에서 → 는 가운데 카드를 건너뛰고 오른쪽 [출격]으로, [출격]에서 ← 는 고른 미션으로 돌아온다.
	// 위치 계산에만 맡기면 3열 사이 이동이 버튼 높이에 따라 달라진다.
	if (StartMissionButton)
	{
		Button->SetNavigationRuleExplicit(EUINavigation::Right, StartMissionButton);
		if (bSelected)
		{
			StartMissionButton->SetNavigationRuleExplicit(EUINavigation::Left, Button);
		}
	}

	// 같은 글자를 다시 넣어도 Slate가 Layout을 무효화하므로, 실제로 바뀐 경우에만 넣는다.
	FText DisplayName = Mission ? Mission->DisplayName : FText::FromName(MissionId);
	if (FlowSubsystem.IsValid() && FlowSubsystem->IsMissionCompleted(MissionId))
	{
		// 이번 실행에서 성공한 미션(TUT-PROGRESS-01). 영구 저장은 현재 미정.
		DisplayName = FText::Format(FText::FromString(TEXT("{0}  · 완료")), DisplayName);
	}
	if (!Label->GetText().EqualTo(DisplayName))
	{
		Label->SetText(DisplayName);
	}
}

void UDroneFrontEndRootWidget::ApplyMissionButtonLabelWrap(UTextBlock* Label) const
{
	if (!Label)
	{
		return;
	}
	if (MissionButtonLabelWrapWidth > 0.f)
	{
		Label->SetAutoWrapText(false);
		Label->SetWrapTextAt(MissionButtonLabelWrapWidth);
	}
	else
	{
		// 고정 폭에서 AutoWrap으로 돌아갈 때 이전 WrapTextAt이 남아 있으면 그 폭에서 먼저 꺾이므로 지운다.
		Label->SetWrapTextAt(0.f);
		Label->SetAutoWrapText(true);
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
