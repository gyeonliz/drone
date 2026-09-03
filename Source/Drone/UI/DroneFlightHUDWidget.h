#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Telemetry/DroneTelemetryTypes.h"
#include "Tutorial/DroneTrainingRecordTypes.h"
#include "DroneFlightHUDWidget.generated.h"

class UBorder;
class UDroneHealthComponent;
class UDroneTelemetryComponent;
class UDroneTrainingLapRecorderComponent;
class UTextBlock;

/**
 * Drone Prototype의 비행 정보를 표시하는 공용 HUD 기반 클래스.
 *
 * 책임 분리:
 * - UDroneTelemetryComponent: 속도·고도·수직 속도·Heading 계산
 * - UDroneFlightHUDWidget: Snapshot을 표시용 Text로 바꾸고 화면에 반영
 * - Widget Blueprint: 위치·색·폰트 같은 최종 외형만 편집
 *
 * WBP를 만들 때 아래 BindWidget 멤버와 같은 이름의 TextBlock 4개를 배치하면
 * C++가 자동으로 찾아 갱신한다. 이름/타입이 틀리면 WBP 컴파일에서 바로 잡는다.
 * Designer Tree가 없는 native Class를 직접 실행할 때는 학습·테스트용 fallback을 생성한다.
 */
UCLASS(Blueprintable)
class UDroneFlightHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 기존 Event 연결을 해제하고 새 Telemetry Source의 최신 Snapshot을 즉시 표시한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|HUD")
	void SetTelemetrySource(UDroneTelemetryComponent* InTelemetrySource);

	/** 현재 Telemetry Event 연결을 해제하고 데이터가 없는 HUD를 숨긴다. */
	UFUNCTION(BlueprintCallable, Category="Drone|HUD")
	void ClearTelemetrySource();

	UFUNCTION(BlueprintPure, Category="Drone|HUD")
	UDroneTelemetryComponent* GetTelemetrySource() const { return TelemetrySource.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|HUD")
	bool HasTelemetrySource() const { return TelemetrySource.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Drone|HUD")
	FDroneTelemetrySnapshot GetDisplayedSnapshot() const { return DisplayedSnapshot; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD")
	FText GetSpeedDisplayText() const { return SpeedDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD")
	FText GetAltitudeDisplayText() const { return AltitudeDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD")
	FText GetVerticalSpeedDisplayText() const { return VerticalSpeedDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD")
	FText GetHeadingDisplayText() const { return HeadingDisplayText; }

	/** 현재 Pawn의 체력 Event를 연결해 Tick 없이 드론 내구도를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|HUD|Health")
	void SetHealthSource(UDroneHealthComponent* InHealthSource);

	UFUNCTION(BlueprintCallable, Category="Drone|HUD|Health")
	void ClearHealthSource();

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Health")
	UDroneHealthComponent* GetHealthSource() const { return HealthSource.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Health")
	FText GetHealthDisplayText() const { return HealthDisplayText; }

	/** Tutorial Course의 구간 기록 Event를 연결해 Tick 없이 결과 패널을 갱신한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|HUD|Training")
	void SetTrainingRecordSource(UDroneTrainingLapRecorderComponent* InTrainingRecordSource);

	UFUNCTION(BlueprintCallable, Category="Drone|HUD|Training")
	void ClearTrainingRecordSource();

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	UDroneTrainingLapRecorderComponent* GetTrainingRecordSource() const { return TrainingRecordSource.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetLastSegmentSpeedDisplayText() const { return LastSegmentSpeedDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetLastSegmentDistanceDisplayText() const { return LastSegmentDistanceDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetLastSegmentTimeDisplayText() const { return LastSegmentTimeDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetAverageSegmentSpeedDisplayText() const { return AverageSegmentSpeedDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetAverageSegmentDistanceDisplayText() const { return AverageSegmentDistanceDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetAverageSegmentTimeDisplayText() const { return AverageSegmentTimeDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetPreviousLapAverageTimeDisplayText() const { return PreviousLapAverageTimeDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetBestLapTimeDisplayText() const { return BestLapTimeDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetLapTimeDeltaDisplayText() const { return LapTimeDeltaDisplayText; }

	UFUNCTION(BlueprintPure, Category="Drone|HUD|Training")
	FText GetLapSpeedDeltaDisplayText() const { return LapSpeedDeltaDisplayText; }

	/** true면 WBP Designer 대신 C++ 기본 레이아웃을 사용하고 있다는 뜻이다. */
	UFUNCTION(BlueprintPure, Category="Drone|HUD")
	bool IsUsingNativeFallbackLayout() const { return bUsingNativeFallbackLayout; }

protected:
	/** Widget 인스턴스당 한 번 호출되며 WBP 또는 native fallback 레이아웃을 준비한다. */
	virtual void NativeOnInitialized() override;

	/** Widget 제거 시 Telemetry Delegate가 남지 않도록 명시적으로 정리한다. */
	virtual void NativeDestruct() override;

private:
	/** 주기적 또는 즉시 갱신 Telemetry Event가 도착했을 때 호출되는 C++ 수신 함수다. */
	UFUNCTION()
	void HandleTelemetryUpdated(FDroneTelemetrySnapshot Snapshot);

	UFUNCTION()
	void HandleHealthChanged(float PreviousHealth, float CurrentHealth, float MaxHealth, float AppliedDamage);

	UFUNCTION()
	void HandleTrainingLapStarted();

	UFUNCTION()
	void HandleTrainingSegmentRecorded(FDroneTrainingSegmentRecord SegmentRecord);

	UFUNCTION()
	void HandleTrainingLapCompleted(FDroneTrainingLapRecord LapRecord);

	UFUNCTION()
	void HandleTrainingLapComparisonReady(FDroneTrainingLapComparison Comparison);

	/** WBP TextBlock 계약을 먼저 확인하고, 없으면 실행 가능한 C++ 기본 UI를 만든다. */
	void BuildDefaultLayout();

	/** WBP Designer의 정확한 이름을 가진 TextBlock 4개를 찾는다. */
	bool TryBindBlueprintLayout();

	/** Designer HUD의 왼쪽 아래에 한국어 Tutorial 구간 기록 패널을 동적으로 붙인다. */
	void BuildTrainingLayout();

	/** Designer 구조를 깨지 않고 우측 상단에 체력 표시를 동적으로 추가한다. */
	void BuildHealthLayout();
	void RefreshHealthDisplay();

	/** Snapshot 값 자체를 다시 계산하지 않고 표시 문자열만 만든다. */
	void ApplySnapshot(const FDroneTelemetrySnapshot& Snapshot);

	/** Telemetry Source가 없을 때 사용할 자리표시자 Text를 준비한다. */
	void ApplyPlaceholderText();

	/** 캐시된 Text를 현재 WBP 또는 native TextBlock에 한 번씩 밀어 넣는다. */
	void PushCachedTextToWidgets();

	/** 마지막 구간과 현재 Lap의 완료 구간 평균을 사람이 읽는 한국어 Text로 만든다. */
	void RefreshTrainingDisplay();

	/** TUT-04의 첫 기준·이전 평균·Best·Delta를 별도 결과 행으로 포맷한다. */
	void RefreshLapComparisonDisplay();

	void PushCachedTrainingTextToWidgets();

	/** Pawn이 파괴돼도 강한 참조로 수명을 늘리지 않도록 Weak Pointer를 사용한다. */
	TWeakObjectPtr<UDroneTelemetryComponent> TelemetrySource;

	TWeakObjectPtr<UDroneHealthComponent> HealthSource;

	TWeakObjectPtr<UDroneTrainingLapRecorderComponent> TrainingRecordSource;

	/** 테스트와 Blueprint 디버깅에서 마지막으로 받은 원본 Snapshot을 확인한다. */
	UPROPERTY(Transient)
	FDroneTelemetrySnapshot DisplayedSnapshot;

	UPROPERTY(Transient)
	FText SpeedDisplayText;

	UPROPERTY(Transient)
	FText AltitudeDisplayText;

	UPROPERTY(Transient)
	FText VerticalSpeedDisplayText;

	UPROPERTY(Transient)
	FText HeadingDisplayText;

	UPROPERTY(Transient)
	FText HealthDisplayText;

	UPROPERTY(Transient)
	TArray<FDroneTrainingSegmentRecord> DisplayedTrainingSegments;

	UPROPERTY(Transient)
	FDroneTrainingLapComparison DisplayedLapComparison;

	UPROPERTY(Transient)
	FText TrainingStatusDisplayText;

	UPROPERTY(Transient)
	FText LastSegmentSpeedDisplayText;

	UPROPERTY(Transient)
	FText LastSegmentDistanceDisplayText;

	UPROPERTY(Transient)
	FText LastSegmentTimeDisplayText;

	UPROPERTY(Transient)
	FText AverageSegmentSpeedDisplayText;

	UPROPERTY(Transient)
	FText AverageSegmentDistanceDisplayText;

	UPROPERTY(Transient)
	FText AverageSegmentTimeDisplayText;

	UPROPERTY(Transient)
	FText PreviousLapAverageTimeDisplayText;

	UPROPERTY(Transient)
	FText BestLapTimeDisplayText;

	UPROPERTY(Transient)
	FText LapTimeDeltaDisplayText;

	UPROPERTY(Transient)
	FText LapSpeedDeltaDisplayText;

	/**
	 * 아래 네 이름은 C++ ↔ WBP Designer 계약이다.
	 * BindWidget은 위젯을 생성하지 않는다. WBP Designer에 같은 이름·타입이 있어야
	 * 컴파일에 성공하고, 생성된 Widget 인스턴스의 이 포인터에 연결된다.
	 */
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> SpeedValueText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> AltitudeValueText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> VerticalSpeedValueText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> HeadingValueText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HealthReadoutPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthValueText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TrainingReadoutPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrainingStatusValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LastSegmentSpeedValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LastSegmentDistanceValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LastSegmentTimeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AverageSegmentSpeedValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AverageSegmentDistanceValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AverageSegmentTimeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviousLapAverageTimeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BestLapTimeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LapTimeDeltaValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LapSpeedDeltaValueText;

	UPROPERTY(Transient)
	bool bUsingNativeFallbackLayout = false;
};
