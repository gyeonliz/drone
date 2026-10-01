#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "UI/DroneGamepadFocus.h"
#include "DroneMissionResultWidget.generated.h"

class ADroneMissionPlayerController;
class UDroneGameFlowSubsystem;
class UButton;
class UTextBlock;
class UWidget;

/**
 * Mission 성공/실패와 재도전·로비 복귀를 담당하는 FLOW-07 결과 화면이다.
 * TUT-PROGRESS-01: 클리어 시간, 이어진 과정의 진행도(수업 n/8), 성공 시 [다음]을 보여 준다.
 * 튜토리얼은 Figma 구성을 따른다.
 *  - 수업 완료(S48): "훈련 완료" / 수업 이름 / 시간 / [다음] [다시하기] (+ [로비])
 *  - 8개 모두 완료(S49): "훈련 완료" / "이제 운용 할 준비가 되었습니다." / [미션 진행] [시작 메뉴]
 *    이때 NextMissionButton이 [미션 진행], ReturnToLobbyButton이 [시작 메뉴] 역할을 하고 [다시하기]는 숨긴다.
 * WBP로 바꿀 때 MissionResultDetailText, NextMissionButton(+Text), RetryMissionButtonText, ReturnToLobbyButtonText는 선택 항목이다.
 */
UCLASS(Blueprintable)
class DRONE_API UDroneMissionResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	void ConfigureResult(ADroneMissionPlayerController* InMissionController, EDroneMissionOutcome InOutcome);

	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool RequestRetry();

	/** 패드 포커스 강조(UI-PAD-01). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Mission Result|Gamepad", meta=(ClampMin="1.0", ClampMax="1.3"))
	float GamepadFocusScale = 1.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Mission Result|Gamepad")
	FLinearColor GamepadFocusTint = FLinearColor(1.0f, 0.86f, 0.42f, 1.0f);

	UWidget* GetGamepadHighlightedWidget() const { return GamepadFocus.GetHighlighted(); }

	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool RequestReturnToLobby();

	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool RequestNextMission();

	/** 클리어 시간과 진행도 줄. */
	UFUNCTION(BlueprintPure, Category="Drone|Mission Result")
	FText GetResultDetailText() const { return ResultDetailText; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Result")
	bool IsNextMissionAvailable() const { return bNextMissionAvailable; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Result")
	FText GetNextMissionButtonLabel() const { return NextMissionButtonLabel; }

	/** 이어진 과정을 이번 실행에서 모두 성공했을 때 true. */
	UFUNCTION(BlueprintPure, Category="Drone|Mission Result")
	bool IsSequenceComplete() const { return bSequenceComplete; }

	/** 튜토리얼 8개를 모두 끝낸 전체 완료 화면(Figma S49)인지. */
	UFUNCTION(BlueprintPure, Category="Drone|Mission Result")
	bool IsTutorialAllComplete() const { return bTutorialAllComplete; }

	/** 전체 완료 화면 [미션 진행]: 로비 미션 탭 첫 미션으로. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool RequestContinueToMissions();

	/** 전체 완료 화면 [시작 메뉴]: 타이틀로. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool RequestReturnToTitle();

	// 튜토리얼 문구(Figma S48·S49 원문). 바꾸려면 Class Defaults에서 수정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Mission Result|Tutorial Text")
	FText TrainingCompleteTitle = FText::FromString(TEXT("훈련 완료"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Mission Result|Tutorial Text", meta=(MultiLine="true"))
	FText TutorialAllCompleteMessage = FText::FromString(TEXT("이제 운용 할 준비가 되었습니다."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Mission Result|Tutorial Text")
	FText NextLessonButtonLabel = FText::FromString(TEXT("다음"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Mission Result|Tutorial Text")
	FText TutorialRetryButtonLabel = FText::FromString(TEXT("다시하기"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Mission Result|Tutorial Text")
	FText ContinueToMissionsButtonLabel = FText::FromString(TEXT("미션 진행"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Mission Result|Tutorial Text")
	FText TitleMenuButtonLabel = FText::FromString(TEXT("시작 메뉴"));

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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	FDroneGamepadFocus GamepadFocus;

private:
	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleReturnToLobbyClicked();

	UFUNCTION()
	void HandleNextMissionClicked();

	UDroneGameFlowSubsystem* GetFlowSubsystem() const;
	void RefreshProgressionDisplay();
	void SetButtonLabel(UTextBlock* Label, const FText& Text) const;
	TArray<UWidget*> GetOrderedButtons() const;

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

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionResultDetailText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> NextMissionButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NextMissionButtonText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RetryMissionButtonText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReturnToLobbyButtonText;

	/** 튜토리얼 문구로 바꾸기 전의 기본 버튼 글(미션용). */
	UPROPERTY(Transient)
	FText DefaultRetryButtonLabel;

	UPROPERTY(Transient)
	FText DefaultLobbyButtonLabel;

	UPROPERTY(Transient)
	bool bTutorialAllComplete = false;

	UPROPERTY(Transient)
	FText ResultDetailText;

	UPROPERTY(Transient)
	FText NextMissionButtonLabel;

	UPROPERTY(Transient)
	bool bNextMissionAvailable = false;

	UPROPERTY(Transient)
	bool bSequenceComplete = false;

	UPROPERTY(Transient)
	bool bUsingNativeFallbackLayout = false;
};
