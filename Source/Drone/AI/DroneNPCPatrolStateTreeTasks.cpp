#include "AI/DroneNPCPatrolStateTreeTasks.h"

#include "AI/DroneNPCAIController.h"
#include "AI/DroneSmartObjectReservationComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	ADroneNPCAIController* GetDroneController(FStateTreeExecutionContext& Context)
	{
		return Cast<ADroneNPCAIController>(Context.GetOwner());
	}

	EStateTreeRunStatus TryClaimPatrolSlot(FStateTreeExecutionContext& Context)
	{
		ADroneNPCAIController* Controller = GetDroneController(Context);
		if (!Controller || !Controller->IsHostileNPC())
		{
			return EStateTreeRunStatus::Failed;
		}

		// AI-PER-01 전에도 감지 중 순찰 Slot을 다시 잡지 않게 한다.
		// Search·공격 전환은 아직 이 Task의 책임이 아니다.
		if (Controller->HasDetectedDrone())
		{
			return EStateTreeRunStatus::Running;
		}

		FTransform SlotTransform;
		return Controller->ClaimNextEnemyPatrolSlot(SlotTransform)
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Running;
	}

	EStateTreeRunStatus TryClaimFriendlyActivitySlot(FStateTreeExecutionContext& Context)
	{
		ADroneNPCAIController* Controller = GetDroneController(Context);
		if (!Controller || !Controller->IsFriendlyNPC())
		{
			return EStateTreeRunStatus::Failed;
		}

		FTransform SlotTransform;
		return Controller->ClaimNextFriendlyActivitySlot(SlotTransform)
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Running;
	}
}

FDroneStateTreeClaimPatrolSlotTask::FDroneStateTreeClaimPatrolSlotTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeClaimPatrolSlotTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeClaimPatrolSlotTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.TimeUntilRetry = 0.0f;
	return TryClaimPatrolSlot(Context);
}

EStateTreeRunStatus FDroneStateTreeClaimPatrolSlotTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.TimeUntilRetry -= DeltaTime;
	if (InstanceData.TimeUntilRetry > 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.TimeUntilRetry = FMath::Max(0.1f, InstanceData.RetryInterval);
	return TryClaimPatrolSlot(Context);
}

FDroneStateTreeClaimFriendlyActivityTask::FDroneStateTreeClaimFriendlyActivityTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeClaimFriendlyActivityTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeClaimFriendlyActivityTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.TimeUntilRetry = 0.0f;
	return TryClaimFriendlyActivitySlot(Context);
}

EStateTreeRunStatus FDroneStateTreeClaimFriendlyActivityTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.TimeUntilRetry -= DeltaTime;
	if (InstanceData.TimeUntilRetry > 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.TimeUntilRetry = FMath::Max(0.1f, InstanceData.RetryInterval);
	return TryClaimFriendlyActivitySlot(Context);
}

FDroneStateTreeMoveToPatrolSlotTask::FDroneStateTreeMoveToPatrolSlotTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeMoveToPatrolSlotTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeMoveToPatrolSlotTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ADroneNPCAIController* Controller = GetDroneController(Context);
	if (!Controller || Controller->HasDetectedDrone())
	{
		if (Controller && Controller->GetReservationComponent())
		{
			Controller->GetReservationComponent()->ReleaseReservation();
		}
		return EStateTreeRunStatus::Failed;
	}

	FTransform SlotTransform;
	if (!Controller->GetReservationComponent()
		|| !Controller->GetReservationComponent()->GetReservedSlotTransform(SlotTransform))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Destination = SlotTransform.GetLocation();
	const EPathFollowingRequestResult::Type MoveResult = Controller->MoveToLocation(
		InstanceData.Destination,
		InstanceData.AcceptanceRadius,
		true,
		true,
		true,
		true,
		nullptr,
		false);

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		return EStateTreeRunStatus::Running;
	}
	Controller->GetReservationComponent()->ReleaseReservation();
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FDroneStateTreeMoveToPatrolSlotTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ADroneNPCAIController* Controller = GetDroneController(Context);
	if (!Controller || Controller->HasDetectedDrone())
	{
		if (Controller)
		{
			Controller->StopMovement();
			Controller->GetReservationComponent()->ReleaseReservation();
		}
		return EStateTreeRunStatus::Failed;
	}

	if (Controller->GetMoveStatus() == EPathFollowingStatus::Moving
		|| Controller->GetMoveStatus() == EPathFollowingStatus::Paused)
	{
		return EStateTreeRunStatus::Running;
	}

	const APawn* Pawn = Controller->GetPawn();
	const float ReachRadius = FMath::Max(10.0f, InstanceData.AcceptanceRadius) + 100.0f;
	if (Pawn && FVector::DistSquared2D(Pawn->GetActorLocation(), InstanceData.Destination) <= FMath::Square(ReachRadius))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	Controller->GetReservationComponent()->ReleaseReservation();
	return EStateTreeRunStatus::Failed;
}

void FDroneStateTreeMoveToPatrolSlotTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (ADroneNPCAIController* Controller = GetDroneController(Context))
	{
		if (Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			Controller->StopMovement();
		}
	}
}

FDroneStateTreeWaitAtPatrolSlotTask::FDroneStateTreeWaitAtPatrolSlotTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeWaitAtPatrolSlotTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeWaitAtPatrolSlotTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;
	const ADroneNPCAIController* Controller = GetDroneController(Context);
	return Controller && !Controller->HasDetectedDrone()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FDroneStateTreeWaitAtPatrolSlotTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ADroneNPCAIController* Controller = GetDroneController(Context);
	if (!Controller || Controller->HasDetectedDrone())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += DeltaTime;
	return InstanceData.ElapsedTime >= FMath::Max(0.0f, InstanceData.WaitDuration)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

const UStruct* FDroneStateTreeReleasePatrolSlotTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeReleasePatrolSlotTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ADroneNPCAIController* Controller = GetDroneController(Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	Controller->CompleteCurrentPatrolSlot();
	return EStateTreeRunStatus::Succeeded;
}

const UStruct* FDroneStateTreeReleaseFriendlyActivityTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeReleaseFriendlyActivityTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ADroneNPCAIController* Controller = GetDroneController(Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	Controller->CompleteCurrentFriendlyActivitySlot();
	return EStateTreeRunStatus::Succeeded;
}
