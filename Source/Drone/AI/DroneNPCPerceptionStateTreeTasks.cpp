#include "AI/DroneNPCPerceptionStateTreeTasks.h"

#include "AI/DroneNPCAIController.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	ADroneNPCAIController* GetPerceptionDroneController(FStateTreeExecutionContext& Context)
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
	ADroneNPCAIController* Controller = GetPerceptionDroneController(Context);
	if (!Controller || !Controller->IsHostileNPC())
	{
		return EStateTreeRunStatus::Failed;
	}

	Controller->EnterDroneDetectedResponse();
	Controller->StartPersonalWeaponFire();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FDroneStateTreeDetectedTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	ADroneNPCAIController* Controller = GetPerceptionDroneController(Context);
	if (!Controller || !Controller->IsHostileNPC() || !Controller->HasDetectedDrone())
	{
		return EStateTreeRunStatus::Failed;
	}
	if (const UDroneNPCWeaponComponent* WeaponComponent = Controller->GetPossessedWeaponComponent();
		WeaponComponent && !WeaponComponent->IsFiring())
	{
		Controller->StartPersonalWeaponFire();
	}
	return EStateTreeRunStatus::Running;
}

void FDroneStateTreeDetectedTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (ADroneNPCAIController* Controller = GetPerceptionDroneController(Context))
	{
		Controller->StopPersonalWeaponFire();
	}
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
	ADroneNPCAIController* Controller = GetPerceptionDroneController(Context);
	return Controller && Controller->BeginDroneSearch(InstanceData.AcceptanceRadius)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FDroneStateTreeSearchTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ADroneNPCAIController* Controller = GetPerceptionDroneController(Context);
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
	if (ADroneNPCAIController* Controller = GetPerceptionDroneController(Context))
	{
		if (Controller->GetResponseState() == EDroneNPCAIResponseState::Search)
		{
			Controller->StopMovement();
		}
	}
}
