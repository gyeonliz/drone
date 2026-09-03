#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "DroneNPCCoverStateTreeTasks.generated.h"

USTRUCT()
struct FDroneStateTreeClaimCoverTaskInstanceData
{
	GENERATED_BODY()
};

/** MG 사용 불가·점유 중일 때 감지 중인 Hostile이 빈 Cover Slot을 Claim한다. */
USTRUCT(meta=(DisplayName="Claim Cover Slot", Category="Drone|AI|Cover"))
struct DRONE_API FDroneStateTreeClaimCoverTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FDroneStateTreeClaimCoverTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FDroneStateTreeMoveToCoverTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Cover", meta=(ClampMin="10.0", ForceUnits="cm"))
	float AcceptanceRadius = 100.0f;

	UPROPERTY(Transient)
	FVector Destination = FVector::ZeroVector;
};

/** 예약된 Cover까지 이동하고 도착하면 Slot을 Occupied로 전환한다. */
USTRUCT(meta=(DisplayName="Move To Reserved Cover", Category="Drone|AI|Cover"))
struct DRONE_API FDroneStateTreeMoveToCoverTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FDroneStateTreeMoveToCoverTaskInstanceData;

	FDroneStateTreeMoveToCoverTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FDroneStateTreeUseCoverTaskInstanceData
{
	GENERATED_BODY()
};

/** Cover 1-Slot을 Occupied로 유지하면서 Rifle/Shotgun 개인 무기 대응을 계속한다. */
USTRUCT(meta=(DisplayName="Use Reserved Cover", Category="Drone|AI|Cover"))
struct DRONE_API FDroneStateTreeUseCoverTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FDroneStateTreeUseCoverTaskInstanceData;

	FDroneStateTreeUseCoverTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
