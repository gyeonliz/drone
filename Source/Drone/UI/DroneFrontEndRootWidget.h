#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "DroneFrontEndRootWidget.generated.h"

class UButton;
class UDroneGameFlowSubsystem;
class UDroneMissionDefinition;
class UTextBlock;
class UWidget;

/**
 * 시작 트레일러 대체 화면과 로비의 단일 Front-end Widget Host다.
 *
 * C++는 상태 구독, 버튼 전환과 중복 방지를 담당한다. Widget Blueprint는 같은 이름의
 * 선택 위젯을 배치해 외형만 교체할 수 있으며, Designer가 비어 있으면 학습용 기본 UI가
 * 자동으로 만들어진다. 실제 영상 재생은 FLOW-04와 분리한다.
 */
UCLASS(Blueprintable)
class DRONE_API UDroneFrontEndRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이전 Flow 구독을 정리하고 GameInstance 수명의 새 Flow를 연결한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End")
	void SetFlowSubsystem(UDroneGameFlowSubsystem* InFlowSubsystem);

	UFUNCTION(BlueprintPure, Category="Drone|Front End")
	UDroneGameFlowSubsystem* GetFlowSubsystem() const { return FlowSubsystem.Get(); }

	/** 정적 대체 화면의 계속 버튼과 실제 Trailer 종료 Callback이 함께 사용하는 진입점이다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End")
	bool FinishOpeningTrailer();

	UFUNCTION(BlueprintPure, Category="Drone|Front End")
	EDroneGameFlowState GetDisplayedState() const { return DisplayedState; }

	UFUNCTION(BlueprintPure, Category="Drone|Front End")
	bool IsUsingNativeFallbackLayout() const { return bUsingNativeFallbackLayout; }

	/** FLOW-03 로비 목록 선택 경계. 표시 Text는 선택한 Mission Definition에서만 읽는다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Lobby")
	bool SelectLobbyMission(FName MissionId);

	/** 선택한 Mission을 확정해 MissionTrailer 상태로 넘긴다. 실제 영상/Map 이동은 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Lobby")
	bool ConfirmSelectedMission();

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	FText GetDisplayedMissionName() const { return DisplayedMissionName; }

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	FText GetDisplayedMissionDescription() const { return DisplayedMissionDescription; }

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	FText GetDisplayedMissionMeta() const { return DisplayedMissionMeta; }

	/** 최종 WBP가 Animation/영상/전환 표현을 붙이는 지점이며 Flow 상태를 바꾸지는 않는다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Front End", meta=(DisplayName="On Front End State Displayed"))
	void ReceiveFrontEndStateDisplayed(EDroneGameFlowState State);

	/** WBP가 선택 강조·Thumbnail Animation을 표현하는 Event다. 선택 판정은 C++에서 끝난 뒤 호출한다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Front End|Lobby", meta=(DisplayName="On Lobby Mission Selection Changed"))
	void ReceiveLobbyMissionSelectionChanged(UDroneMissionDefinition* Definition);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleFlowStateChanged(EDroneGameFlowState PreviousState, EDroneGameFlowState NewState);

	UFUNCTION()
	void HandleFlowSnapshotChanged(const FDroneGameFlowSnapshot& Snapshot);

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleFirstMissionClicked();

	UFUNCTION()
	void HandleStartMissionClicked();

	void BuildDefaultLayout();
	bool TryBindBlueprintLayout();
	void ApplyDisplayedState(EDroneGameFlowState State);
	void RefreshLobbyContent();
	void ClearFlowBinding();

	TWeakObjectPtr<UDroneGameFlowSubsystem> FlowSubsystem;

	UPROPERTY(Transient)
	EDroneGameFlowState DisplayedState = EDroneGameFlowState::Boot;

	UPROPERTY(Transient)
	FName FirstDisplayedMissionId = NAME_None;

	UPROPERTY(Transient)
	FText DisplayedMissionName;

	UPROPERTY(Transient)
	FText DisplayedMissionDescription;

	UPROPERTY(Transient)
	FText DisplayedMissionMeta;

	/** WBP Designer에서 같은 이름을 쓰면 C++ 수명·전환 로직을 그대로 재사용한다. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> OpeningPanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> LobbyPanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> OpeningTitleText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LobbyTitleText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LobbyStatusText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> MissionSelectButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionSelectButtonText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionNameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionDescriptionText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionMetaText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> StartMissionButton;

	UPROPERTY(Transient)
	bool bUsingNativeFallbackLayout = false;
};
