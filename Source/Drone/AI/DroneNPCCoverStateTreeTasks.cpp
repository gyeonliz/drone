#include "AI/DroneNPCCoverStateTreeTasks.h"

#include "AI/DroneNPCAIController.h"
#include "AI/DroneSmartObjectReservationComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
ADroneNPCAIController* GetCoverController(FStateTreeExecutionContext& Context)
{
	return Cast<ADroneNPCAIController>(Context.GetOwner());
}

bool HoldCoverStateWhileRecovering(
	ADroneNPCAIController* Controller,
	const EDroneNPCAIResponseState ExpectedState)
{
	if (!Controller
		|| Controller->GetResponseState() != ExpectedState
		|| Controller->HasSatisfiedMinimumResponseStateDuration())
	{
		return false;
	}

	// 유지시간 동안 예약·감지·개인화기 조건을 다시 확인한다. 회복되면 기존 Tick이
	// 다음 프레임 정상 경로를 계속하고, 회복되지 않아도 최소시간 뒤에만 실패 전환한다.
	Controller->MaintainCurrentResponseStateAction();
	return true;
}
}

const UStruct* FDroneStateTreeClaimCoverTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeClaimCoverTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ADroneNPCAIController* Controller = GetCoverController(Context);
	FTransform SlotTransform;
	return Controller && Controller->ClaimAvailableCover(SlotTransform)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

FDroneStateTreeMoveToCoverTask::FDroneStateTreeMoveToCoverTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeMoveToCoverTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeMoveToCoverTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ADroneNPCAIController* Controller = GetCoverController(Context);
	FTransform SlotTransform;
	if (!Controller
		|| Controller->GetResponseState() != EDroneNPCAIResponseState::MoveToCover
		|| !Controller->HasDetectedDrone()
		|| !Controller->GetReservationComponent()->GetReservedSlotTransform(SlotTransform))
	{
		if (HoldCoverStateWhileRecovering(Controller, EDroneNPCAIResponseState::MoveToCover))
		{
			return EStateTreeRunStatus::Running;
		}
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Destination = SlotTransform.GetLocation();
	const EPathFollowingRequestResult::Type MoveResult = Controller->MoveToLocation(
		InstanceData.Destination,
		FMath::Max(10.0f, InstanceData.AcceptanceRadius),
		true,
		true,
		true,
		true,
		nullptr,
		false);
	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		return Controller->CompleteCoverMove()
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Failed;
	}
	if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		return EStateTreeRunStatus::Running;
	}

	if (HoldCoverStateWhileRecovering(Controller, EDroneNPCAIResponseState::MoveToCover))
	{
		return EStateTreeRunStatus::Running;
	}
	Controller->AbortCoverResponse();
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FDroneStateTreeMoveToCoverTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ADroneNPCAIController* Controller = GetCoverController(Context);
	if (!Controller
		|| !Controller->HasDetectedDrone()
		|| !Controller->GetReservationComponent()->HasValidReservation())
	{
		if (HoldCoverStateWhileRecovering(Controller, EDroneNPCAIResponseState::MoveToCover))
		{
			return EStateTreeRunStatus::Running;
		}
		if (Controller)
		{
			Controller->AbortCoverResponse();
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
		return Controller->CompleteCoverMove()
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Failed;
	}

	if (HoldCoverStateWhileRecovering(Controller, EDroneNPCAIResponseState::MoveToCover))
	{
		return EStateTreeRunStatus::Running;
	}
	Controller->AbortCoverResponse();
	return EStateTreeRunStatus::Failed;
}

void FDroneStateTreeMoveToCoverTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (ADroneNPCAIController* Controller = GetCoverController(Context))
	{
		if (Controller->GetResponseState() == EDroneNPCAIResponseState::MoveToCover)
		{
			Controller->AbortCoverResponse();
		}
	}
}

FDroneStateTreeUseCoverTask::FDroneStateTreeUseCoverTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeUseCoverTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeUseCoverTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ADroneNPCAIController* Controller = GetCoverController(Context);
	if (Controller && Controller->UpdateCoverResponse())
	{
		return EStateTreeRunStatus::Running;
	}
	return HoldCoverStateWhileRecovering(Controller, EDroneNPCAIResponseState::UseCover)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FDroneStateTreeUseCoverTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	ADroneNPCAIController* Controller = GetCoverController(Context);
	if (Controller && Controller->UpdateCoverResponse())
	{
		return EStateTreeRunStatus::Running;
	}
	return HoldCoverStateWhileRecovering(Controller, EDroneNPCAIResponseState::UseCover)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

void FDroneStateTreeUseCoverTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (ADroneNPCAIController* Controller = GetCoverController(Context))
	{
		if (Controller->GetResponseState() == EDroneNPCAIResponseState::UseCover)
		{
			Controller->AbortCoverResponse();
		}
	}
}
