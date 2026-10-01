#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "Mission/DroneMissionDefinition.h"
#include "DroneFrontEndRootWidget.generated.h"

class UButton;
class UImage;
class UTexture2D;
class UDroneGameFlowSubsystem;
class UDroneMissionDefinition;
class UDroneMissionSelectionButton;
class UTextBlock;
class UVerticalBox;
class UWidget;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneMissionMapLoadRequestedSignature,
	UDroneMissionDefinition*, MissionDefinition);

/**
 * 시작 트레일러 대체 화면과 로비의 단일 Front-end Widget Host다.
 *
 * C++는 상태 구독, 버튼 전환과 중복 방지를 담당한다. Widget Blueprint는 같은 이름의
 * 선택 위젯을 배치해 외형만 교체할 수 있으며, Designer가 비어 있으면 학습용 기본 UI가
 * 자동으로 만들어진다. 실제 영상 Asset이 없으면 같은 Mission 데이터의 정적 Briefing을 표시한다.
 */
UCLASS(Blueprintable)
class DRONE_API UDroneFrontEndRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** WBP Class Defaults에서 드래그하여 교체한다. 원본 PNG 파일 경로에 의존하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Artwork")
	TObjectPtr<UTexture2D> TitleBackgroundTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Artwork")
	TObjectPtr<UTexture2D> TitleOverlayTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Artwork")
	TObjectPtr<UTexture2D> TitleLogoTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Artwork")
	TObjectPtr<UTexture2D> ButtonNormalTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Artwork")
	TObjectPtr<UTexture2D> ButtonHoveredTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Artwork")
	TObjectPtr<UTexture2D> ButtonPressedTexture;

	/** 제공 버튼 PNG에는 투명 여백이 있다. 새 PNG에 여백이 없으면 이 옵션을 끈다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Artwork")
	bool bUseProvidedButtonAtlasRegions = true;

	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Artwork")
	void RefreshArtwork();

	/** 사운드 소스가 지급되면 WBP에서 연결한다. 미지정 상태는 무음이며 임의 사운드는 만들지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Feedback")
	TObjectPtr<USoundBase> ButtonHoverSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Front End|Feedback")
	TObjectPtr<USoundBase> ButtonClickSound;

	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Settings")
	void SetSettingsVisible(bool bVisible);
	UFUNCTION(BlueprintPure, Category="Drone|Front End|Settings")
	bool IsSettingsVisible() const { return bSettingsVisible; }

	/** Auto는 선택 가능한 탭이 아니다. 변경 후 숨겨진 탭의 선택은 시작할 수 없다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Lobby")
	bool SetLobbyCategory(EDroneMissionCategory Category);

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	EDroneMissionCategory GetLobbyCategory() const { return ActiveLobbyCategory; }

	/** Designer에 자체 목록을 만들 때 이 배열로 버튼을 만들고 SelectLobbyMission을 호출한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	TArray<FName> GetVisibleMissionIds() const;

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

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Debug")
	int32 GetNativeMissionButtonCount() const { return NativeMissionButtons.Num(); }

	/** FLOW-03 로비 목록 선택 경계. 표시 Text는 선택한 Mission Definition에서만 읽는다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Lobby")
	bool SelectLobbyMission(FName MissionId);

	/** 선택한 Mission을 확정해 정적 Briefing 또는 추후 영상이 표시될 MissionTrailer 상태로 넘긴다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Lobby")
	bool ConfirmSelectedMission();

	/** 정적 Briefing의 작전 시작 버튼과 추후 영상 종료 Callback이 함께 사용하는 Map 진입 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Briefing")
	bool FinishMissionBriefing();

	/** 설정 닫기 / 설명→로비 / 로비→시작. 최종 WBP 버튼에서도 호출 가능하다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Front End|Navigation")
	bool NavigateBack();

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	FText GetDisplayedMissionName() const { return DisplayedMissionName; }

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	FText GetDisplayedMissionDescription() const { return DisplayedMissionDescription; }

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	FText GetDisplayedMissionMeta() const { return DisplayedMissionMeta; }

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Briefing")
	FText GetDisplayedBriefingTitle() const { return DisplayedBriefingTitle; }

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Briefing")
	FText GetDisplayedBriefingBody() const { return DisplayedBriefingBody; }

	/** PlayerController만 이 요청을 받아 실제 OpenLevel을 수행한다. Widget은 Map 수명을 소유하지 않는다. */
	UPROPERTY(BlueprintAssignable, Category="Drone|Front End|Briefing")
	FDroneMissionMapLoadRequestedSignature OnMissionMapLoadRequested;

	/** 최종 WBP가 Animation/영상/전환 표현을 붙이는 지점이며 Flow 상태를 바꾸지는 않는다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Front End", meta=(DisplayName="On Front End State Displayed"))
	void ReceiveFrontEndStateDisplayed(EDroneGameFlowState State);

	/** WBP가 선택 강조·Thumbnail Animation을 표현하는 Event다. 선택 판정은 C++에서 끝난 뒤 호출한다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Front End|Lobby", meta=(DisplayName="On Lobby Mission Selection Changed"))
	void ReceiveLobbyMissionSelectionChanged(UDroneMissionDefinition* Definition);

	UFUNCTION(BlueprintImplementableEvent, Category="Drone|Front End|Lobby")
	void ReceiveLobbyCategoryChanged(EDroneMissionCategory Category);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleBackClicked();
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> LobbyBackButton;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> BriefingBackButton;
	UFUNCTION()
	void HandleSettingsClicked();
	UFUNCTION()
	void HandleSettingsBackClicked();
	UFUNCTION()
	void HandleLowQualityClicked();
	UFUNCTION()
	void HandleMediumQualityClicked();
	UFUNCTION()
	void HandleHighQualityClicked();
	UFUNCTION()
	void HandleTitleHovered();
	UFUNCTION()
	void HandleTitleClickFeedback();
	void ApplyGraphicsQuality(int32 Quality);

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> SettingsPanel;
	UPROPERTY(Transient)
	bool bSettingsVisible = false;
	UFUNCTION()
	void HandleTutorialTabClicked();
	UFUNCTION()
	void HandleRacingTabClicked();
	UFUNCTION()
	void HandleMissionTabClicked();
	UFUNCTION()
	void HandleExitClicked();
	UFUNCTION()
	void HandleFlowStateChanged(EDroneGameFlowState PreviousState, EDroneGameFlowState NewState);

	UFUNCTION()
	void HandleFlowSnapshotChanged(const FDroneGameFlowSnapshot& Snapshot);

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleFirstMissionClicked();

	UFUNCTION()
	void HandleMissionButtonSelected(FName MissionId);

	UFUNCTION()
	void HandleStartMissionClicked();

	UFUNCTION()
	void HandleFinishBriefingClicked();

	void BuildDefaultLayout();
	bool TryBindBlueprintLayout();
	void ApplyDisplayedState(EDroneGameFlowState State);
	void RefreshLobbyContent();
	void RebuildNativeMissionButtons(const TArray<FName>& MissionIds);
	void RefreshMissionBriefingContent();
	void ClearFlowBinding();
	void ApplyTitleButtonStyle(UButton* Button);

	UPROPERTY(Transient)
	EDroneMissionCategory ActiveLobbyCategory = EDroneMissionCategory::Tutorial;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> TitleBackgroundImage;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> TitleOverlayImage;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> TitleLogoImage;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> MissionThumbnailImage;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> TutorialTabButton;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> RacingTabButton;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> MissionTabButton;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ExitButton;

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

	UPROPERTY(Transient)
	FText DisplayedBriefingTitle;

	UPROPERTY(Transient)
	FText DisplayedBriefingBody;

	/** WBP Designer에서 같은 이름을 쓰면 C++ 수명·전환 로직을 그대로 재사용한다. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> OpeningPanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> LobbyPanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MissionBriefingPanel;

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

	/** Native fallback만 동적으로 다시 만드는 Mission 버튼 목록이다. WBP는 SelectLobbyMission API를 그대로 쓴다. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> MissionButtonsColumn;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDroneMissionSelectionButton>> NativeMissionButtons;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionNameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionDescriptionText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionMetaText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> StartMissionButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionBriefingTitleText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionBriefingBodyText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> FinishMissionBriefingButton;

	UPROPERTY(Transient)
	bool bUsingNativeFallbackLayout = false;
};
