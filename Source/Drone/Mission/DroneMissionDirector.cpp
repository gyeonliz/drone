#include "Mission/DroneMissionDirector.h"

#include "Drone.h"
#include "Abilities/DronePayloadDropComponent.h"
#include "Abilities/DroneReconScanComponent.h"
#include "EngineUtils.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Health/DroneHealthComponent.h"
#include "Health/DroneBatteryComponent.h"
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
	Snapshot.RestartCount = 0;
	Snapshot.LastCheckpointId = NAME_None;
	MissionStartWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	MissionEndWorldSeconds = -1.0;
	// 체크포인트를 아직 안 지났으면 처음 출격한 자리에서 다시 띄운다.
	RestartTransform = InDrone->GetActorTransform();
	ActiveDroneHealth = InDrone->GetHealthComponent();
	if (ActiveDroneHealth)
	{
		ActiveDroneHealth->OnDeath.AddUniqueDynamic(this, &ADroneMissionDirector::HandleDroneDeath);
	}
	ActiveDroneBattery = InDrone->GetBatteryComponent();
	if (UDroneBatteryComponent* Battery = ActiveDroneBattery.Get())
	{
		Battery->OnBatteryDepleted.AddUniqueDynamic(this, &ADroneMissionDirector::HandleBatteryDepleted);
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
	if (Event != EDroneMissionObjectiveEvent::TrainingLap
		&& IsValid(EventActor) && CountedObjectiveActorNames.Contains(EventActor->GetFName()))
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
	// 기체 파괴·제한 시간 초과·실패 Trigger가 모두 이 경로를 지난다. DA가 재출격이면 결과 화면 대신 다시 띄운다.
	if (CanRestartFromCheckpoint())
	{
		return RequestCheckpointRestart();
	}
	return FinishMission(EDroneMissionOutcome::Failure);
}

bool ADroneMissionDirector::CanRestartFromCheckpoint() const
{
	return IsMissionActive()
		&& MissionDefinition
		&& MissionDefinition->FailureResponse == EDroneMissionFailureResponse::RestartFromCheckpoint
		&& (MissionDefinition->MaxCheckpointRestarts <= 0 || Snapshot.RestartCount < MissionDefinition->MaxCheckpointRestarts)
		// 새 기체를 띄울 Controller가 없으면 재출격할 수 없으니 기존처럼 결과 화면으로 간다.
		&& OnMissionRestartRequested.IsBound();
}

bool ADroneMissionDirector::RequestCheckpointRestart()
{
	// 죽은 기체가 다시 사망 이벤트를 보내지 않게 먼저 끊고, 제한 시간도 멈춘다. 새 기체가 붙으면 다시 센다.
	UnbindActiveDroneEvents();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ObjectiveTimeoutHandle);
	}
	++Snapshot.RestartCount;
	UE_LOG(LogDrone, Log, TEXT("[MISSION-CHECKPOINT] '%s' restart %d from '%s'."),
		*Snapshot.MissionId.ToString(), Snapshot.RestartCount,
		Snapshot.LastCheckpointId.IsNone() ? TEXT("launch point") : *Snapshot.LastCheckpointId.ToString());
	BroadcastSnapshot();
	OnMissionRestartRequested.Broadcast(RestartTransform);
	return true;
}

bool ADroneMissionDirector::ActivateCheckpoint(const FName CheckpointId, const FTransform& InRestartTransform)
{
	if (!IsMissionActive())
	{
		return false;
	}
	RestartTransform = InRestartTransform;
	Snapshot.LastCheckpointId = CheckpointId;
	BroadcastSnapshot();
	return true;
}

bool ADroneMissionDirector::RebindActiveDrone(ADronePrototypePawn* NewDrone)
{
	if (!IsMissionActive() || !IsValid(NewDrone))
	{
		return false;
	}
	UnbindActiveDroneEvents();
	ActiveDrone = NewDrone;
	BindActiveDroneEvents();
	// 완료한 목표·진행 수는 유지하고, 지금 목표의 제한 시간만 처음부터 다시 센다(Figma M1: 타이머 리셋 후 재시작).
	StartCurrentObjectiveTimeout();
	BroadcastSnapshot();
	return true;
}

void ADroneMissionDirector::BindActiveDroneEvents()
{
	if (!ActiveDrone)
	{
		return;
	}
	ActiveDroneHealth = ActiveDrone->GetHealthComponent();
	if (ActiveDroneHealth)
	{
		ActiveDroneHealth->OnDeath.AddUniqueDynamic(this, &ADroneMissionDirector::HandleDroneDeath);
	}
	ActiveDroneBattery = ActiveDrone->GetBatteryComponent();
	if (UDroneBatteryComponent* Battery = ActiveDroneBattery.Get())
	{
		Battery->OnBatteryDepleted.AddUniqueDynamic(this, &ADroneMissionDirector::HandleBatteryDepleted);
	}
	ReconScan = ActiveDrone->GetReconScanComponent();
	PayloadDrop = ActiveDrone->GetPayloadDropComponent();
	if (UDroneReconScanComponent* Recon = ReconScan.Get())
	{
		Recon->OnScanCompleted.AddUniqueDynamic(this, &ADroneMissionDirector::HandleReconScanCompleted);
	}
	if (UDronePayloadDropComponent* Drop = PayloadDrop.Get())
	{
		Drop->OnPayloadResolved.AddUniqueDynamic(this, &ADroneMissionDirector::HandlePayloadResolved);
	}
}

void ADroneMissionDirector::UnbindActiveDroneEvents()
{
	if (ActiveDroneHealth)
	{
		ActiveDroneHealth->OnDeath.RemoveDynamic(this, &ADroneMissionDirector::HandleDroneDeath);
	}
	if (UDroneBatteryComponent* Battery = ActiveDroneBattery.Get())
	{
		Battery->OnBatteryDepleted.RemoveDynamic(this, &ADroneMissionDirector::HandleBatteryDepleted);
	}
	if (UDroneReconScanComponent* Recon = ReconScan.Get())
	{
		Recon->OnScanCompleted.RemoveDynamic(this, &ADroneMissionDirector::HandleReconScanCompleted);
	}
	if (UDronePayloadDropComponent* Drop = PayloadDrop.Get())
	{
		Drop->OnPayloadResolved.RemoveDynamic(this, &ADroneMissionDirector::HandlePayloadResolved);
	}
	ActiveDroneHealth = nullptr;
	ActiveDroneBattery.Reset();
	ReconScan.Reset();
	PayloadDrop.Reset();
}

bool ADroneMissionDirector::RegisterObjectiveTarget(AActor* TargetActor)
{
	if (!IsMissionActive() || !IsValid(TargetActor) || TargetActor == ActiveDrone.Get())
	{
		return false;
	}
	UDroneHealthComponent* Health = TargetActor->FindComponentByClass<UDroneHealthComponent>();
	if (!Health || ObjectiveTargetHealthBindings.Contains(Health))
	{
		return false;
	}
	Health->OnDeath.AddUniqueDynamic(this, &ADroneMissionDirector::HandleObjectiveTargetDeath);
	ObjectiveTargetHealthBindings.Add(Health);
	return true;
}

void ADroneMissionDirector::UnregisterObjectiveTarget(AActor* TargetActor)
{
	UDroneHealthComponent* Health = IsValid(TargetActor)
		? TargetActor->FindComponentByClass<UDroneHealthComponent>()
		: nullptr;
	if (!Health)
	{
		return;
	}
	Health->OnDeath.RemoveDynamic(this, &ADroneMissionDirector::HandleObjectiveTargetDeath);
	ObjectiveTargetHealthBindings.Remove(Health);
}

void ADroneMissionDirector::HandleDroneDeath(
	AActor* /*DeadActor*/,
	AController* /*InstigatorController*/,
	AActor* /*DamageCauser*/)
{
	ReportMissionFailure();
}

void ADroneMissionDirector::HandleBatteryDepleted()
{
	const UDroneBatteryComponent* Battery = ActiveDroneBattery.Get();
	if (Battery && Battery->DepletedResponse == EDroneBatteryDepletedResponse::FailMission)
	{
		ReportMissionFailure();
	}
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
		// 코스 Actor를 전달해야 원형/슬라럼 등 서로 다른 코스의 Tag를 구분할 수 있다.
		ReportObjectiveEvent(EDroneMissionObjectiveEvent::TrainingLap,
			TrainingLapRecorder ? TrainingLapRecorder->GetOwner() : nullptr);
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

double ADroneMissionDirector::GetMissionElapsedSeconds() const
{
	if (Snapshot.State == EDroneMissionRuntimeState::Inactive)
	{
		return 0.0;
	}
	const double Now = MissionEndWorldSeconds >= 0.0
		? MissionEndWorldSeconds
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : MissionStartWorldSeconds);
	return FMath::Max(0.0, Now - MissionStartWorldSeconds);
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
	static const TArray<FName> NoFacts;
	const bool bApplyFacts = Outcome == EDroneMissionOutcome::Success && MissionDefinition;
	const double FinishedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : MissionStartWorldSeconds;
	const bool bFlowCompleted = Flow->CompleteMissionWithStoryFacts(
		Outcome,
		bApplyFacts ? MissionDefinition->StoryFactsGrantedOnSuccess : NoFacts,
		bApplyFacts ? MissionDefinition->StoryFactsRemovedOnSuccess : NoFacts,
		FMath::Max(0.0, FinishedAt - MissionStartWorldSeconds));
	if (!bFlowCompleted)
	{
		return false;
	}
	MissionEndWorldSeconds = FinishedAt;

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
	FName RequiredCourseTag = NAME_None;
	if (MissionDefinition && !MissionDefinition->ObjectiveRules.IsEmpty())
	{
		const FDroneMissionObjectiveRule* LapRule = MissionDefinition->ObjectiveRules.FindByPredicate(
			[](const FDroneMissionObjectiveRule& Rule) { return Rule.Event == EDroneMissionObjectiveEvent::TrainingLap; });
		if (!LapRule) return;
		RequiredCourseTag = LapRule->TargetId;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADroneTrainingCourse> It(World); It; ++It)
		{
			ADroneTrainingCourse* Course = *It;
			if (!IsValid(Course))
			{
				continue;
			}
			const bool bMatches = RequiredCourseTag.IsNone() || Course->ActorHasTag(RequiredCourseTag);
			// 명시적 코스 목표에서는 다른 시험 코스의 선/Trigger를 꺼 혼선을 막는다.
			if (!RequiredCourseTag.IsNone()) Course->SetCourseRuntimeActive(bMatches);
			if (bMatches && !TrainingLapRecorder) TrainingLapRecorder = Course->GetLapRecorderComponent();
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
				RegisterObjectiveTarget(Actor);
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
	if (UDroneBatteryComponent* Battery = ActiveDroneBattery.Get())
	{
		Battery->OnBatteryDepleted.RemoveDynamic(this, &ADroneMissionDirector::HandleBatteryDepleted);
	}
	ActiveDroneBattery.Reset();
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
