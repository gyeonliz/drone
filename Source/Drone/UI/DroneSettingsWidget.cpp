#include "UI/DroneSettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Styling/CoreStyle.h"
#include "UI/DroneAudioSettingsSubsystem.h"

namespace DroneSettingsUI
{
const FLinearColor TextColor(0.90f, 0.94f, 0.95f);
const FLinearColor MutedColor(0.58f, 0.67f, 0.71f);
const FLinearColor AccentColor(0.23f, 0.79f, 0.76f);

FString ResolutionLabel(const FIntPoint Resolution)
{
	return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
}

FString GraphicsLabel(const int32 Quality)
{
	switch (Quality)
	{
	case 0: return TEXT("낮음");
	case 1: return TEXT("보통");
	case 2: return TEXT("높음");
	case 3: return TEXT("최고 (현재 설정)");
	case 4: return TEXT("시네마틱 (현재 설정)");
	default: return TEXT("사용자 지정 (현재 설정)");
	}
}

FString FrameRateLabel(const float Limit)
{
	return Limit <= 0.0f ? FString(TEXT("무제한")) : FString::Printf(TEXT("%g FPS"), Limit);
}

FString DisplayModeLabel(const int32 Mode)
{
	if (Mode == static_cast<int32>(EWindowMode::Fullscreen)) return TEXT("전체 화면");
	if (Mode == static_cast<int32>(EWindowMode::Windowed)) return TEXT("창 모드");
	return TEXT("테두리 없는 창");
}
}

void UDroneSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
	RefreshFromCurrentSettings();
}

void UDroneSettingsWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime())
	{
		BuildLayout();
		UpdateControls();
	}
}

UDroneAudioSettingsSubsystem* UDroneSettingsWidget::GetAudioSettings() const
{
	UGameInstance* Instance = GetGameInstance();
	return Instance ? Instance->GetSubsystem<UDroneAudioSettingsSubsystem>() : nullptr;
}

bool UDroneSettingsWidget::AreDisplaySettingsAvailable() const
{
	const UWorld* World = GetWorld();
	return World && World->WorldType == EWorldType::Game && !IsDesignTime();
}

void UDroneSettingsWidget::RefreshFromCurrentSettings()
{
	if (const UDroneAudioSettingsSubsystem* Audio = GetAudioSettings()) PendingMasterVolume = Audio->GetMasterVolume();
	InitialMasterVolume = PendingMasterVolume;
	if (const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		PendingDisplayMode = static_cast<int32>(Settings->GetFullscreenMode());
		PendingResolution = Settings->GetScreenResolution();
		PendingGraphicsQuality = Settings->GetOverallScalabilityLevel();
		PendingFrameRateLimit = Settings->GetFrameRateLimit();
		bPendingVSync = Settings->IsVSyncEnabled();
	}
	bAudioDirty = bDisplayModeDirty = bResolutionDirty = bGraphicsDirty = bVSyncDirty = bFrameRateDirty = false;
	UpdateControls();
	if (StatusText) StatusText->SetText(FText::FromString(AreDisplaySettingsAvailable()
		? TEXT("전체 음량은 즉시 미리보기됩니다. 변경 후 적용을 눌러 저장하세요.")
		: TEXT("PIE에서는 창 모드·해상도를 변경할 수 없습니다. 해당 항목은 Standalone 실행에서 적용하세요.")));
}

void UDroneSettingsWidget::CancelPendingSettings()
{
	if (bAudioDirty)
	{
		if (UDroneAudioSettingsSubsystem* Audio = GetAudioSettings()) Audio->SetMasterVolume(InitialMasterVolume);
		PendingMasterVolume = InitialMasterVolume;
	}
	RefreshFromCurrentSettings();
}

void UDroneSettingsWidget::SetMasterVolume(const float InVolume)
{
	if (!FMath::IsFinite(InVolume)) return;
	PendingMasterVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	bAudioDirty = !FMath::IsNearlyEqual(PendingMasterVolume, InitialMasterVolume);
	if (UDroneAudioSettingsSubsystem* Audio = GetAudioSettings()) Audio->SetMasterVolume(PendingMasterVolume);
	const bool bWasUpdating = bUpdatingControls;
	bUpdatingControls = true;
	if (MasterVolumeSlider) MasterVolumeSlider->SetValue(PendingMasterVolume);
	if (MasterVolumeText) MasterVolumeText->SetText(FText::AsPercent(PendingMasterVolume));
	bUpdatingControls = bWasUpdating;
}

bool UDroneSettingsWidget::ApplyPendingSettings()
{
	const bool bAnyGraphicsDirty = bDisplayModeDirty || bResolutionDirty || bGraphicsDirty || bVSyncDirty || bFrameRateDirty;
	if (bAnyGraphicsDirty)
	{
		UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
		if (!Settings)
		{
			if (StatusText) StatusText->SetText(FText::FromString(TEXT("게임 설정을 불러올 수 없어 적용하지 못했습니다.")));
			return false;
		}
		if (bGraphicsDirty && PendingGraphicsQuality >= 0) Settings->SetOverallScalabilityLevel(PendingGraphicsQuality);
		if (bVSyncDirty) Settings->SetVSyncEnabled(bPendingVSync);
		if (bFrameRateDirty) Settings->SetFrameRateLimit(PendingFrameRateLimit);
		if (AreDisplaySettingsAvailable() && (bDisplayModeDirty || bResolutionDirty))
		{
			if (bDisplayModeDirty) Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(PendingDisplayMode));
			if (bResolutionDirty) Settings->SetScreenResolution(PendingResolution);
			Settings->ApplySettings(false);
			Settings->ConfirmVideoMode();
			Settings->SaveSettings();
		}
		else
		{
			Settings->ApplyNonResolutionSettings();
			Settings->SaveSettings();
		}
	}
	if (bAudioDirty)
	{
		UDroneAudioSettingsSubsystem* Audio = GetAudioSettings();
		if (!Audio || !Audio->SaveMasterVolume())
		{
			if (StatusText) StatusText->SetText(FText::FromString(TEXT("전체 음량을 저장하지 못했습니다. 변경값을 유지했으니 다시 적용해주세요.")));
			return false;
		}
	}
	RefreshFromCurrentSettings();
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("설정을 적용하고 저장했습니다.")));
	return true;
}

void UDroneSettingsWidget::RestoreDefaults()
{
	SetMasterVolume(1.0f);
	// 프로젝트 기본값을 편집 값으로 준비한다. 적용 전에는 엔진 설정을 변경하지 않는다.
	if (AreDisplaySettingsAvailable())
	{
		PendingDisplayMode = static_cast<int32>(EWindowMode::WindowedFullscreen);
		PendingResolution = FIntPoint(1920, 1080);
		bDisplayModeDirty = bResolutionDirty = true;
	}
	PendingGraphicsQuality = 1;
	PendingFrameRateLimit = 60.0f;
	bPendingVSync = true;
	bGraphicsDirty = bFrameRateDirty = bVSyncDirty = true;
	UpdateControls();
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("기본값을 선택했습니다. 적용을 누르면 저장됩니다.")));
}

void UDroneSettingsWidget::RequestClose()
{
	CancelPendingSettings();
	OnCloseRequested.Broadcast();
}

void UDroneSettingsWidget::HandleVolumeChanged(const float Value)
{
	if (!bUpdatingControls) SetMasterVolume(Value);
}

void UDroneSettingsWidget::HandleDisplayModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls || !AreDisplaySettingsAvailable()) return;
	PendingDisplayMode = SelectedItem == TEXT("전체 화면") ? static_cast<int32>(EWindowMode::Fullscreen)
		: SelectedItem == TEXT("창 모드") ? static_cast<int32>(EWindowMode::Windowed)
		: static_cast<int32>(EWindowMode::WindowedFullscreen);
	bDisplayModeDirty = true;
}

void UDroneSettingsWidget::HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls || !AreDisplaySettingsAvailable()) return;
	FString Width, Height;
	if (SelectedItem.Split(TEXT(" x "), &Width, &Height))
	{
		const FIntPoint NewResolution(FCString::Atoi(*Width), FCString::Atoi(*Height));
		if (NewResolution.X > 0 && NewResolution.Y > 0)
		{
			PendingResolution = NewResolution;
			bResolutionDirty = true;
		}
	}
}

void UDroneSettingsWidget::HandleGraphicsChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls) return;
	for (int32 Quality = -1; Quality <= 4; ++Quality)
	{
		if (SelectedItem == DroneSettingsUI::GraphicsLabel(Quality))
		{
			PendingGraphicsQuality = Quality;
			bGraphicsDirty = Quality >= 0;
			break;
		}
	}
}

void UDroneSettingsWidget::HandleVSyncChanged(const bool bChecked)
{
	if (bUpdatingControls) return;
	bPendingVSync = bChecked;
	bVSyncDirty = true;
}

void UDroneSettingsWidget::HandleFrameRateChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls) return;
	PendingFrameRateLimit = SelectedItem == TEXT("무제한") ? 0.0f : FCString::Atof(*SelectedItem);
	bFrameRateDirty = true;
}

void UDroneSettingsWidget::HandleApplyClicked()
{
	ApplyPendingSettings();
}

void UDroneSettingsWidget::UpdateControls()
{
	bUpdatingControls = true;
	if (MasterVolumeSlider) MasterVolumeSlider->SetValue(PendingMasterVolume);
	if (MasterVolumeText) MasterVolumeText->SetText(FText::AsPercent(PendingMasterVolume));
	if (DisplayModeCombo)
	{
		DisplayModeCombo->SetSelectedOption(DroneSettingsUI::DisplayModeLabel(PendingDisplayMode));
		DisplayModeCombo->SetIsEnabled(AreDisplaySettingsAvailable());
	}
	if (ResolutionCombo)
	{
		const FString Label = DroneSettingsUI::ResolutionLabel(PendingResolution);
		if (ResolutionCombo->FindOptionIndex(Label) == INDEX_NONE) ResolutionCombo->AddOption(Label);
		ResolutionCombo->SetSelectedOption(Label);
		ResolutionCombo->SetIsEnabled(AreDisplaySettingsAvailable());
	}
	if (GraphicsCombo)
	{
		const FString Label = DroneSettingsUI::GraphicsLabel(PendingGraphicsQuality);
		if (GraphicsCombo->FindOptionIndex(Label) == INDEX_NONE) GraphicsCombo->AddOption(Label);
		GraphicsCombo->SetSelectedOption(Label);
	}
	if (VSyncCheck) VSyncCheck->SetIsChecked(bPendingVSync);
	if (FrameRateCombo)
	{
		const FString Label = DroneSettingsUI::FrameRateLabel(PendingFrameRateLimit);
		if (FrameRateCombo->FindOptionIndex(Label) == INDEX_NONE) FrameRateCombo->AddOption(Label);
		FrameRateCombo->SetSelectedOption(Label);
	}
	bUpdatingControls = false;
}

void UDroneSettingsWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;
	using namespace DroneSettingsUI;
	USizeBox* Outer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SettingsWidth"));
	Outer->SetWidthOverride(900.0f);
	WidgetTree->RootWidget = Outer;
	UBorder* Surface = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsSurface"));
	Surface->SetBrushColor(FLinearColor(0.035f, 0.055f, 0.07f, 0.97f));
	Surface->SetPadding(FMargin(36.0f, 28.0f));
	Outer->AddChild(Surface);
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsContent"));
	Surface->AddChild(Content);

	auto MakeText = [this](const FString& Label, const int32 Size, const FLinearColor Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	};
	auto AddSection = [Content, &MakeText](const FString& Title)
	{
		UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(MakeText(Title, 16, AccentColor));
		Slot->SetPadding(FMargin(0.0f, 17.0f, 0.0f, 6.0f));
	};
	auto AddRow = [this, Content, &MakeText](const FString& Label, UWidget* Control)
	{
		USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Height->SetMinDesiredHeight(47.0f);
		Content->AddChildToVerticalBox(Height)->SetPadding(FMargin(0.0f, 2.0f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Height->AddChild(Row);
		USizeBox* LabelWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelWidth->SetWidthOverride(225.0f);
		LabelWidth->AddChild(MakeText(Label, 20, TextColor));
		Row->AddChildToHorizontalBox(LabelWidth)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* ControlSlot = Row->AddChildToHorizontalBox(Control);
		ControlSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ControlSlot->SetVerticalAlignment(VAlign_Center);
	};
	auto MakeCombo = [this](const FName Name)
	{
		UComboBoxString* Combo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), Name);
		Combo->SetMaxListHeight(300.0f);
		return Combo;
	};
	Content->AddChildToVerticalBox(MakeText(TEXT("설정"), 34, TextColor));
	Content->AddChildToVerticalBox(MakeText(TEXT("비행 환경을 원하는 방식으로 맞춰보세요."), 17, MutedColor))->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

	AddSection(TEXT("사운드"));
	UHorizontalBox* AudioRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MasterAudioRow"));
	MasterVolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("MasterVolumeSlider"));
	MasterVolumeSlider->SetMinValue(0.0f);
	MasterVolumeSlider->SetMaxValue(1.0f);
	MasterVolumeSlider->SetStepSize(0.01f);
	MasterVolumeSlider->SetSliderBarColor(MutedColor);
	MasterVolumeSlider->SetSliderHandleColor(AccentColor);
	AudioRow->AddChildToHorizontalBox(MasterVolumeSlider)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MasterVolumeText = MakeText(TEXT("100%"), 20, TextColor);
	USizeBox* PercentWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PercentWidth->SetWidthOverride(76.0f);
	PercentWidth->AddChild(MasterVolumeText);
	AudioRow->AddChildToHorizontalBox(PercentWidth)->SetPadding(FMargin(18.0f, 0.0f, 0.0f, 0.0f));
	AddRow(TEXT("전체 음량"), AudioRow);
	MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UDroneSettingsWidget::HandleVolumeChanged);

	AddSection(TEXT("화면"));
	DisplayModeCombo = MakeCombo(TEXT("DisplayModeCombo"));
	for (const FString& Option : TArray<FString>{TEXT("창 모드"), TEXT("테두리 없는 창"), TEXT("전체 화면")}) DisplayModeCombo->AddOption(Option);
	AddRow(TEXT("화면 모드"), DisplayModeCombo);
	DisplayModeCombo->OnSelectionChanged.AddUniqueDynamic(this, &UDroneSettingsWidget::HandleDisplayModeChanged);
	ResolutionCombo = MakeCombo(TEXT("ResolutionCombo"));
	for (const FIntPoint Resolution : TArray<FIntPoint>{FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1440)}) ResolutionCombo->AddOption(ResolutionLabel(Resolution));
	AddRow(TEXT("해상도"), ResolutionCombo);
	ResolutionCombo->OnSelectionChanged.AddUniqueDynamic(this, &UDroneSettingsWidget::HandleResolutionChanged);

	AddSection(TEXT("성능"));
	GraphicsCombo = MakeCombo(TEXT("GraphicsCombo"));
	for (int32 Quality = 0; Quality <= 2; ++Quality) GraphicsCombo->AddOption(GraphicsLabel(Quality));
	AddRow(TEXT("그래픽 품질"), GraphicsCombo);
	GraphicsCombo->OnSelectionChanged.AddUniqueDynamic(this, &UDroneSettingsWidget::HandleGraphicsChanged);
	FrameRateCombo = MakeCombo(TEXT("FrameRateCombo"));
	for (const float Limit : TArray<float>{0.0f, 30.0f, 60.0f, 120.0f}) FrameRateCombo->AddOption(FrameRateLabel(Limit));
	AddRow(TEXT("프레임 제한"), FrameRateCombo);
	FrameRateCombo->OnSelectionChanged.AddUniqueDynamic(this, &UDroneSettingsWidget::HandleFrameRateChanged);
	VSyncCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("VSyncCheck"));
	VSyncCheck->AddChild(MakeText(TEXT("켜기 · 화면 찢어짐 방지"), 18, TextColor));
	AddRow(TEXT("수직 동기화"), VSyncCheck);
	VSyncCheck->OnCheckStateChanged.AddUniqueDynamic(this, &UDroneSettingsWidget::HandleVSyncChanged);

	StatusText = MakeText(TEXT("변경 후 적용을 눌러 저장하세요."), 15, MutedColor);
	StatusText->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 16.0f));
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsActions"));
	Content->AddChildToVerticalBox(Actions);
	auto MakeButton = [this, &MakeText](const FName Name, const FString& Label, const FLinearColor Color)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Color);
		Button->AddChild(MakeText(Label, 20, TextColor));
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(170.0f);
		Size->SetHeightOverride(48.0f);
		Size->AddChild(Button);
		return TPair<UButton*, USizeBox*>(Button, Size);
	};
	const auto Defaults = MakeButton(TEXT("RestoreDefaultsButton"), TEXT("기본값 복원"), FLinearColor(0.11f, 0.17f, 0.20f));
	Actions->AddChildToHorizontalBox(Defaults.Value);
	Defaults.Key->OnClicked.AddUniqueDynamic(this, &UDroneSettingsWidget::RestoreDefaults);
	USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Actions->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	const auto Apply = MakeButton(TEXT("ApplySettingsButton"), TEXT("적용"), FLinearColor(0.07f, 0.46f, 0.44f));
	Actions->AddChildToHorizontalBox(Apply.Value)->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
	Apply.Key->OnClicked.AddUniqueDynamic(this, &UDroneSettingsWidget::HandleApplyClicked);
	const auto Back = MakeButton(TEXT("SettingsBackButton"), TEXT("뒤로"), FLinearColor(0.11f, 0.17f, 0.20f));
	Actions->AddChildToHorizontalBox(Back.Value);
	Back.Key->OnClicked.AddUniqueDynamic(this, &UDroneSettingsWidget::RequestClose);
}
