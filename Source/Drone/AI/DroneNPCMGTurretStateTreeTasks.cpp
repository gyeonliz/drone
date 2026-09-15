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

	FTransform SlotTransform;
	if (!Controller->GetReservedMGTurretOperatorTransform(SlotTransform))
	{
		Controller->AbortMGTurretResponse();
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Destination = SlotTransform.GetLocation();
	InstanceData.LastObservedLocation = Controller->GetPawn()->GetActorLocation();
	InstanceData.StalledSeconds = 0.0f;
	InstanceData.RepathAttempts = 0;
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
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
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
	const APawn* Pawn = Controller->GetPawn();
	const float OperatorSnapRadius = FMath::Max(
		FMath::Max(10.0f, InstanceData.AcceptanceRadius),
		InstanceData.OperatorSnapRadius);
	// 포탑 주변 Mesh/엄폐물 모서리에서 PathFollowing이 Moving으로 남아도 충분히 가까우면
	// 정확한 Operator Anchor로 한 번 정렬한다. 최종 위치와 회전은 Controller가 보장한다.
	if (Pawn && FVector::DistSquared2D(Pawn->GetActorLocation(), InstanceData.Destination)
		<= FMath::Square(OperatorSnapRadius))
	{
		return Controller->CompleteMGTurretMove()
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Failed;
	}
	if (Controller->GetMoveStatus() == EPathFollowingStatus::Moving
		|| Controller->GetMoveStatus() == EPathFollowingStatus::Paused)
	{
		if (!Pawn)
		{
			Controller->AbortMGTurretResponse();
			return EStateTreeRunStatus::Failed;
		}

		const float MinimumProgressDistance = FMath::Max(1.0f, InstanceData.MinimumProgressDistance);
		if (FVector::DistSquared2D(Pawn->GetActorLocation(), InstanceData.LastObservedLocation)
			>= FMath::Square(MinimumProgressDistance))
		{
			InstanceData.LastObservedLocation = Pawn->GetActorLocation();
			InstanceData.StalledSeconds = 0.0f;
			return EStateTreeRunStatus::Running;
		}

		InstanceData.StalledSeconds += FMath::Max(0.0f, DeltaTime);
		if (InstanceData.StalledSeconds < FMath::Max(0.25f, InstanceData.StallTimeoutSeconds))
		{
			return EStateTreeRunStatus::Running;
		}

		if (InstanceData.RepathAttempts >= FMath::Clamp(InstanceData.MaxRepathAttempts, 0, 5))
		{
			Controller->AbortMGTurretResponse();
			return EStateTreeRunStatus::Failed;
		}

		FTransform CurrentOperatorTransform;
		if (!Controller->GetReservedMGTurretOperatorTransform(CurrentOperatorTransform))
		{
			Controller->AbortMGTurretResponse();
			return EStateTreeRunStatus::Failed;
		}

		++InstanceData.RepathAttempts;
		InstanceData.Destination = CurrentOperatorTransform.GetLocation();
		InstanceData.LastObservedLocation = Pawn->GetActorLocation();
		InstanceData.StalledSeconds = 0.0f;
		Controller->StopMovement();
		const EPathFollowingRequestResult::Type RetryResult = Controller->MoveToLocation(
			InstanceData.Destination,
			FMath::Max(10.0f, InstanceData.AcceptanceRadius),
			true,
			true,
			true,
			true,
			nullptr,
			false);
		if (RetryResult == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			return Controller->CompleteMGTurretMove()
				? EStateTreeRunStatus::Succeeded
				: EStateTreeRunStatus::Failed;
		}
		if (RetryResult != EPathFollowingRequestResult::RequestSuccessful)
		{
			Controller->AbortMGTurretResponse();
			return EStateTreeRunStatus::Failed;
		}
		return EStateTreeRunStatus::Running;
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
