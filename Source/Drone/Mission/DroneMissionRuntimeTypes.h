#pragma once

#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "DroneMissionRuntimeTypes.generated.h"

/** Map 안의 Mission Director가 소유하는 실행 상태다. Front-end Flow 상태와 중복되지 않는다. */
UENUM(BlueprintType)
enum class EDroneMissionRuntimeState : uint8
{
	Inactive UMETA(DisplayName="Inactive"),
	Active UMETA(DisplayName="Active"),
	Finished UMETA(DisplayName="Finished")
};

/** Event 기반 목표 UI가 한 번에 읽는 개별 목표 사본이다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneMissionObjectiveSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	FName ObjectiveId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	int32 CurrentProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	int32 RequiredProgress = 1;

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	bool bCompleted = false;
};

/** Tick이나 Actor 검색 없이 Mission 목표 패널에 전달하는 전체 실행 사본이다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneMissionRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	EDroneMissionRuntimeState State = EDroneMissionRuntimeState::Inactive;

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	FName MissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	TArray<FDroneMissionObjectiveSnapshot> Objectives;

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	int32 CurrentObjectiveIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Drone Mission")
	EDroneMissionOutcome Outcome = EDroneMissionOutcome::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneMissionRuntimeSnapshotChangedSignature,
	const FDroneMissionRuntimeSnapshot&, Snapshot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneMissionRuntimeFinishedSignature,
	EDroneMissionOutcome, Outcome);
