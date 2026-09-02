#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "DroneNPCMGTurretStateTreeTasks.generated.h"

USTRUCT()
struct FDroneStateTreeClaimMGTurretTaskInstanceData
{
	GENERATED_BODY()
};

/** MG 사용 권한이 있고 드론을 감지 중인 Hostile만 빈 MGTurret Slot을 한 번 Claim한다. */
USTRUCT(meta=(DisplayName="Claim MG Turret Slot", Category="Drone|AI|MG"))
struct DRONE_API FDroneStateTreeClaimMGTurretTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeClaimMGTurretTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FDroneStateTreeMoveToMGTurretTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="MG", meta=(ClampMin="10.0", ForceUnits="cm"))
	float AcceptanceRadius = 100.0f;

	UPROPERTY(Transient)
	FVector Destination = FVector::ZeroVector;
};

/** 감지를 유지하면서 예약된 MG Slot까지 NavMesh로 이동한다. */
USTRUCT(meta=(DisplayName="Move To Reserved MG Turret", Category="Drone|AI|MG"))
struct DRONE_API FDroneStateTreeMoveToMGTurretTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeMoveToMGTurretTaskInstanceData;

	FDroneStateTreeMoveToMGTurretTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FDroneStateTreeHoldMGTurretTaskInstanceData
{
	GENERATED_BODY()
};

/** AI-MG-02가 Occupy·Fire를 연결할 때까지 도착한 1-Slot Claim을 유지한다. */
USTRUCT(meta=(DisplayName="Hold MG Turret Reservation", Category="Drone|AI|MG"))
struct DRONE_API FDroneStateTreeHoldMGTurretTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeHoldMGTurretTaskInstanceData;

	FDroneStateTreeHoldMGTurretTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
