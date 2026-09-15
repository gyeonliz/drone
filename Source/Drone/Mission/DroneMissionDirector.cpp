#include "Mission/DroneMissionDirector.h"

#include "Drone.h"
#include "Abilities/DronePayloadDropComponent.h"
#include "Abilities/DroneReconScanComponent.h"
#include "EngineUtils.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneMissionDefinition.h"
#include "Prototype/DronePrototypePawn.h"
#include "Signal/DroneJammingVolume.h"
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
	if (!InMissionDefinition->ObjectiveRules.IsEmpty())
	{
		for (const FDroneMissionObjectiveRule& Rule : InMissionDefinition->ObjectiveRules)
		{
			const bool bHasStoryFact = InFlowSubsystem->HasStoryFact(Rule.StoryFactId);
			const bool bIncludeRule = Rule.StoryFactCondition == EDroneMissionStoryFactCondition::Always
				|| (Rule.StoryFactCondition == EDroneMissionStoryFactCondition::FactPresent && bHasStoryFact)
				|| (Rule.StoryFactCondition == EDroneMissionStoryFactCondition::FactAbsent && !bHasStoryFact);
			if (!bIncludeRule)
			{
				continue;
			}
			FDroneMissionObjectiveSnapshot Objective;
			Objective.ObjectiveId = Rule.ObjectiveId;
			Objective.Description = Rule.Description;
			Objective.Event = Rule.Event;
			Objective.TargetId = Rule.TargetId;
			Objective.RequiredProgress = Rule.RequiredProgress;
			Objective.TimeLimitSeconds = Rule.TimeLimitSeconds;
			Snapshot.Objectives.Add(Objective);
		}
	}
	else
	{
		// 기존 Data Asset은 변경하지 않는다. 문구마다 1회 Manual 목표로 승격한다.
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
	BindObjectiveEvents();
	StartCurrentObjectiveTimeout();
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
	CountedObjectiveActorNames.Reset();
	StartCurrentObjectiveTimeout();
	BroadcastSnapshot();
	return true;
}

bool ADroneMissionDirector::ReportObjectiveEvent(
	const EDroneMissionObjectiveEvent Event,
	AActor* EventActor)
{
	if (!IsMissionActive() || !Snapshot.Objectives.IsValidIndex(Snapshot.CurrentObjectiveIndex))
	{
		return false;
	}
	const FDroneMissionObjectiveSnapshot& Objective = Snapshot.Objectives[Snapshot.CurrentObjectiveIndex];
	if (Objective.Event != Event
		|| (!Objective.TargetId.IsNone()
			&& (!IsValid(EventActor) || !EventActor->ActorHasTag(Objective.TargetId))))
	{
		return false;
	}
	if (IsValid(EventActor) && CountedObjectiveActorNames.Contains(EventActor->GetFName()))
	{
		return false;
	}
	if (IsValid(EventActor))
	{
		CountedObjectiveActorNames.Add(EventActor->GetFName());
	}
	return SetCurrentObjectiveProgress(Objective.CurrentProgress + 1);
}

float ADroneMissionDirector::GetCurrentObjectiveTimeRemainingSeconds() const
{
	UWorld* World = GetWorld();
	return World && ObjectiveTimeoutHandle.IsValid()
		? FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(ObjectiveTimeoutHandle))
		: 0.0f;
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
	if (MissionDefinition && MissionDefinition->ObjectiveRules.IsEmpty())
	{
		// 이전 Training Mission의 Lap 성공 계약을 그대로 유지한다.
		CompleteCurrentObjective();
	}
	else
	{
		ReportObjectiveEvent(EDroneMissionObjectiveEvent::TrainingLap, nullptr);
	}
}

void ADroneMissionDirector::HandleReconScanCompleted(AActor* TargetActor)
{
	ReportObjectiveEvent(EDroneMissionObjectiveEvent::ReconScan, TargetActor);
}

void ADroneMissionDirector::HandlePayloadResolved(
	ADroneDroppedPayload* /*PayloadActor*/,
	AActor* HitActor,
	const bool bHitIntendedTarget)
{
	if (bHitIntendedTarget)
	{
		ReportObjectiveEvent(EDroneMissionObjectiveEvent::PayloadDelivered, HitActor);
	}
}

void ADroneMissionDirector::HandleObjectiveTargetDeath(
	AActor* DeadActor,
	AController* /*InstigatorController*/,
	AActor* /*DamageCauser*/)
{
	ReportObjectiveEvent(EDroneMissionObjectiveEvent::TargetDestroyed, DeadActor);
}

void ADroneMissionDirector::HandleJammerDisabled(AActor* JammerActor)
{
	ReportObjectiveEvent(EDroneMissionObjectiveEvent::JammerDisabled, JammerActor);
}

void ADroneMissionDirector::HandleDroneExitedJamming(AActor* DroneActor, AActor* JammerActor)
{
	if (DroneActor == ActiveDrone.Get())
	{
		ReportObjectiveEvent(EDroneMissionObjectiveEvent::JammingExited, JammerActor);
	}
}

void ADroneMissionDirector::HandleObjectiveTimeExpired()
{
	ReportMissionFailure();
}

bool ADroneMissionDirector::FinishMission(const EDroneMissionOutcome Outcome)
{
	UDroneGameFlowSubsystem* Flow = FlowSubsystem.Get();
	if (!IsMissionActive()
		|| Outcome == EDroneMissionOutcome::None
		|| !Flow)
	{
		return false;
	}
	const bool bFlowCompleted = Outcome == EDroneMissionOutcome::Success && MissionDefinition
		? Flow->CompleteMissionWithStoryFacts(
			Outcome,
			MissionDefinition->StoryFactsGrantedOnSuccess,
			MissionDefinition->StoryFactsRemovedOnSuccess)
		: Flow->CompleteMission(Outcome);
	if (!bFlowCompleted)
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

void ADroneMissionDirector::BindObjectiveEvents()
{
	if (ActiveDrone)
	{
		ReconScan = ActiveDrone->GetReconScanComponent();
		PayloadDrop = ActiveDrone->GetPayloadDropComponent();
	}
	if (UDroneReconScanComponent* Recon = ReconScan.Get())
	{
		Recon->OnScanCompleted.AddUniqueDynamic(this, &ADroneMissionDirector::HandleReconScanCompleted);
	}
	if (UDronePayloadDropComponent* Drop = PayloadDrop.Get())
	{
		Drop->OnPayloadResolved.AddUniqueDynamic(this, &ADroneMissionDirector::HandlePayloadResolved);
	}
	if (MissionDefinition && GetWorld())
	{
		const bool bHasJammingRule = MissionDefinition->ObjectiveRules.ContainsByPredicate(
			[](const FDroneMissionObjectiveRule& Rule)
			{
				return Rule.Event == EDroneMissionObjectiveEvent::JammingExited
					|| Rule.Event == EDroneMissionObjectiveEvent::JammerDisabled;
			});
		if (bHasJammingRule)
		{
			for (TActorIterator<ADroneJammingVolume> It(GetWorld()); It; ++It)
			{
				ADroneJammingVolume* Zone = *It;
				if (IsValid(Zone))
				{
					Zone->OnJammerDisabledNative.AddUObject(this, &ADroneMissionDirector::HandleJammerDisabled);
					Zone->OnDroneExitedJammingNative.AddUObject(this, &ADroneMissionDirector::HandleDroneExitedJamming);
					JammingVolumeBindings.Add(Zone);
				}
			}
		}
		const bool bHasDestroyRule = MissionDefinition->ObjectiveRules.ContainsByPredicate(
			[](const FDroneMissionObjectiveRule& Rule)
			{
				return Rule.Event == EDroneMissionObjectiveEvent::TargetDestroyed;
			});
		if (bHasDestroyRule)
		{
			// 목표 Actor의 체력 사망 Event만 1회 구독한다. 대상 선택은 현재 Rule의 Actor Tag로 판정한다.
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				AActor* Actor = *It;
				UDroneHealthComponent* Health = Actor && Actor != ActiveDrone.Get()
					? Actor->FindComponentByClass<UDroneHealthComponent>()
					: nullptr;
				if (Health)
				{
					Health->OnDeath.AddUniqueDynamic(this, &ADroneMissionDirector::HandleObjectiveTargetDeath);
					ObjectiveTargetHealthBindings.Add(Health);
				}
			}
		}
	}
}

void ADroneMissionDirector::StartCurrentObjectiveTimeout()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ObjectiveTimeoutHandle);
		if (Snapshot.Objectives.IsValidIndex(Snapshot.CurrentObjectiveIndex))
		{
			const float Limit = Snapshot.Objectives[Snapshot.CurrentObjectiveIndex].TimeLimitSeconds;
			if (Limit > 0.0f)
			{
				World->GetTimerManager().SetTimer(
					ObjectiveTimeoutHandle,
					this,
					&ADroneMissionDirector::HandleObjectiveTimeExpired,
					Limit,
					false);
			}
		}
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
	if (UDroneReconScanComponent* Recon = ReconScan.Get())
	{
		Recon->OnScanCompleted.RemoveDynamic(this, &ADroneMissionDirector::HandleReconScanCompleted);
	}
	if (UDronePayloadDropComponent* Drop = PayloadDrop.Get())
	{
		Drop->OnPayloadResolved.RemoveDynamic(this, &ADroneMissionDirector::HandlePayloadResolved);
	}
	for (const TWeakObjectPtr<UDroneHealthComponent>& WeakHealth : ObjectiveTargetHealthBindings)
	{
		if (UDroneHealthComponent* Health = WeakHealth.Get())
		{
			Health->OnDeath.RemoveDynamic(this, &ADroneMissionDirector::HandleObjectiveTargetDeath);
		}
	}
	for (const TWeakObjectPtr<ADroneJammingVolume>& WeakZone : JammingVolumeBindings)
	{
		if (ADroneJammingVolume* Zone = WeakZone.Get())
		{
			Zone->OnJammerDisabledNative.RemoveAll(this);
			Zone->OnDroneExitedJammingNative.RemoveAll(this);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ObjectiveTimeoutHandle);
	}
	ActiveDroneHealth = nullptr;
	TrainingLapRecorder = nullptr;
	ReconScan.Reset();
	PayloadDrop.Reset();
	ObjectiveTargetHealthBindings.Reset();
	JammingVolumeBindings.Reset();
	CountedObjectiveActorNames.Reset();
}

void ADroneMissionDirector::BroadcastSnapshot()
{
	OnMissionSnapshotChanged.Broadcast(Snapshot);
}
