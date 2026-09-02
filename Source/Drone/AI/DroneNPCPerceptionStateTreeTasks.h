#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "DroneNPCPerceptionStateTreeTasks.generated.h"

USTRUCT()
struct FDroneStateTreeDetectedTaskInstanceData
{
	GENERATED_BODY()
};

/** 드론을 보고 있는 동안 이동·Smart Object 점유를 중단한다. */
USTRUCT(meta=(DisplayName="Hold Drone Detected Response", Category="Drone|AI|Perception"))
struct DRONE_API FDroneStateTreeDetectedTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeDetectedTaskInstanceData;

	FDroneStateTreeDetectedTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FDroneStateTreeSearchTaskInstanceData
{
	GENERATED_BODY()

	/** 마지막 감지 위치에 도착한 뒤를 포함한 최소 탐색 체류 시간이다. */
	UPROPERTY(EditAnywhere, Category="Search", meta=(ClampMin="0.1", ForceUnits="s"))
	float SearchDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category="Search", meta=(ClampMin="10.0", ForceUnits="cm"))
	float AcceptanceRadius = 120.0f;

	UPROPERTY(Transient)
	float ElapsedTime = 0.0f;
};

/** 마지막으로 본 드론 위치로 이동하며 잠시 탐색한 뒤 Patrol로 복귀한다. */
USTRUCT(meta=(DisplayName="Search Last Known Drone Location", Category="Drone|AI|Perception"))
struct DRONE_API FDroneStateTreeSearchTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeSearchTaskInstanceData;

	FDroneStateTreeSearchTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
