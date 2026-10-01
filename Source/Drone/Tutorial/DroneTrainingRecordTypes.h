#pragma once

#include "CoreMinimal.h"
#include "DroneTrainingRecordTypes.generated.h"

/** Tutorial Course 기록기가 현재 한 Lap을 어떤 상태로 다루는지 나타낸다. */
UENUM(BlueprintType)
enum class EDroneTrainingLapRecordState : uint8
{
	Idle UMETA(DisplayName="Idle"),
	Recording UMETA(DisplayName="Recording"),
	Completed UMETA(DisplayName="Completed")
};

/** 직전 정상 Gate부터 현재 정상 Gate까지의 원본 기록이다. */
USTRUCT(BlueprintType)
struct FDroneTrainingSegmentRecord
{
	GENERATED_BODY()

	/** 0부터 시작하는 Segment 배열 위치다. Gate 0은 출발선이므로 첫 Segment는 0 -> 1이다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording")
	int32 SegmentIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording")
	int32 FromGateIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording")
	int32 ToGateIndex = INDEX_NONE;

	/** World Game Time 기준 구간 시간. Pause와 Time Dilation을 그대로 따른다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording", meta=(Units="s"))
	double ElapsedSeconds = 0.0;

	/** Drone의 3차원 World 위치 표본 사이 거리를 합산한 값이다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording", meta=(Units="m"))
	double TravelDistanceMeters = 0.0;

	/** TravelDistanceMeters / ElapsedSeconds를 km/h로 변환한 값이다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording", meta=(Units="km/h"))
	double AverageSpeedKilometersPerHour = 0.0;
};

/** Gate 0 승인부터 마지막 Gate 승인까지의 성공 Lap 원본 기록이다. */
USTRUCT(BlueprintType)
struct FDroneTrainingLapRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording", meta=(Units="s"))
	double ElapsedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording", meta=(Units="m"))
	double TravelDistanceMeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording", meta=(Units="km/h"))
	double AverageSpeedKilometersPerHour = 0.0;

	/** Gate가 N개라면 Gate 0을 출발선으로 사용하므로 Segment는 N-1개다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Recording")
	TArray<FDroneTrainingSegmentRecord> Segments;
};

/** 현재 구간을 같은 위치의 이전 성공 구간들과 비교한 TUT-04 결과다. */
USTRUCT(BlueprintType)
struct FDroneTrainingSegmentComparison
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	FDroneTrainingSegmentRecord CurrentSegment;

	/** 현재 시도를 제외하고 비교 계산에 사용한 이전 정상 구간 수다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	int32 PreviousRecordCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	bool bHasPreviousBaseline = false;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="s"))
	double PreviousAverageElapsedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="km/h"))
	double PreviousAverageSpeedKilometersPerHour = 0.0;

	/** 현재 구간을 포함한 최단 시간이다. 첫 정상 기록도 Best로 시작한다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="s"))
	double BestElapsedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="km/h"))
	double BestAverageSpeedKilometersPerHour = 0.0;

	/** Current - PreviousAverage. 음수면 이전 평균보다 빠르다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="s"))
	double ElapsedDeltaFromPreviousAverageSeconds = 0.0;

	/** Current - PreviousAverage. 양수면 이전 평균보다 평균 속도가 높다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="km/h"))
	double SpeedDeltaFromPreviousAverageKilometersPerHour = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	bool bIsNewBestTime = false;
};

/** 한 Lap 완료 직후 생성되는 이전 평균·Best·Delta의 불변 결과다. */
USTRUCT(BlueprintType)
struct FDroneTrainingLapComparison
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	FDroneTrainingLapRecord CurrentLap;

	/** 현재 Lap을 제외하고 비교 계산에 사용한 이전 정상 Lap 수다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	int32 PreviousLapCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	bool bHasPreviousBaseline = false;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="s"))
	double PreviousAverageElapsedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="km/h"))
	double PreviousAverageSpeedKilometersPerHour = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="s"))
	double BestElapsedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="km/h"))
	double BestAverageSpeedKilometersPerHour = 0.0;

	/** Current - PreviousAverage. 음수면 이전 평균보다 빠르다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="s"))
	double ElapsedDeltaFromPreviousAverageSeconds = 0.0;

	/** Current - PreviousAverage. 양수면 이전 평균보다 평균 속도가 높다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="km/h"))
	double SpeedDeltaFromPreviousAverageKilometersPerHour = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	bool bIsNewBestTime = false;

	/** TUT-BEST-01: 이번 랩 전에 디스크에 저장된 같은 코스·기체·조작 방식 최고 기록이 있었는지. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	bool bHasSavedBest = false;

	/** 이번 랩 전 저장 최고 기록(초). BestElapsedSeconds에는 이 값까지 합쳐 반영된다. */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison", meta=(Units="s"))
	double SavedBestElapsedSeconds = 0.0;

	/** 이번 랩이 저장 최고 기록을 새로 썼는지(처음 기록 포함). */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	bool bIsNewSavedBest = false;

	UPROPERTY(BlueprintReadOnly, Category="Tutorial|Comparison")
	TArray<FDroneTrainingSegmentComparison> SegmentComparisons;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDroneTrainingLapStartedSignature);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneTrainingSegmentRecordedSignature,
	FDroneTrainingSegmentRecord, SegmentRecord);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneTrainingLapCompletedSignature,
	FDroneTrainingLapRecord, LapRecord);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneTrainingLapComparisonReadySignature,
	FDroneTrainingLapComparison, Comparison);
