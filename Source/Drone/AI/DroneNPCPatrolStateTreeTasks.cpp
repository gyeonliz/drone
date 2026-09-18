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
	InstanceData.IdleElapsedSeconds = 0.0f;
	InstanceData.MoveRetryRemainingSeconds = 0.0f;
	if (APawn* Pawn = Controller->GetPawn())
	{
		const FVector MoveDirection =
			(InstanceData.Destination - Pawn->GetActorLocation()).GetSafeNormal2D();
		if (!MoveDirection.IsNearlyZero())
		{
			// 첫 Nav 이동 프레임에는 CharacterMovement의 RotationRate가 아직 회전하지
			// 않았을 수 있다. 출발 직전 목적지 방향을 한 번만 맞춰 BlendSpace가
			// 후진/옆걸음 샘플로 시작하지 않게 한다. 이동 중 회전은 CharacterMovement가 담당한다.
			const FRotator MoveFacing(0.0f, MoveDirection.Rotation().Yaw, 0.0f);
			Controller->SetControlRotation(MoveFacing);
			Pawn->SetActorRotation(MoveFacing);
		}
	}
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
		// 순찰 슬롯의 화살표는 상호작용/경계 방향일 수 있다. 순찰에서는 그 Yaw를
		// 강제로 적용하면 다음 슬롯으로 이동할 때 로컬 속도가 뒤쪽으로 계산되어
		// 후진 BlendSpace가 선택될 수 있으므로 CharacterMovement의 이동 방향 회전에 맡긴다.
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
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ADroneNPCAIController* Controller = GetDroneController(Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Controller->HasDetectedDrone())
	{
		// 감지 이후 이동 명령은 전투 Controller가 소유한다. 순찰 Task의 늦은 Tick이
		// 새 Pursue MoveTo를 취소하면 같은 목표로 재요청하며 멈춤/재출발이 반복된다.
		Controller->GetReservationComponent()->ReleaseReservation();
		return EStateTreeRunStatus::Failed;
	}

	const EPathFollowingStatus::Type MoveStatus = Controller->GetMoveStatus();
	if (MoveStatus == EPathFollowingStatus::Moving
		|| MoveStatus == EPathFollowingStatus::Waiting)
	{
		InstanceData.IdleElapsedSeconds = 0.0f;
		InstanceData.MoveRetryRemainingSeconds = 0.0f;
		return EStateTreeRunStatus::Running;
	}

	const APawn* Pawn = Controller->GetPawn();
	const float ReachRadius = FMath::Max(10.0f, InstanceData.AcceptanceRadius) + 100.0f;
	if (Pawn && FVector::DistSquared2D(Pawn->GetActorLocation(), InstanceData.Destination) <= FMath::Square(ReachRadius))
	{
		// 도착한 순찰 NPC도 슬롯 Yaw가 아닌 실제 마지막 이동 방향을 유지한다.
		// MG/Cover 도착은 별도 Task에서 AlignPawnToReservedSlot을 사용한다.
		return EStateTreeRunStatus::Succeeded;
	}

	// PathFollowing은 경로 재평가·NavMesh 갱신 중 잠깐 Idle/Paused를 보고할 수 있다.
	// 기존 코드는 이 한 프레임을 즉시 실패로 처리해 Claim/Move를 반복했고,
	// 화면에서는 NPC가 걷다 멈추고 다시 출발하는 것처럼 보였다. 짧은 유예 동안
	// 같은 목적지로 재요청하고, 실제로 2초 이상 진전이 없을 때만 실패시킨다.
	// Paused를 정상 진행으로 인정하면 이 타이머가 매 프레임 초기화되어 영구 정체할
	// 수 있으므로 실제 이동/경로 대기만 정상 진행으로 취급한다.
	InstanceData.IdleElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	InstanceData.MoveRetryRemainingSeconds -= FMath::Max(0.0f, DeltaTime);
	if (InstanceData.IdleElapsedSeconds >= 2.0f)
	{
		Controller->GetReservationComponent()->ReleaseReservation();
		return EStateTreeRunStatus::Failed;
	}
	if (InstanceData.MoveRetryRemainingSeconds <= 0.0f)
	{
		InstanceData.MoveRetryRemainingSeconds = 0.25f;
		const EPathFollowingRequestResult::Type RetryResult = Controller->MoveToLocation(
			InstanceData.Destination,
			InstanceData.AcceptanceRadius,
			true,
			true,
			true,
			true,
			nullptr,
			false);
		if (RetryResult == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}
	return EStateTreeRunStatus::Running;
}

void FDroneStateTreeMoveToPatrolSlotTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (ADroneNPCAIController* Controller = GetDroneController(Context))
	{
		// 전투 전환 후에는 Controller가 이미 새 이동을 발행했을 수 있다. 이 Task가
		// 소유했던 순찰 이동만 정리하고 전투 추적 MoveTo는 취소하지 않는다.
		if (!Controller->HasDetectedDrone()
			&& Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
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
