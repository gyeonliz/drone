#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "DroneMissionResultWidget.generated.h"

class ADroneMissionPlayerController;
class UButton;
class UTextBlock;
class UWidget;

/** Mission 성공/실패와 재도전·로비 복귀만 담당하는 FLOW-07 결과 화면이다. */
UCLASS(Blueprintable)
class DRONE_API UDroneMissionResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	void ConfigureResult(ADroneMissionPlayerController* InMissionController, EDroneMissionOutcome InOutcome);

	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool RequestRetry();

	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool RequestReturnToLobby();

	UFUNCTION(BlueprintPure, Category="Drone|Mission Result")
	EDroneMissionOutcome GetDisplayedOutcome() const { return DisplayedOutcome; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Result")
	FText GetResultDisplayText() const { return ResultDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Result")
	bool IsUsingNativeFallbackLayout() const { return bUsingNativeFallbackLayout; }

	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Mission Result", meta=(DisplayName="On Mission Result Displayed"))
	void ReceiveMissionResultDisplayed(EDroneMissionOutcome Outcome);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleReturnToLobbyClicked();

	void BuildDefaultLayout();
	bool TryBindBlueprintLayout();
	void RefreshResultDisplay();

	TWeakObjectPtr<ADroneMissionPlayerController> MissionController;

	UPROPERTY(Transient)
	EDroneMissionOutcome DisplayedOutcome = EDroneMissionOutcome::None;

	UPROPERTY(Transient)
	FText ResultDisplayText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MissionResultPanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionResultTitleText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> RetryMissionButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ReturnToLobbyButton;

	UPROPERTY(Transient)
	bool bUsingNativeFallbackLayout = false;
};
