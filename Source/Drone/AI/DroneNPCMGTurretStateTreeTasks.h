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

	/** 주변 장애물 때문에 PathFollowing이 Idle로 끝나지 않아도 사수 위치로 최종 정렬할 수 있는 반경이다. */
	UPROPERTY(EditAnywhere, Category="MG", meta=(ClampMin="10.0", ForceUnits="cm"))
	float OperatorSnapRadius = 250.0f;

	/** PathFollowing은 Moving인데 Pawn이 움직이지 않을 때 다시 경로를 요청하기 전 대기 시간이다. */
	UPROPERTY(EditAnywhere, Category="MG", meta=(ClampMin="0.25", ForceUnits="s"))
	float StallTimeoutSeconds = 2.0f;

	/** 이 거리 이상 이동하면 정지 감시 시간을 초기화한다. */
	UPROPERTY(EditAnywhere, Category="MG", meta=(ClampMin="1.0", ForceUnits="cm"))
	float MinimumProgressDistance = 5.0f;

	/** 한 Claim 안에서 허용할 경로 재요청 횟수다. 이후에는 Claim을 반납해 다른 재시도가 가능하다. */
	UPROPERTY(EditAnywhere, Category="MG", meta=(ClampMin="0", ClampMax="5"))
	int32 MaxRepathAttempts = 1;

	UPROPERTY(Transient)
	FVector Destination = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector LastObservedLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float StalledSeconds = 0.0f;

	UPROPERTY(Transient)
	int32 RepathAttempts = 0;
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

/** 기존 저장 Struct 경로를 유지하면서 Claim을 Occupied로 전환하고 MG 조준·사격을 실행한다. */
USTRUCT(meta=(DisplayName="Use Reserved MG Turret", Category="Drone|AI|MG"))
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
