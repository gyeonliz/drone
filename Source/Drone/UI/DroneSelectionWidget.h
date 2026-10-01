#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "Prototype/DroneFlightControlTypes.h"
#include "UI/DroneGamepadFocus.h"
#include "DroneSelectionWidget.generated.h"

class UBorder;
class UButton;
class UDroneDefinition;
class UDroneGameFlowSubsystem;
class UTextBlock;
class UWidget;

/**
 * Mission Map에 진입한 뒤 허용된 기체와 조작 설정을 고르는 FLOW-05 화면이다.
 *
 * 선택 가능 여부와 실제 Definition은 GameInstance Flow가 소유한다. 이 Widget은 화면 표시와
 * 입력 전달만 담당하며, 최종 Pawn Spawn/Possess는 Mission PlayerController가 한 번만 수행한다.
 */
UCLASS(Blueprintable)
class DRONE_API UDroneSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Drone|Selection")
	void SetFlowSubsystem(UDroneGameFlowSubsystem* InFlowSubsystem);

	UFUNCTION(BlueprintPure, Category="Drone|Selection")
	UDroneGameFlowSubsystem* GetFlowSubsystem() const { return FlowSubsystem.Get(); }

	/** 패드 포커스 강조(UI-PAD-01). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Selection|Gamepad", meta=(ClampMin="1.0", ClampMax="1.3"))
	float GamepadFocusScale = 1.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Selection|Gamepad")
	FLinearColor GamepadFocusTint = FLinearColor(1.0f, 0.86f, 0.42f, 1.0f);

	UWidget* GetGamepadHighlightedWidget() const { return GamepadFocus.GetHighlighted(); }
	UButton* GetDroneButton(int32 Index) const { return DroneButtons.IsValidIndex(Index) ? DroneButtons[Index].Get() : nullptr; }
	FName GetDisplayedDroneId(int32 Index) const { return DisplayedDroneButtonIds.IsValidIndex(Index) ? DisplayedDroneButtonIds[Index] : NAME_None; }
	int32 GetDroneButtonCount() const { return DroneButtons.Num(); }

	/** Mission이 허용하고 현재 구현된 기체만 Flow가 승인한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Selection")
	bool SelectDrone(FName DroneId);

	UFUNCTION(BlueprintCallable, Category="Drone|Selection|Control")
	void ToggleControlMode();

	/** Legacy Blueprint 호출 호환용이다. 속도 단계는 제거되어 항상 단일 기본 성능을 유지한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Selection|Control")
	void CycleHandlingPreset();

	/** Controller가 Spawn과 Possess까지 모두 성공시킨 경우에만 true다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Selection")
	bool ConfirmAndLaunchSelectedDrone();

	UFUNCTION(BlueprintCallable, Category="Drone|Selection|Navigation")
	bool NavigateBack();

	UFUNCTION(BlueprintPure, Category="Drone|Selection")
	FName GetDisplayedDroneId() const { return DisplayedDroneId; }

	UFUNCTION(BlueprintPure, Category="Drone|Selection")
	FText GetDisplayedDroneName() const { return DisplayedDroneName; }

	UFUNCTION(BlueprintPure, Category="Drone|Selection")
	FText GetDisplayedDroneDescription() const { return DisplayedDroneDescription; }

	UFUNCTION(BlueprintPure, Category="Drone|Selection|Control")
	EDroneControlMode GetSelectedControlMode() const { return SelectedControlMode; }

	UFUNCTION(BlueprintPure, Category="Drone|Selection|Control")
	EDroneHandlingPreset GetSelectedHandlingPreset() const { return SelectedHandlingPreset; }

	UFUNCTION(BlueprintPure, Category="Drone|Selection")
	bool IsUsingNativeFallbackLayout() const { return bUsingNativeFallbackLayout; }

	/** 최종 WBP가 카드 강조와 Preview 연출을 붙이는 지점이다. 판정은 이미 C++에서 끝난 상태다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Selection", meta=(DisplayName="On Drone Selection Refreshed"))
	void ReceiveDroneSelectionRefreshed(UDroneDefinition* Definition);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 기체 선택 화면에 들어올 때 한 번: 고른 기체 → 첫 기체 → 출격 → 뒤로 순으로 첫 포커스. */
	void RequestDroneSelectFocus(FName SelectedDroneId);
	TArray<UWidget*> GetGamepadHighlightables() const;
	FDroneGamepadFocus GamepadFocus;
	bool bDroneSelectFocusRequested = false;
	UFUNCTION()
	void HandleBackClicked();
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> SelectionBackButton;
	UFUNCTION()
	void HandleFlowSnapshotChanged(const FDroneGameFlowSnapshot& Snapshot);

	UFUNCTION()
	void HandleDroneButton0Clicked();

	UFUNCTION()
	void HandleDroneButton1Clicked();

	UFUNCTION()
	void HandleDroneButton2Clicked();

	UFUNCTION()
	void HandleDroneButton3Clicked();

	UFUNCTION()
	void HandleDroneButton4Clicked();

	UFUNCTION()
	void HandleControlModeClicked();

	UFUNCTION()
	void HandleHandlingPresetClicked();

	UFUNCTION()
	void HandleLaunchClicked();

	void BuildDefaultLayout();
	bool TryBindBlueprintLayout();
	void RefreshFromFlow();
	void RefreshControlLabels();
	void SelectDisplayedButton(int32 ButtonIndex);
	void ClearFlowBinding();

	TWeakObjectPtr<UDroneGameFlowSubsystem> FlowSubsystem;

	UPROPERTY(Transient)
	FName DisplayedDroneId = NAME_None;

	UPROPERTY(Transient)
	FText DisplayedDroneName;

	UPROPERTY(Transient)
	FText DisplayedDroneDescription;

	UPROPERTY(Transient)
	EDroneControlMode SelectedControlMode = EDroneControlMode::AssistedEasy;

	UPROPERTY(Transient)
	EDroneHandlingPreset SelectedHandlingPreset = EDroneHandlingPreset::Balanced;

	UPROPERTY(Transient)
	TArray<FName> DisplayedDroneButtonIds;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> DroneSelectionPanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionNameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DroneNameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DroneDescriptionText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DroneProfileText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> DroneButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DroneButtonTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> DroneCardBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DroneCardRoleTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DroneCardSelectionTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> DroneCardAirframes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> DroneCardGroundFrames;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DroneAirframePreview;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DroneGroundPreview;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DronePreviewRoleText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ControlModeButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ControlModeButtonText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> HandlingPresetButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HandlingPresetButtonText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> LaunchDroneButton;

	UPROPERTY(Transient)
	bool bUsingNativeFallbackLayout = false;
};
