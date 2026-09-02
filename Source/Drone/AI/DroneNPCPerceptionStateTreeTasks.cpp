#include "AI/DroneNPCPerceptionStateTreeTasks.h"

#include "AI/DroneNPCAIController.h"
#include "StateTreeExecutionContext.h"

namespace
{
	ADroneNPCAIController* GetDroneController(FStateTreeExecutionContext& Context)
	{
		return Cast<ADroneNPCAIController>(Context.GetOwner());
	}
}

FDroneStateTreeDetectedTask::FDroneStateTreeDetectedTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeDetectedTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeDetectedTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ADroneNPCAIController* Controller = GetDroneController(Context);
	if (!Controller || !Controller->IsHostileNPC())
	{
		return EStateTreeRunStatus::Failed;
	}

	Controller->EnterDroneDetectedResponse();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FDroneStateTreeDetectedTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	const ADroneNPCAIController* Controller = GetDroneController(Context);
	return Controller && Controller->IsHostileNPC()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

FDroneStateTreeSearchTask::FDroneStateTreeSearchTask()
{
	bShouldCallTick = true;
}

const UStruct* FDroneStateTreeSearchTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FDroneStateTreeSearchTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;
	ADroneNPCAIController* Controller = GetDroneController(Context);
	return Controller && Controller->BeginDroneSearch(InstanceData.AcceptanceRadius)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FDroneStateTreeSearchTask::Tick(
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
		// DroneDetected Event 전환이 이 Task보다 먼저 대응 상태를 선택한다.
		return EStateTreeRunStatus::Running;
	}

	InstanceData.ElapsedTime += DeltaTime;
	if (InstanceData.ElapsedTime < FMath::Max(0.1f, InstanceData.SearchDuration))
	{
		return EStateTreeRunStatus::Running;
	}

	Controller->CompleteDroneSearch();
	return EStateTreeRunStatus::Succeeded;
}

void FDroneStateTreeSearchTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (ADroneNPCAIController* Controller = GetDroneController(Context))
	{
		if (Controller->GetResponseState() == EDroneNPCAIResponseState::Search)
		{
			Controller->StopMovement();
		}
	}
}
