#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "UI/DroneGamepadFocus.h"
#include "DroneSettingsWidget.generated.h"

class UCheckBox;
class UComboBoxString;
class USlider;
class UTextBlock;
class UDroneAudioSettingsSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDroneSettingsCloseRequestedSignature);

/** 설정 화면. 소리만 즉시 미리보기하고 영상/성능 값은 적용 버튼으로 확정한다. */
UCLASS(Blueprintable)
class DRONE_API UDroneSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category="Drone|Settings")
	FDroneSettingsCloseRequestedSignature OnCloseRequested;

	/** Root가 설정을 열 때 호출한다. 기존 설정을 읽고 새 편집 세션을 시작한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Settings")
	void RefreshFromCurrentSettings();

	/** Root의 ESC/뒤로가기 경로에서도 호출해 미적용 사운드 미리보기를 되돌린다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Settings")
	void CancelPendingSettings();

	/** 설정 화면이 열릴 때 Root가 부른다. 다음 Tick부터 첫 조작 항목(음량 슬라이더)에 패드 포커스를 준다(UI-PAD-01). */
	void RequestGamepadFocus();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Settings|Gamepad", meta=(ClampMin="1.0", ClampMax="1.3"))
	float GamepadFocusScale = 1.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Settings|Gamepad")
	FLinearColor GamepadFocusTint = FLinearColor(1.0f, 0.86f, 0.42f, 1.0f);

	UWidget* GetGamepadHighlightedWidget() const { return GamepadFocus.GetHighlighted(); }
	/** 위→아래 조작 순서: 음량, 화면 모드, 해상도, 품질, VSync, FPS, 기본값·적용·뒤로. */
	TArray<UWidget*> GetGamepadControls() const;

	UFUNCTION(BlueprintCallable, Category="Drone|Settings")
	bool ApplyPendingSettings();

	UFUNCTION(BlueprintCallable, Category="Drone|Settings")
	void RestoreDefaults();

	UFUNCTION(BlueprintCallable, Category="Drone|Settings")
	void RequestClose();

	UFUNCTION(BlueprintCallable, Category="Drone|Settings|Audio")
	void SetMasterVolume(float InVolume);

	UFUNCTION(BlueprintPure, Category="Drone|Settings|Audio")
	float GetMasterVolume() const { return PendingMasterVolume; }

	UFUNCTION(BlueprintPure, Category="Drone|Settings")
	int32 GetPendingGraphicsQuality() const { return PendingGraphicsQuality; }

	UFUNCTION(BlueprintPure, Category="Drone|Settings")
	float GetPendingFrameRateLimit() const { return PendingFrameRateLimit; }

	UFUNCTION(BlueprintPure, Category="Drone|Settings")
	bool IsPendingVSyncEnabled() const { return bPendingVSync; }

	UFUNCTION(BlueprintPure, Category="Drone|Settings")
	FIntPoint GetPendingResolution() const { return PendingResolution; }

	/** PIE/Designer에서는 창 모드와 해상도 변경을 비활성화한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Settings")
	bool AreDisplaySettingsAvailable() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FDroneGamepadFocus GamepadFocus;
	void BuildLayout();
	void UpdateControls();
	UDroneAudioSettingsSubsystem* GetAudioSettings() const;

	UFUNCTION()
	void HandleVolumeChanged(float Value);
	UFUNCTION()
	void HandleDisplayModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleGraphicsChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleVSyncChanged(bool bChecked);
	UFUNCTION()
	void HandleFrameRateChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleApplyClicked();

	UPROPERTY(Transient)
	TObjectPtr<USlider> MasterVolumeSlider;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MasterVolumeText;
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> DisplayModeCombo;
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> ResolutionCombo;
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> GraphicsCombo;
	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> VSyncCheck;
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> FrameRateCombo;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	float PendingMasterVolume = 1.0f;
	float InitialMasterVolume = 1.0f;
	int32 PendingDisplayMode = 1;
	FIntPoint PendingResolution = FIntPoint(1920, 1080);
	int32 PendingGraphicsQuality = 1;
	float PendingFrameRateLimit = 60.0f;
	bool bPendingVSync = true;
	bool bUpdatingControls = false;
	bool bAudioDirty = false;
	bool bDisplayModeDirty = false;
	bool bResolutionDirty = false;
	bool bGraphicsDirty = false;
	bool bVSyncDirty = false;
	bool bFrameRateDirty = false;
};
