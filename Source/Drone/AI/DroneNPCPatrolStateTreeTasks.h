#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "DroneNPCPatrolStateTreeTasks.generated.h"

/**
 * 빈 EnemyPatrol Slot을 찾을 때까지 일정 간격으로 재시도한다.
 *
 * 검색과 Claim Handle 수명주기는 Controller가 소유한 Reservation Component에 남긴다.
 * StateTree Task는 성공·실패 상태만 전달하므로 Handle이 여러 노드에 복제되지 않는다.
 */
USTRUCT()
struct FDroneStateTreeClaimPatrolSlotTaskInstanceData
{
	GENERATED_BODY()

	/** 다른 NPC가 모든 지점을 사용 중일 때 다시 검색하는 간격이다. */
	UPROPERTY(EditAnywhere, Category="Patrol", meta=(ClampMin="0.1", ForceUnits="s"))
	float RetryInterval = 0.5f;

	UPROPERTY(Transient)
	float TimeUntilRetry = 0.0f;
};

USTRUCT(meta=(DisplayName="Claim Enemy Patrol Slot", Category="Drone|AI|Patrol"))
struct DRONE_API FDroneStateTreeClaimPatrolSlotTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeClaimPatrolSlotTaskInstanceData;

	FDroneStateTreeClaimPatrolSlotTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

/** FriendlyBasePatrol/Ambient Slot을 찾을 때까지 재시도한다. */
USTRUCT(meta=(DisplayName="Claim Friendly Activity Slot", Category="Drone|AI|Friendly"))
struct DRONE_API FDroneStateTreeClaimFriendlyActivityTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeClaimPatrolSlotTaskInstanceData;

	FDroneStateTreeClaimFriendlyActivityTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

/** 예약된 Slot까지 NavMesh 경로로 이동한다. */
USTRUCT()
struct FDroneStateTreeMoveToPatrolSlotTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Patrol", meta=(ClampMin="10.0", ForceUnits="cm"))
	float AcceptanceRadius = 80.0f;

	UPROPERTY(Transient)
	FVector Destination = FVector::ZeroVector;
};

USTRUCT(meta=(DisplayName="Move To Reserved Patrol Slot", Category="Drone|AI|Patrol"))
struct DRONE_API FDroneStateTreeMoveToPatrolSlotTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeMoveToPatrolSlotTaskInstanceData;

	FDroneStateTreeMoveToPatrolSlotTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** Slot 도착 뒤 짧게 경계 대기한다. 최종 Animation은 AI-VIS 카드에서 연결한다. */
USTRUCT()
struct FDroneStateTreeWaitAtPatrolSlotTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Patrol", meta=(ClampMin="0.0", ForceUnits="s"))
	float WaitDuration = 1.0f;

	UPROPERTY(Transient)
	float ElapsedTime = 0.0f;
};

USTRUCT(meta=(DisplayName="Wait At Patrol Slot", Category="Drone|AI|Patrol"))
struct DRONE_API FDroneStateTreeWaitAtPatrolSlotTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeWaitAtPatrolSlotTaskInstanceData;

	FDroneStateTreeWaitAtPatrolSlotTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

/** 현재 Slot을 해제하고 완료 횟수·방문 지점을 Controller에 기록한다. */
USTRUCT()
struct FDroneStateTreeReleasePatrolSlotTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta=(DisplayName="Release Patrol Slot", Category="Drone|AI|Patrol"))
struct DRONE_API FDroneStateTreeReleasePatrolSlotTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeReleasePatrolSlotTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** 아군 활동 완료 기록을 남기고 Smart Object Claim을 해제한다. */
USTRUCT(meta=(DisplayName="Release Friendly Activity Slot", Category="Drone|AI|Friendly"))
struct DRONE_API FDroneStateTreeReleaseFriendlyActivityTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDroneStateTreeReleasePatrolSlotTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
