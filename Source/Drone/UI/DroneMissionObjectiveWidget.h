#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Mission/DroneMissionRuntimeTypes.h"
#include "DroneMissionObjectiveWidget.generated.h"

class ADroneMissionDirector;
class UTextBlock;
class UWidget;

/** Mission Director Snapshot Event만 구독하는 측면 목표 패널이다. Tick이나 Actor 검색을 하지 않는다. */
UCLASS(Blueprintable)
class DRONE_API UDroneMissionObjectiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Drone|Mission UI")
	void SetMissionDirector(ADroneMissionDirector* InMissionDirector);

	UFUNCTION(BlueprintCallable, Category="Drone|Mission UI")
	void ClearMissionDirector();

	UFUNCTION(BlueprintPure, Category="Drone|Mission UI")
	ADroneMissionDirector* GetMissionDirector() const { return MissionDirector.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|Mission UI")
	FDroneMissionRuntimeSnapshot GetDisplayedSnapshot() const { return DisplayedSnapshot; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission UI")
	FText GetObjectiveDisplayText() const { return ObjectiveDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission UI")
	FText GetProgressDisplayText() const { return ProgressDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission UI")
	bool IsUsingNativeFallbackLayout() const { return bUsingNativeFallbackLayout; }

	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Mission UI", meta=(DisplayName="On Objective Snapshot Displayed"))
	void ReceiveObjectiveSnapshotDisplayed(const FDroneMissionRuntimeSnapshot& Snapshot);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleMissionSnapshotChanged(const FDroneMissionRuntimeSnapshot& Snapshot);

	void BuildDefaultLayout();
	bool TryBindBlueprintLayout();
	void ApplySnapshot(const FDroneMissionRuntimeSnapshot& Snapshot);

	TWeakObjectPtr<ADroneMissionDirector> MissionDirector;

	UPROPERTY(Transient)
	FDroneMissionRuntimeSnapshot DisplayedSnapshot;

	UPROPERTY(Transient)
	FText ObjectiveDisplayText;

	UPROPERTY(Transient)
	FText ProgressDisplayText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MissionObjectivePanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionObjectiveTitleText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionObjectiveText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionObjectiveProgressText;

	UPROPERTY(Transient)
	bool bUsingNativeFallbackLayout = false;
};
