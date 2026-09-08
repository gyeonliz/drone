#include "Mission/DroneMissionDirector.h"

#include "Drone.h"
#include "EngineUtils.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneMissionDefinition.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"

ADroneMissionDirector::ADroneMissionDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void ADroneMissionDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRuntimeBindings();
	FlowSubsystem.Reset();
	MissionDefinition = nullptr;
	ActiveDrone = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool ADroneMissionDirector::InitializeMission(
	UDroneGameFlowSubsystem* InFlowSubsystem,
	UDroneMissionDefinition* InMissionDefinition,
	ADronePrototypePawn* InDrone)
{
	if (Snapshot.State != EDroneMissionRuntimeState::Inactive
		|| !InFlowSubsystem
		|| !IsValid(InMissionDefinition)
		|| !IsValid(InDrone)
		|| InFlowSubsystem->GetSnapshot().State != EDroneGameFlowState::InMission
		|| !InFlowSubsystem->GetSnapshot().bMissionStartRequested
		|| InFlowSubsystem->GetSnapshot().SelectedMissionId != InMissionDefinition->MissionId)
	{
		return false;
	}

	FlowSubsystem = InFlowSubsystem;
	MissionDefinition = InMissionDefinition;
	ActiveDrone = InDrone;
	Snapshot.MissionId = InMissionDefinition->MissionId;
	Snapshot.Objectives.Reset();
	for (int32 Index = 0; Index < InMissionDefinition->InitialObjectives.Num(); ++Index)
	{
		const FText& ObjectiveText = InMissionDefinition->InitialObjectives[Index];
		if (ObjectiveText.IsEmpty())
		{
			continue;
		}
		FDroneMissionObjectiveSnapshot Objective;
		Objective.ObjectiveId = FName(*FString::Printf(TEXT("Objective.%02d"), Index + 1));
		Objective.Description = ObjectiveText;
		Snapshot.Objectives.Add(Objective);
	}
	if (Snapshot.Objectives.IsEmpty())
	{
		UE_LOG(LogDrone, Error, TEXT("Mission '%s' has no usable initial objective."), *Snapshot.MissionId.ToString());
		FlowSubsystem.Reset();
		MissionDefinition = nullptr;
		ActiveDrone = nullptr;
		Snapshot = FDroneMissionRuntimeSnapshot();
		return false;
	}

	// Director만 시작 요청을 소비한다. UI나 Pawn은 이 Boolean을 별도로 소유하지 않는다.
	if (!InFlowSubsystem->ConsumeMissionStartRequest())
	{
		FlowSubsystem.Reset();
		MissionDefinition = nullptr;
		ActiveDrone = nullptr;
		Snapshot = FDroneMissionRuntimeSnapshot();
		return false;
	}

	Snapshot.State = EDroneMissionRuntimeState::Active;
	Snapshot.CurrentObjectiveIndex = 0;
	Snapshot.Outcome = EDroneMissionOutcome::None;
	ActiveDroneHealth = InDrone->GetHealthComponent();
	if (ActiveDroneHealth)
	{
		ActiveDroneHealth->OnDeath.AddUniqueDynamic(this, &ADroneMissionDirector::HandleDroneDeath);
	}
	BindOptionalTrainingCourse();
	BroadcastSnapshot();
	return true;
}

FDroneMissionObjectiveSnapshot ADroneMissionDirector::GetCurrentObjective() const
{
	return Snapshot.Objectives.IsValidIndex(Snapshot.CurrentObjectiveIndex)
		? Snapshot.Objectives[Snapshot.CurrentObjectiveIndex]
		: FDroneMissionObjectiveSnapshot();
}

bool ADroneMissionDirector::CompleteCurrentObjective()
{
	if (!IsMissionActive() || !Snapshot.Objectives.IsValidIndex(Snapshot.CurrentObjectiveIndex))
	{
		return false;
	}

	FDroneMissionObjectiveSnapshot& Objective = Snapshot.Objectives[Snapshot.CurrentObjectiveIndex];
	Objective.CurrentProgress = Objective.RequiredProgress;
	Objective.bCompleted = true;
	if (Snapshot.CurrentObjectiveIndex + 1 >= Snapshot.Objectives.Num())
	{
		return FinishMission(EDroneMissionOutcome::Success);
	}

	++Snapshot.CurrentObjectiveIndex;
	BroadcastSnapshot();
	return true;
}

bool ADroneMissionDirector::SetCurrentObjectiveProgress(const int32 NewProgress)
{
	if (!IsMissionActive() || !Snapshot.Objectives.IsValidIndex(Snapshot.CurrentObjectiveIndex))
	{
		return false;
	}
	FDroneMissionObjectiveSnapshot& Objective = Snapshot.Objectives[Snapshot.CurrentObjectiveIndex];
	Objective.CurrentProgress = FMath::Clamp(NewProgress, 0, FMath::Max(1, Objective.RequiredProgress));
	if (Objective.CurrentProgress >= Objective.RequiredProgress)
	{
		return CompleteCurrentObjective();
	}
	BroadcastSnapshot();
	return true;
}

bool ADroneMissionDirector::ReportMissionFailure()
{
	return FinishMission(EDroneMissionOutcome::Failure);
}

void ADroneMissionDirector::HandleDroneDeath(
	AActor* /*DeadActor*/,
	AController* /*InstigatorController*/,
	AActor* /*DamageCauser*/)
{
	ReportMissionFailure();
}

void ADroneMissionDirector::HandleTrainingLapCompleted(FDroneTrainingLapRecord /*LapRecord*/)
{
	// Training Mission의 현재 한 개 목표를 Lap 완료 Event가 만족시키는 Greybox 연결이다.
	// 최종 Mission별 Rule Object가 생겨도 CompleteCurrentObjective 경계는 그대로 재사용한다.
	CompleteCurrentObjective();
}

bool ADroneMissionDirector::FinishMission(const EDroneMissionOutcome Outcome)
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	if (!IsMissionActive()
		|| Outcome == EDroneMissionOutcome::None
		|| !Flow
		|| !Flow->CompleteMission(Outcome))
	{
		return false;
	}

	Snapshot.State = EDroneMissionRuntimeState::Finished;
	Snapshot.Outcome = Outcome;
	Snapshot.CurrentObjectiveIndex = INDEX_NONE;
	++FinishEventCount;
	ClearRuntimeBindings();
	BroadcastSnapshot();
	OnMissionFinished.Broadcast(Outcome);
	return true;
}

void ADroneMissionDirector::BindOptionalTrainingCourse()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADroneTrainingCourse> It(World); It; ++It)
		{
			ADroneTrainingCourse* Course = *It;
			if (!IsValid(Course))
			{
				continue;
			}
			TrainingLapRecorder = Course->GetLapRecorderComponent();
			break;
		}
	}
	if (TrainingLapRecorder)
	{
		TrainingLapRecorder->OnLapCompleted.AddUniqueDynamic(
			this,
			&ADroneMissionDirector::HandleTrainingLapCompleted);
	}
}

void ADroneMissionDirector::ClearRuntimeBindings()
{
	if (ActiveDroneHealth)
	{
		ActiveDroneHealth->OnDeath.RemoveDynamic(this, &ADroneMissionDirector::HandleDroneDeath);
	}
	if (TrainingLapRecorder)
	{
		TrainingLapRecorder->OnLapCompleted.RemoveDynamic(
			this,
			&ADroneMissionDirector::HandleTrainingLapCompleted);
	}
	ActiveDroneHealth = nullptr;
	TrainingLapRecorder = nullptr;
}

void ADroneMissionDirector::BroadcastSnapshot()
{
	OnMissionSnapshotChanged.Broadcast(Snapshot);
}
