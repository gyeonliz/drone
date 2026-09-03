#include "UI/DroneFlightHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Drone.h"
#include "Health/DroneHealthComponent.h"
#include "Styling/CoreStyle.h"
#include "Telemetry/DroneTelemetryComponent.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"

namespace DroneFlightHUD
{
// RootCanvasName은 native fallback 전용 이름이다.
const FName RootCanvasName(TEXT("FlightHUDRoot"));
// 아래 네 TextName만 WBP_DroneFlightHUD Designer와 공유하는 필수 계약이다.
const FName SpeedTextName(TEXT("SpeedValueText"));
const FName AltitudeTextName(TEXT("AltitudeValueText"));
const FName VerticalSpeedTextName(TEXT("VerticalSpeedValueText"));
const FName HeadingTextName(TEXT("HeadingValueText"));
}

void UDroneFlightHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// WBP가 계약을 만족하면 Designer 위젯을 쓰고, 아니면 C++ 기본 UI를 만든다.
	BuildDefaultLayout();
	BuildHealthLayout();
	BuildTrainingLayout();
	if (UDroneTelemetryComponent* CurrentSource = TelemetrySource.Get())
	{
		// 첫 주기 Event를 기다리지 않고 현재 값을 바로 보여 준다.
		ApplySnapshot(CurrentSource->GetLatestSnapshot());
	}
	else
	{
		ApplyPlaceholderText();
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDroneFlightHUDWidget::NativeDestruct()
{
	// Weak Pointer만 비우는 것으로는 Dynamic Delegate가 자동 해제된다고 가정하지 않는다.
	ClearTrainingRecordSource();
	ClearHealthSource();
	ClearTelemetrySource();
	Super::NativeDestruct();
}

void UDroneFlightHUDWidget::SetTelemetrySource(UDroneTelemetryComponent* InTelemetrySource)
{
	UDroneTelemetryComponent* CurrentSource = TelemetrySource.Get();
	if (CurrentSource != InTelemetrySource)
	{
		// Pawn 전환 시 이전 Drone이 계속 HUD를 갱신하지 못하도록 먼저 구독을 끊는다.
		if (CurrentSource)
		{
			CurrentSource->OnTelemetryUpdated.RemoveDynamic(
				this,
				&UDroneFlightHUDWidget::HandleTelemetryUpdated);
		}

		TelemetrySource = InTelemetrySource;
	}

	if (InTelemetrySource)
	{
		// 같은 Source를 다시 전달받아도 중복 구독이 생기지 않는다.
		InTelemetrySource->OnTelemetryUpdated.AddUniqueDynamic(
			this,
			&UDroneFlightHUDWidget::HandleTelemetryUpdated);
		ApplySnapshot(InTelemetrySource->GetLatestSnapshot());
		// HUD는 표시만 하므로 Mouse/조종 입력의 Hit Test를 가로채지 않는다.
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		TelemetrySource.Reset();
		ApplyPlaceholderText();
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDroneFlightHUDWidget::ClearTelemetrySource()
{
	SetTelemetrySource(nullptr);
}

void UDroneFlightHUDWidget::HandleTelemetryUpdated(const FDroneTelemetrySnapshot Snapshot)
{
	ApplySnapshot(Snapshot);
}

void UDroneFlightHUDWidget::SetHealthSource(UDroneHealthComponent* InHealthSource)
{
	UDroneHealthComponent* CurrentSource = HealthSource.Get();
	if (CurrentSource != InHealthSource && CurrentSource)
	{
		CurrentSource->OnHealthChanged.RemoveDynamic(this, &UDroneFlightHUDWidget::HandleHealthChanged);
	}

	HealthSource = InHealthSource;
	if (InHealthSource)
	{
		InHealthSource->OnHealthChanged.AddUniqueDynamic(this, &UDroneFlightHUDWidget::HandleHealthChanged);
	}
	RefreshHealthDisplay();
}

void UDroneFlightHUDWidget::ClearHealthSource()
{
	SetHealthSource(nullptr);
}

void UDroneFlightHUDWidget::HandleHealthChanged(
	float PreviousHealth,
	float CurrentHealth,
	float MaxHealth,
	float AppliedDamage)
{
	RefreshHealthDisplay();
}

void UDroneFlightHUDWidget::SetTrainingRecordSource(
	UDroneTrainingLapRecorderComponent* InTrainingRecordSource)
{
	UDroneTrainingLapRecorderComponent* CurrentSource = TrainingRecordSource.Get();
	if (CurrentSource != InTrainingRecordSource && CurrentSource)
	{
		CurrentSource->OnLapStarted.RemoveDynamic(this, &UDroneFlightHUDWidget::HandleTrainingLapStarted);
		CurrentSource->OnSegmentRecorded.RemoveDynamic(this, &UDroneFlightHUDWidget::HandleTrainingSegmentRecorded);
		CurrentSource->OnLapCompleted.RemoveDynamic(this, &UDroneFlightHUDWidget::HandleTrainingLapCompleted);
		CurrentSource->OnLapComparisonReady.RemoveDynamic(
			this,
			&UDroneFlightHUDWidget::HandleTrainingLapComparisonReady);
	}

	TrainingRecordSource = InTrainingRecordSource;
	DisplayedTrainingSegments.Reset();
	DisplayedLapComparison = FDroneTrainingLapComparison();
	if (!InTrainingRecordSource)
	{
		RefreshTrainingDisplay();
		if (TrainingReadoutPanel)
		{
			TrainingReadoutPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	InTrainingRecordSource->OnLapStarted.AddUniqueDynamic(this, &UDroneFlightHUDWidget::HandleTrainingLapStarted);
	InTrainingRecordSource->OnSegmentRecorded.AddUniqueDynamic(this, &UDroneFlightHUDWidget::HandleTrainingSegmentRecorded);
	InTrainingRecordSource->OnLapCompleted.AddUniqueDynamic(this, &UDroneFlightHUDWidget::HandleTrainingLapCompleted);
	InTrainingRecordSource->OnLapComparisonReady.AddUniqueDynamic(
		this,
		&UDroneFlightHUDWidget::HandleTrainingLapComparisonReady);

	DisplayedTrainingSegments = InTrainingRecordSource->IsLapRecording()
		? InTrainingRecordSource->GetCurrentSegments()
		: InTrainingRecordSource->GetLastCompletedLap().Segments;
	DisplayedLapComparison = InTrainingRecordSource->GetLastCompletedComparison();
	RefreshTrainingDisplay();
	if (TrainingReadoutPanel)
	{
		TrainingReadoutPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UDroneFlightHUDWidget::ClearTrainingRecordSource()
{
	SetTrainingRecordSource(nullptr);
}

void UDroneFlightHUDWidget::HandleTrainingLapStarted()
{
	DisplayedTrainingSegments.Reset();
	DisplayedLapComparison = FDroneTrainingLapComparison();
	RefreshTrainingDisplay();
}

void UDroneFlightHUDWidget::HandleTrainingSegmentRecorded(const FDroneTrainingSegmentRecord SegmentRecord)
{
	DisplayedTrainingSegments.Add(SegmentRecord);
	RefreshTrainingDisplay();
}

void UDroneFlightHUDWidget::HandleTrainingLapCompleted(const FDroneTrainingLapRecord LapRecord)
{
	DisplayedTrainingSegments = LapRecord.Segments;
	RefreshTrainingDisplay();
}

void UDroneFlightHUDWidget::HandleTrainingLapComparisonReady(
	const FDroneTrainingLapComparison Comparison)
{
	DisplayedLapComparison = Comparison;
	RefreshTrainingDisplay();
}

void UDroneFlightHUDWidget::BuildDefaultLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	if (TryBindBlueprintLayout())
	{
		return;
	}

	// native Class는 Designer Tree가 없으므로 C++ 기본 UI를 만든다.
	// 컴파일된 WBP는 BindWidget 4개를 반드시 가져야 하며, 런타임에 예외적으로 계약이
	// 불완전한 경우만 방어적으로 이 fallback Root를 쓰고 Asset 자체는 수정하지 않는다.
	if (WidgetTree->RootWidget)
	{
		UE_LOG(
			LogDrone,
			Verbose,
			TEXT("Flight HUD '%s' is using the native fallback because its WBP TextBlock contract is incomplete."),
			*GetNameSafe(GetClass()));
	}

	bUsingNativeFallbackLayout = true;
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		DroneFlightHUD::RootCanvasName);
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	UBorder* ReadoutPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("FlightReadoutPanel"));
	ReadoutPanel->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.035f, 0.78f));
	ReadoutPanel->SetPadding(FMargin(14.0f, 10.0f));
	ReadoutPanel->SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanelSlot* ReadoutSlot = RootCanvas->AddChildToCanvas(ReadoutPanel);
	ReadoutSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	ReadoutSlot->SetAlignment(FVector2D::ZeroVector);
	ReadoutSlot->SetPosition(FVector2D(24.0f, 24.0f));
	ReadoutSlot->SetAutoSize(true);

	UVerticalBox* ReadoutColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("FlightReadoutColumn"));
	ReadoutPanel->SetContent(ReadoutColumn);

	UTextBlock* HeaderText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("FlightReadoutHeader"));
	HeaderText->SetText(FText::FromString(TEXT("드론 비행 정보")));
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.25f, 0.85f, 1.0f, 1.0f)));
	HeaderText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16.0f));
	UVerticalBoxSlot* HeaderSlot = ReadoutColumn->AddChildToVerticalBox(HeaderText);
	HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	// 반복되는 TextBlock 생성 규칙은 Lambda 한 곳에 모아 네 값의 스타일을 일치시킨다.
	auto AddValueText = [this, ReadoutColumn](const FName WidgetName) -> UTextBlock*
	{
		UTextBlock* ValueText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			WidgetName);
		ValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ValueText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f));
		ValueText->SetJustification(ETextJustify::Left);
		UVerticalBoxSlot* ValueSlot = ReadoutColumn->AddChildToVerticalBox(ValueText);
		ValueSlot->SetPadding(FMargin(0.0f, 1.0f));
		return ValueText;
	};

	SpeedValueText = AddValueText(DroneFlightHUD::SpeedTextName);
	AltitudeValueText = AddValueText(DroneFlightHUD::AltitudeTextName);
	VerticalSpeedValueText = AddValueText(DroneFlightHUD::VerticalSpeedTextName);
	HeadingValueText = AddValueText(DroneFlightHUD::HeadingTextName);
	PushCachedTextToWidgets();
}

bool UDroneFlightHUDWidget::TryBindBlueprintLayout()
{
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return false;
	}

	// BindWidget은 자동 생성이 아니므로 이름과 타입 계약을 실행 중에도 다시 확인한다.
	SpeedValueText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFlightHUD::SpeedTextName));
	AltitudeValueText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFlightHUD::AltitudeTextName));
	VerticalSpeedValueText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFlightHUD::VerticalSpeedTextName));
	HeadingValueText = Cast<UTextBlock>(WidgetTree->FindWidget(DroneFlightHUD::HeadingTextName));

	const bool bHasCompleteBlueprintLayout =
		SpeedValueText && AltitudeValueText && VerticalSpeedValueText && HeadingValueText;
	if (!bHasCompleteBlueprintLayout)
	{
		// 부분 연결 상태로 남기지 않아 이후 Push에서 일부 값만 갱신되는 상황을 막는다.
		SpeedValueText = nullptr;
		AltitudeValueText = nullptr;
		VerticalSpeedValueText = nullptr;
		HeadingValueText = nullptr;
		return false;
	}

	bUsingNativeFallbackLayout = false;
	if (UTextBlock* HeaderText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("FlightReadoutHeader"))))
	{
		HeaderText->SetText(FText::FromString(TEXT("드론 비행 정보")));
		HeaderText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16.0f));
	}

	// Designer WBP가 Roboto를 저장했더라도 한글 시스템 폴백을 사용할 수 있는
	// Engine 기본 Composite Font로 런타임에 다시 고정한다.
	const FSlateFontInfo ReadoutFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f);
	SpeedValueText->SetFont(ReadoutFont);
	AltitudeValueText->SetFont(ReadoutFont);
	VerticalSpeedValueText->SetFont(ReadoutFont);
	HeadingValueText->SetFont(ReadoutFont);
	PushCachedTextToWidgets();
	return true;
}

void UDroneFlightHUDWidget::BuildHealthLayout()
{
	if (!WidgetTree || HealthReadoutPanel)
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		UE_LOG(LogDrone, Warning, TEXT("Flight HUD '%s' needs a CanvasPanel root for the Health panel."), *GetNameSafe(GetClass()));
		return;
	}

	HealthReadoutPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HealthReadoutPanel"));
	HealthReadoutPanel->SetBrushColor(FLinearColor(0.06f, 0.015f, 0.02f, 0.84f));
	HealthReadoutPanel->SetPadding(FMargin(14.0f, 9.0f));
	HealthReadoutPanel->SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanelSlot* HealthSlot = RootCanvas->AddChildToCanvas(HealthReadoutPanel);
	HealthSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	HealthSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	HealthSlot->SetPosition(FVector2D(-24.0f, 24.0f));
	HealthSlot->SetAutoSize(true);

	HealthValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthValueText"));
	HealthValueText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.35f, 0.3f, 1.0f)));
	HealthValueText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18.0f));
	HealthReadoutPanel->SetContent(HealthValueText);
	RefreshHealthDisplay();
}

void UDroneFlightHUDWidget::RefreshHealthDisplay()
{
	const UDroneHealthComponent* Source = HealthSource.Get();
	if (Source)
	{
		HealthDisplayText = FText::FromString(Source->IsDead()
			? FString::Printf(TEXT("기체 상태  파괴됨  0 / %.0f"), Source->GetMaxHealth())
			: FString::Printf(
				TEXT("기체 내구도  %.0f / %.0f"),
				Source->GetCurrentHealth(),
				Source->GetMaxHealth()));
	}
	else
	{
		HealthDisplayText = FText::FromString(TEXT("기체 내구도  --- / ---"));
	}
	if (HealthValueText)
	{
		HealthValueText->SetText(HealthDisplayText);
	}
	if (HealthReadoutPanel)
	{
		HealthReadoutPanel->SetVisibility(Source
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UDroneFlightHUDWidget::BuildTrainingLayout()
{
	if (!WidgetTree || TrainingReadoutPanel)
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		UE_LOG(LogDrone, Warning, TEXT("Flight HUD '%s' needs a CanvasPanel root for the Training statistics panel."), *GetNameSafe(GetClass()));
		return;
	}

	TrainingReadoutPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrainingReadoutPanel"));
	TrainingReadoutPanel->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.035f, 0.84f));
	TrainingReadoutPanel->SetPadding(FMargin(14.0f, 10.0f));
	TrainingReadoutPanel->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* TrainingSlot = RootCanvas->AddChildToCanvas(TrainingReadoutPanel);
	TrainingSlot->SetAnchors(FAnchors(0.0f, 1.0f));
	TrainingSlot->SetAlignment(FVector2D(0.0f, 1.0f));
	TrainingSlot->SetPosition(FVector2D(24.0f, -24.0f));
	TrainingSlot->SetAutoSize(true);

	UVerticalBox* TrainingColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("TrainingReadoutColumn"));
	TrainingReadoutPanel->SetContent(TrainingColumn);

	UTextBlock* HeaderText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("TrainingReadoutHeader"));
	HeaderText->SetText(FText::FromString(TEXT("코스 구간 기록")));
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.25f, 1.0f, 0.55f, 1.0f)));
	HeaderText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16.0f));
	UVerticalBoxSlot* HeaderSlot = TrainingColumn->AddChildToVerticalBox(HeaderText);
	HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	auto AddTrainingText = [this, TrainingColumn](const FName WidgetName) -> UTextBlock*
	{
		UTextBlock* ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		ValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ValueText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16.0f));
		ValueText->SetJustification(ETextJustify::Left);
		UVerticalBoxSlot* ValueSlot = TrainingColumn->AddChildToVerticalBox(ValueText);
		ValueSlot->SetPadding(FMargin(0.0f, 1.0f));
		return ValueText;
	};

	TrainingStatusValueText = AddTrainingText(TEXT("TrainingStatusValueText"));
	LastSegmentSpeedValueText = AddTrainingText(TEXT("LastSegmentSpeedValueText"));
	LastSegmentDistanceValueText = AddTrainingText(TEXT("LastSegmentDistanceValueText"));
	LastSegmentTimeValueText = AddTrainingText(TEXT("LastSegmentTimeValueText"));
	AverageSegmentSpeedValueText = AddTrainingText(TEXT("AverageSegmentSpeedValueText"));
	AverageSegmentDistanceValueText = AddTrainingText(TEXT("AverageSegmentDistanceValueText"));
	AverageSegmentTimeValueText = AddTrainingText(TEXT("AverageSegmentTimeValueText"));
	PreviousLapAverageTimeValueText = AddTrainingText(TEXT("PreviousLapAverageTimeValueText"));
	BestLapTimeValueText = AddTrainingText(TEXT("BestLapTimeValueText"));
	LapTimeDeltaValueText = AddTrainingText(TEXT("LapTimeDeltaValueText"));
	LapSpeedDeltaValueText = AddTrainingText(TEXT("LapSpeedDeltaValueText"));
	RefreshTrainingDisplay();
}

void UDroneFlightHUDWidget::ApplySnapshot(const FDroneTelemetrySnapshot& Snapshot)
{
	// 단위 변환은 Telemetry에서 끝났으므로 여기서는 자릿수와 부호만 포맷한다.
	DisplayedSnapshot = Snapshot;
	SpeedDisplayText = FText::FromString(FString::Printf(
		TEXT("현재 속도  %.1f km/h"),
		Snapshot.SpeedKilometersPerHour));
	AltitudeDisplayText = FText::FromString(FString::Printf(
		TEXT("현재 고도  %.1f m"),
		Snapshot.AltitudeMeters));
	VerticalSpeedDisplayText = FText::FromString(FString::Printf(
		TEXT("수직 속도  %+.1f m/s"),
		Snapshot.VerticalSpeedMetersPerSecond));

	// 359.6도처럼 반올림 결과가 360이 되면 000도 표시로 되돌린다.
	const int32 RoundedHeading = FMath::RoundToInt(FRotator::ClampAxis(Snapshot.HeadingDegrees)) % 360;
	HeadingDisplayText = FText::FromString(FString::Printf(
		TEXT("진행 방향  %03d\u00B0"),
		RoundedHeading));
	PushCachedTextToWidgets();
}

void UDroneFlightHUDWidget::ApplyPlaceholderText()
{
	DisplayedSnapshot = FDroneTelemetrySnapshot();
	SpeedDisplayText = FText::FromString(TEXT("현재 속도  --.- km/h"));
	AltitudeDisplayText = FText::FromString(TEXT("현재 고도  --.- m"));
	VerticalSpeedDisplayText = FText::FromString(TEXT("수직 속도  --.- m/s"));
	HeadingDisplayText = FText::FromString(TEXT("진행 방향  ---\u00B0"));
	PushCachedTextToWidgets();
}

void UDroneFlightHUDWidget::PushCachedTextToWidgets()
{
	// Property Binding을 사용하지 않고 주기적·즉시 갱신 Telemetry Event가 왔을 때만 Text를 바꾼다.
	if (SpeedValueText)
	{
		SpeedValueText->SetText(SpeedDisplayText);
	}
	if (AltitudeValueText)
	{
		AltitudeValueText->SetText(AltitudeDisplayText);
	}
	if (VerticalSpeedValueText)
	{
		VerticalSpeedValueText->SetText(VerticalSpeedDisplayText);
	}
	if (HeadingValueText)
	{
		HeadingValueText->SetText(HeadingDisplayText);
	}
}

void UDroneFlightHUDWidget::RefreshTrainingDisplay()
{
	const UDroneTrainingLapRecorderComponent* Source = TrainingRecordSource.Get();
	RefreshLapComparisonDisplay();
	if (!Source)
	{
		TrainingStatusDisplayText = FText::FromString(TEXT("코스 기록  없음"));
		LastSegmentSpeedDisplayText = FText::FromString(TEXT("방금 구간 평균 속도  --.- km/h"));
		LastSegmentDistanceDisplayText = FText::FromString(TEXT("방금 구간 이동 거리  --.- m"));
		LastSegmentTimeDisplayText = FText::FromString(TEXT("방금 구간 통과 시간  --.--초"));
		AverageSegmentSpeedDisplayText = FText::FromString(TEXT("완료 구간 평균 속도  --.- km/h"));
		AverageSegmentDistanceDisplayText = FText::FromString(TEXT("완료 구간 평균 거리  --.- m"));
		AverageSegmentTimeDisplayText = FText::FromString(TEXT("완료 구간 평균 시간  --.--초"));
		PushCachedTrainingTextToWidgets();
		return;
	}

	if (Source->IsLapRecording())
	{
		TrainingStatusDisplayText = FText::FromString(FString::Printf(
			TEXT("코스 기록  측정 중 · 완료 %d구간"),
			DisplayedTrainingSegments.Num()));
	}
	else if (Source->HasCompletedLap())
	{
		TrainingStatusDisplayText = FText::FromString(FString::Printf(
			TEXT("코스 기록  최근 완주 · %d구간"),
			DisplayedTrainingSegments.Num()));
	}
	else
	{
		TrainingStatusDisplayText = FText::FromString(TEXT("코스 기록  시작 게이트 대기"));
	}

	if (DisplayedTrainingSegments.IsEmpty())
	{
		LastSegmentSpeedDisplayText = FText::FromString(TEXT("방금 구간 평균 속도  --.- km/h"));
		LastSegmentDistanceDisplayText = FText::FromString(TEXT("방금 구간 이동 거리  --.- m"));
		LastSegmentTimeDisplayText = FText::FromString(TEXT("방금 구간 통과 시간  --.--초"));
		AverageSegmentSpeedDisplayText = FText::FromString(TEXT("완료 구간 평균 속도  --.- km/h"));
		AverageSegmentDistanceDisplayText = FText::FromString(TEXT("완료 구간 평균 거리  --.- m"));
		AverageSegmentTimeDisplayText = FText::FromString(TEXT("완료 구간 평균 시간  --.--초"));
		PushCachedTrainingTextToWidgets();
		return;
	}

	const FDroneTrainingSegmentRecord& LastSegment = DisplayedTrainingSegments.Last();
	LastSegmentSpeedDisplayText = FText::FromString(FString::Printf(
		TEXT("방금 구간 평균 속도  %.1f km/h"),
		LastSegment.AverageSpeedKilometersPerHour));
	LastSegmentDistanceDisplayText = FText::FromString(FString::Printf(
		TEXT("방금 구간 이동 거리  %.1f m"),
		LastSegment.TravelDistanceMeters));
	LastSegmentTimeDisplayText = FText::FromString(FString::Printf(
		TEXT("방금 구간 통과 시간  %.2f초"),
		LastSegment.ElapsedSeconds));

	double SpeedSum = 0.0;
	double DistanceSum = 0.0;
	double TimeSum = 0.0;
	for (const FDroneTrainingSegmentRecord& Segment : DisplayedTrainingSegments)
	{
		SpeedSum += FMath::IsFinite(Segment.AverageSpeedKilometersPerHour)
			? Segment.AverageSpeedKilometersPerHour
			: 0.0;
		DistanceSum += FMath::IsFinite(Segment.TravelDistanceMeters)
			? Segment.TravelDistanceMeters
			: 0.0;
		TimeSum += FMath::IsFinite(Segment.ElapsedSeconds)
			? Segment.ElapsedSeconds
			: 0.0;
	}

	const double SegmentCount = static_cast<double>(DisplayedTrainingSegments.Num());
	AverageSegmentSpeedDisplayText = FText::FromString(FString::Printf(
		TEXT("완료 구간 평균 속도  %.1f km/h"),
		SpeedSum / SegmentCount));
	AverageSegmentDistanceDisplayText = FText::FromString(FString::Printf(
		TEXT("완료 구간 평균 거리  %.1f m"),
		DistanceSum / SegmentCount));
	AverageSegmentTimeDisplayText = FText::FromString(FString::Printf(
		TEXT("완료 구간 평균 시간  %.2f초"),
		TimeSum / SegmentCount));
	PushCachedTrainingTextToWidgets();
}

void UDroneFlightHUDWidget::RefreshLapComparisonDisplay()
{
	const UDroneTrainingLapRecorderComponent* Source = TrainingRecordSource.Get();
	const FDroneTrainingLapComparison& Comparison = DisplayedLapComparison;
	if (!Source || !Comparison.CurrentLap.bCompleted)
	{
		PreviousLapAverageTimeDisplayText = FText::FromString(TEXT("이전 완주 평균  --.--초"));
		BestLapTimeDisplayText = FText::FromString(TEXT("최고 완주 기록  --.--초"));
		LapTimeDeltaDisplayText = FText::FromString(Source && Source->IsLapRecording()
			? TEXT("비교 결과  새 기록 측정 중")
			: TEXT("비교 결과  완주 후 생성"));
		LapSpeedDeltaDisplayText = FText::FromString(TEXT("속도 평균 대비  --.- km/h"));
		return;
	}

	BestLapTimeDisplayText = FText::FromString(Comparison.bIsNewBestTime
		? FString::Printf(
			TEXT("최고 완주 기록  %.2f초 · 신기록"),
			Comparison.BestElapsedSeconds)
		: FString::Printf(
			TEXT("최고 완주 기록  %.2f초"),
			Comparison.BestElapsedSeconds));

	if (!Comparison.bHasPreviousBaseline)
	{
		PreviousLapAverageTimeDisplayText = FText::FromString(TEXT("이전 완주 평균  없음"));
		LapTimeDeltaDisplayText = FText::FromString(TEXT("비교 결과  기준 기록 생성"));
		LapSpeedDeltaDisplayText = FText::FromString(TEXT("속도 평균 대비  기준 기록"));
		return;
	}

	PreviousLapAverageTimeDisplayText = FText::FromString(FString::Printf(
		TEXT("이전 완주 평균  %.2f초"),
		Comparison.PreviousAverageElapsedSeconds));

	const double TimeDelta = Comparison.ElapsedDeltaFromPreviousAverageSeconds;
	if (FMath::IsNearlyZero(TimeDelta, 0.005))
	{
		LapTimeDeltaDisplayText = FText::FromString(TEXT("평균 대비  ±0.00초 동일"));
	}
	else
	{
		LapTimeDeltaDisplayText = FText::FromString(TimeDelta < 0.0
			? FString::Printf(TEXT("평균 대비  %+.2f초 빠름"), TimeDelta)
			: FString::Printf(TEXT("평균 대비  %+.2f초 느림"), TimeDelta));
	}

	LapSpeedDeltaDisplayText = FText::FromString(FString::Printf(
		TEXT("속도 평균 대비  %+.1f km/h"),
		Comparison.SpeedDeltaFromPreviousAverageKilometersPerHour));
}

void UDroneFlightHUDWidget::PushCachedTrainingTextToWidgets()
{
	if (TrainingStatusValueText)
	{
		TrainingStatusValueText->SetText(TrainingStatusDisplayText);
	}
	if (LastSegmentSpeedValueText)
	{
		LastSegmentSpeedValueText->SetText(LastSegmentSpeedDisplayText);
	}
	if (LastSegmentDistanceValueText)
	{
		LastSegmentDistanceValueText->SetText(LastSegmentDistanceDisplayText);
	}
	if (LastSegmentTimeValueText)
	{
		LastSegmentTimeValueText->SetText(LastSegmentTimeDisplayText);
	}
	if (AverageSegmentSpeedValueText)
	{
		AverageSegmentSpeedValueText->SetText(AverageSegmentSpeedDisplayText);
	}
	if (AverageSegmentDistanceValueText)
	{
		AverageSegmentDistanceValueText->SetText(AverageSegmentDistanceDisplayText);
	}
	if (AverageSegmentTimeValueText)
	{
		AverageSegmentTimeValueText->SetText(AverageSegmentTimeDisplayText);
	}
	if (PreviousLapAverageTimeValueText)
	{
		PreviousLapAverageTimeValueText->SetText(PreviousLapAverageTimeDisplayText);
	}
	if (BestLapTimeValueText)
	{
		BestLapTimeValueText->SetText(BestLapTimeDisplayText);
	}
	if (LapTimeDeltaValueText)
	{
		LapTimeDeltaValueText->SetText(LapTimeDeltaDisplayText);
	}
	if (LapSpeedDeltaValueText)
	{
		LapSpeedDeltaValueText->SetText(LapSpeedDeltaDisplayText);
	}
}
