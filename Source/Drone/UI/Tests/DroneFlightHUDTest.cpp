#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Health/DroneHealthComponent.h"
#include "Telemetry/DroneTelemetryComponent.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"
#include "UI/DroneFlightHUDWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFlightHUDTelemetryBindingTest,
	"Drone.UI.FlightHUDTelemetryBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneFlightHUDTelemetryBindingTest::RunTest(const FString& Parameters)
{
	// 이 테스트는 화면 디자인이 아니라 Event 연결·표시 포맷·정리 계약을 검증한다.
	UDroneFlightHUDWidget* Widget = NewObject<UDroneFlightHUDWidget>();
	UDroneTelemetryComponent* FirstSource = NewObject<UDroneTelemetryComponent>();
	UDroneTelemetryComponent* SecondSource = NewObject<UDroneTelemetryComponent>();
	TestNotNull(TEXT("Native Flight HUD Widget can be created"), Widget);
	TestNotNull(TEXT("First telemetry source can be created"), FirstSource);
	TestNotNull(TEXT("Second telemetry source can be created"), SecondSource);
	if (!Widget || !FirstSource || !SecondSource)
	{
		return false;
	}

	// 같은 Source를 두 번 지정해도 Dynamic Delegate는 한 번만 연결되어야 한다.
	Widget->SetTelemetrySource(FirstSource);
	Widget->SetTelemetrySource(FirstSource);
	TestTrue(TEXT("Flight HUD retains the first source"), Widget->GetTelemetrySource() == FirstSource);
	TestTrue(
		TEXT("Flight HUD binds to its first telemetry source"),
		FirstSource->OnTelemetryUpdated.Contains(Widget, FName(TEXT("HandleTelemetryUpdated"))));
	TestEqual(TEXT("Flight HUD is visible while telemetry is available"), Widget->GetVisibility(), ESlateVisibility::HitTestInvisible);

	// 첫 Event가 네 표시 문자열에 올바른 단위·자릿수·부호로 반영되는지 확인한다.
	FDroneTelemetrySnapshot FirstSnapshot;
	FirstSnapshot.SpeedKilometersPerHour = 42.5f;
	FirstSnapshot.AltitudeMeters = 18.2f;
	FirstSnapshot.VerticalSpeedMetersPerSecond = 1.4f;
	FirstSnapshot.HeadingDegrees = 315.0f;
	FirstSource->OnTelemetryUpdated.Broadcast(FirstSnapshot);

	TestEqual(TEXT("Speed text uses the Korean HUD format"), Widget->GetSpeedDisplayText().ToString(), FString(TEXT("현재 속도  42.5 km/h")));
	TestEqual(TEXT("Altitude text uses the Korean HUD format"), Widget->GetAltitudeDisplayText().ToString(), FString(TEXT("현재 고도  18.2 m")));
	TestEqual(TEXT("Vertical speed preserves its sign"), Widget->GetVerticalSpeedDisplayText().ToString(), FString(TEXT("수직 속도  +1.4 m/s")));
	TestEqual(TEXT("Heading text uses a normalized three-digit bearing"), Widget->GetHeadingDisplayText().ToString(), FString(TEXT("진행 방향  315\u00B0")));

	// Pawn 교체를 흉내 내어 이전 Source 해제 후 새 Source 연결을 검증한다.
	Widget->SetTelemetrySource(SecondSource);
	TestFalse(
		TEXT("Changing source removes the previous telemetry binding"),
		FirstSource->OnTelemetryUpdated.Contains(Widget, FName(TEXT("HandleTelemetryUpdated"))));
	TestTrue(
		TEXT("Changing source installs the new telemetry binding"),
		SecondSource->OnTelemetryUpdated.Contains(Widget, FName(TEXT("HandleTelemetryUpdated"))));

	FDroneTelemetrySnapshot SecondSnapshot;
	SecondSnapshot.SpeedKilometersPerHour = 10.0f;
	SecondSnapshot.AltitudeMeters = -2.0f;
	SecondSnapshot.VerticalSpeedMetersPerSecond = -0.5f;
	SecondSnapshot.HeadingDegrees = 359.6f;
	SecondSource->OnTelemetryUpdated.Broadcast(SecondSnapshot);
	TestEqual(TEXT("Descending vertical speed keeps its negative sign"), Widget->GetVerticalSpeedDisplayText().ToString(), FString(TEXT("수직 속도  -0.5 m/s")));
	TestEqual(TEXT("Rounded 360-degree heading wraps to north"), Widget->GetHeadingDisplayText().ToString(), FString(TEXT("진행 방향  000\u00B0")));

	// 체력 UI도 Tick/Property Binding 없이 HealthChanged Event만으로 갱신한다.
	UDroneHealthComponent* HealthSource = NewObject<UDroneHealthComponent>();
	TestNotNull(TEXT("Health source can be created"), HealthSource);
	if (HealthSource)
	{
		Widget->SetHealthSource(HealthSource);
		TestEqual(TEXT("Health HUD starts at the 100-point greybox default"),
			Widget->GetHealthDisplayText().ToString(), FString(TEXT("기체 내구도  100 / 100")));
		HealthSource->ApplyHealthDamage(30.0f, nullptr, nullptr);
		TestEqual(TEXT("Health HUD reflects applied damage"),
			Widget->GetHealthDisplayText().ToString(), FString(TEXT("기체 내구도  70 / 100")));
		HealthSource->ApplyHealthDamage(70.0f, nullptr, nullptr);
		TestEqual(TEXT("Health HUD labels zero Health as destroyed"),
			Widget->GetHealthDisplayText().ToString(), FString(TEXT("기체 상태  파괴됨  0 / 100")));
		TestEqual(TEXT("Lethal damage emits death only once"), HealthSource->GetDeathEventCount(), 1);
		TestFalse(TEXT("Damage after death is ignored"),
			HealthSource->ApplyHealthDamage(10.0f, nullptr, nullptr));
		Widget->ClearHealthSource();
		TestFalse(
			TEXT("Clearing Health source removes its event binding"),
			HealthSource->OnHealthChanged.Contains(Widget, FName(TEXT("HandleHealthChanged"))));
	}

	// Tutorial 기록 Source는 Gate Event마다 방금 구간과 완료 구간 평균을 한국어로 갱신한다.
	UDroneTrainingLapRecorderComponent* TrainingSource = NewObject<UDroneTrainingLapRecorderComponent>();
	TestNotNull(TEXT("Training record source can be created"), TrainingSource);
	if (TrainingSource)
	{
		Widget->SetTrainingRecordSource(TrainingSource);
		Widget->SetTrainingRecordSource(TrainingSource);
		TestTrue(
			TEXT("Flight HUD binds the Training segment event once"),
			TrainingSource->OnSegmentRecorded.Contains(Widget, FName(TEXT("HandleTrainingSegmentRecorded"))));
		TestTrue(
			TEXT("Flight HUD binds the TUT-04 comparison event once"),
			TrainingSource->OnLapComparisonReady.Contains(
				Widget,
				FName(TEXT("HandleTrainingLapComparisonReady"))));

		FDroneTrainingSegmentRecord FirstSegment;
		FirstSegment.ElapsedSeconds = 2.0;
		FirstSegment.TravelDistanceMeters = 100.0;
		FirstSegment.AverageSpeedKilometersPerHour = 10.0;
		TrainingSource->OnSegmentRecorded.Broadcast(FirstSegment);

		FDroneTrainingSegmentRecord SecondSegment;
		SecondSegment.ElapsedSeconds = 4.0;
		SecondSegment.TravelDistanceMeters = 200.0;
		SecondSegment.AverageSpeedKilometersPerHour = 20.0;
		TrainingSource->OnSegmentRecorded.Broadcast(SecondSegment);

		TestEqual(TEXT("Last segment speed is readable in Korean"), Widget->GetLastSegmentSpeedDisplayText().ToString(), FString(TEXT("방금 구간 평균 속도  20.0 km/h")));
		TestEqual(TEXT("Last segment distance is readable in Korean"), Widget->GetLastSegmentDistanceDisplayText().ToString(), FString(TEXT("방금 구간 이동 거리  200.0 m")));
		TestEqual(TEXT("Last segment time is readable in Korean"), Widget->GetLastSegmentTimeDisplayText().ToString(), FString(TEXT("방금 구간 통과 시간  4.00초")));
		TestEqual(TEXT("Completed segment average speed is calculated"), Widget->GetAverageSegmentSpeedDisplayText().ToString(), FString(TEXT("완료 구간 평균 속도  15.0 km/h")));
		TestEqual(TEXT("Completed segment average distance is calculated"), Widget->GetAverageSegmentDistanceDisplayText().ToString(), FString(TEXT("완료 구간 평균 거리  150.0 m")));
		TestEqual(TEXT("Completed segment average time is calculated"), Widget->GetAverageSegmentTimeDisplayText().ToString(), FString(TEXT("완료 구간 평균 시간  3.00초")));

		// 비교 계산은 Recorder 책임이고 HUD는 전달받은 결과의 부호·의미만 표시한다.
		FDroneTrainingLapComparison Comparison;
		Comparison.CurrentLap.bCompleted = true;
		Comparison.CurrentLap.ElapsedSeconds = 9.0;
		Comparison.bHasPreviousBaseline = true;
		Comparison.PreviousLapCount = 2;
		Comparison.PreviousAverageElapsedSeconds = 11.0;
		Comparison.BestElapsedSeconds = 9.0;
		Comparison.ElapsedDeltaFromPreviousAverageSeconds = -2.0;
		Comparison.SpeedDeltaFromPreviousAverageKilometersPerHour = 5.0;
		Comparison.bIsNewBestTime = true;
		TrainingSource->OnLapComparisonReady.Broadcast(Comparison);

		TestEqual(TEXT("HUD shows the previous Lap average"), Widget->GetPreviousLapAverageTimeDisplayText().ToString(), FString(TEXT("이전 완주 평균  11.00초")));
		TestEqual(TEXT("HUD marks a new Best Lap"), Widget->GetBestLapTimeDisplayText().ToString(), FString(TEXT("최고 완주 기록  9.00초 · 신기록")));
		TestEqual(TEXT("Negative time Delta is labeled faster"), Widget->GetLapTimeDeltaDisplayText().ToString(), FString(TEXT("평균 대비  -2.00초 빠름")));
		TestEqual(TEXT("Positive speed Delta keeps its sign"), Widget->GetLapSpeedDeltaDisplayText().ToString(), FString(TEXT("속도 평균 대비  +5.0 km/h")));

		Widget->ClearTrainingRecordSource();
		TestFalse(
			TEXT("Clearing Training source removes the segment binding"),
			TrainingSource->OnSegmentRecorded.Contains(Widget, FName(TEXT("HandleTrainingSegmentRecorded"))));
		TestFalse(
			TEXT("Clearing Training source removes the comparison binding"),
			TrainingSource->OnLapComparisonReady.Contains(
				Widget,
				FName(TEXT("HandleTrainingLapComparisonReady"))));
	}

	// 이미 해제한 Source의 늦은 Event가 현재 HUD를 덮어쓰면 안 된다.
	FirstSnapshot.SpeedKilometersPerHour = 999.0f;
	FirstSource->OnTelemetryUpdated.Broadcast(FirstSnapshot);
	TestTrue(
		TEXT("An old telemetry source can no longer change the HUD"),
		FMath::IsNearlyEqual(Widget->GetDisplayedSnapshot().SpeedKilometersPerHour, 10.0f));

	// UnPossess/종료 경로처럼 Source를 비우면 구독 해제와 숨김이 함께 일어나야 한다.
	Widget->ClearTelemetrySource();
	TestFalse(TEXT("Flight HUD clears its telemetry source"), Widget->HasTelemetrySource());
	TestFalse(
		TEXT("Clearing the source removes the final telemetry binding"),
		SecondSource->OnTelemetryUpdated.Contains(Widget, FName(TEXT("HandleTelemetryUpdated"))));
	TestEqual(TEXT("Flight HUD is collapsed without telemetry"), Widget->GetVisibility(), ESlateVisibility::Collapsed);

	return !HasAnyErrors();
}

#endif
