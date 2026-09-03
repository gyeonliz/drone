#include "AI/DroneNPCMGTurretStateTreeTasks.h"

#include "AI/DroneNPCAIController.h"
#include "AI/DroneSmartObjectReservationComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	ADroneNPCAIController* GetMGTurretDroneController(FStateTreeExecutionContext& Context)
	{
		return Cast<ADroneNPCAIController>(Context.GetOwner());
	}
}

const UStruct* FDroneStateTreeClaimMGTurretTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeClaimMGTurretTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ADroneNPCAIController* Controller = GetMGTurretDroneController(Context);
	FTransform SlotTransform;
	return Controller && Controller->ClaimAvailableMGTurret(SlotTransform)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

FDroneStateTreeMoveToMGTurretTask::FDroneStateTreeMoveToMGTurretTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeMoveToMGTurretTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeMoveToMGTurretTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ADroneNPCAIController* Controller = GetMGTurretDroneController(Context);
	if (!Controller
		|| !Controller->HasDetectedDrone()
		|| Controller->GetResponseState() != EDroneNPCAIResponseState::MoveToMGTurret)
	{
		return EStateTreeRunStatus::Failed;
	}

	UDroneSmartObjectReservationComponent* Reservation = Controller->GetReservationComponent();
	FTransform SlotTransform;
	if (!Reservation || !Reservation->GetReservedSlotTransform(SlotTransform))
	{
		Controller->AbortMGTurretResponse();
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
		return Controller->CompleteMGTurretMove()
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Failed;
	}
	if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		return EStateTreeRunStatus::Running;
	}

	Controller->AbortMGTurretResponse();
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FDroneStateTreeMoveToMGTurretTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ADroneNPCAIController* Controller = GetMGTurretDroneController(Context);
	if (!Controller || !Controller->HasDetectedDrone())
	{
		if (Controller)
		{
			Controller->AbortMGTurretResponse();
		}
		return EStateTreeRunStatus::Failed;
	}

	UDroneSmartObjectReservationComponent* Reservation = Controller->GetReservationComponent();
	if (!Reservation || !Reservation->HasValidReservation())
	{
		Controller->AbortMGTurretResponse();
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
		return Controller->CompleteMGTurretMove()
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Failed;
	}

	Controller->AbortMGTurretResponse();
	return EStateTreeRunStatus::Failed;
}

void FDroneStateTreeMoveToMGTurretTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (ADroneNPCAIController* Controller = GetMGTurretDroneController(Context))
	{
		if (Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			Controller->StopMovement();
		}
	}
}

FDroneStateTreeHoldMGTurretTask::FDroneStateTreeHoldMGTurretTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeHoldMGTurretTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeHoldMGTurretTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ADroneNPCAIController* Controller = GetMGTurretDroneController(Context);
	return Controller && Controller->BeginMGTurretOperation()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FDroneStateTreeHoldMGTurretTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	ADroneNPCAIController* Controller = GetMGTurretDroneController(Context);
	return Controller && Controller->UpdateMGTurretOperation()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

void FDroneStateTreeHoldMGTurretTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (ADroneNPCAIController* Controller = GetMGTurretDroneController(Context))
	{
		if (Controller->GetResponseState() == EDroneNPCAIResponseState::HoldMGTurret
			|| Controller->GetResponseState() == EDroneNPCAIResponseState::UseMGTurret)
		{
			Controller->AbortMGTurretResponse();
		}
	}
}
