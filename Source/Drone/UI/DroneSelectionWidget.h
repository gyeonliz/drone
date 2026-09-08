#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "Prototype/DroneFlightControlTypes.h"
#include "DroneSelectionWidget.generated.h"

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

	/** Mission이 허용하고 현재 구현된 기체만 Flow가 승인한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Selection")
	bool SelectDrone(FName DroneId);

	UFUNCTION(BlueprintCallable, Category="Drone|Selection|Control")
	void ToggleControlMode();

	UFUNCTION(BlueprintCallable, Category="Drone|Selection|Control")
	void CycleHandlingPreset();

	/** Controller가 Spawn과 Possess까지 모두 성공시킨 경우에만 true다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Selection")
	bool ConfirmAndLaunchSelectedDrone();

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

private:
	UFUNCTION()
	void HandleFlowSnapshotChanged(const FDroneGameFlowSnapshot& Snapshot);

	UFUNCTION()
	void HandleDroneButton0Clicked();

	UFUNCTION()
	void HandleDroneButton1Clicked();

	UFUNCTION()
	void HandleDroneButton2Clicked();

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
