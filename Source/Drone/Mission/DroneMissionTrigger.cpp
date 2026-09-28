#include "Mission/DroneMissionTrigger.h"

#include "Components/BoxComponent.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Mission/DroneMissionDirector.h"
#include "Prototype/DronePrototypePawn.h"

ADroneMissionTrigger::ADroneMissionTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->InitBoxExtent(FVector(300.0f, 300.0f, 150.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}

void ADroneMissionTrigger::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ADroneMissionTrigger::HandleTriggerOverlap);
}

void ADroneMissionTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &ADroneMissionTrigger::HandleTriggerOverlap);
	Super::EndPlay(EndPlayReason);
}

bool ADroneMissionTrigger::TryActivateMissionTrigger(AActor* ActivatingActor)
{
	return ActivateMissionTriggerWithDirector(ResolveMissionDirector(ActivatingActor), ActivatingActor);
}

bool ADroneMissionTrigger::ActivateMissionTriggerWithDirector(
	ADroneMissionDirector* MissionDirector,
	AActor* ActivatingActor)
{
	if (!MissionDirector
		|| !MissionDirector->IsMissionActive()
		|| (bTriggerOnce && bHasTriggered)
		|| !IsActorAllowed(ActivatingActor))
	{
		return false;
	}

	const bool bAccepted = TriggerAction == EDroneMissionTriggerAction::FailMission
		? MissionDirector->ReportMissionFailure()
		: MissionDirector->ReportObjectiveEvent(
			ObjectiveEvent,
			bUseOverlappingActorAsEventActor ? ActivatingActor : this);
	if (!bAccepted)
	{
		return false;
	}

	bHasTriggered = true;
	OnMissionTriggerActivated.Broadcast(this, ActivatingActor);
	return true;
}

void ADroneMissionTrigger::ConfigureMissionTrigger(
	const EDroneMissionTriggerAction InAction,
	const EDroneMissionObjectiveEvent InObjectiveEvent,
	const EDroneMissionTriggerActorPolicy InActorPolicy,
	const FName InRequiredActorTag,
	const bool bInUseOverlappingActorAsEventActor)
{
	TriggerAction = InAction;
	ObjectiveEvent = InObjectiveEvent;
	ActorPolicy = InActorPolicy;
	RequiredActorTag = InRequiredActorTag;
	bUseOverlappingActorAsEventActor = bInUseOverlappingActorAsEventActor;
}

void ADroneMissionTrigger::ResetMissionTrigger()
{
	bHasTriggered = false;
}

void ADroneMissionTrigger::HandleTriggerOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	TryActivateMissionTrigger(OtherActor);
}

bool ADroneMissionTrigger::IsActorAllowed(AActor* ActivatingActor) const
{
	if (!IsValid(ActivatingActor))
	{
		return false;
	}
	if (ActorPolicy == EDroneMissionTriggerActorPolicy::AnyActor)
	{
		return true;
	}
	if (ActorPolicy == EDroneMissionTriggerActorPolicy::ActorWithTag)
	{
		return !RequiredActorTag.IsNone() && ActivatingActor->ActorHasTag(RequiredActorTag);
	}

	const ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(ActivatingActor);
	const ADroneMissionPlayerController* Controller = Drone
		? Cast<ADroneMissionPlayerController>(Drone->GetController())
		: nullptr;
	return Controller && Controller->GetSpawnedDrone() == Drone;
}

ADroneMissionDirector* ADroneMissionTrigger::ResolveMissionDirector(AActor* ActivatingActor) const
{
	if (const ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(ActivatingActor))
	{
		if (ADroneMissionPlayerController* Controller =
			Cast<ADroneMissionPlayerController>(Drone->GetController()))
		{
			return Controller->GetMissionDirector();
		}
	}

	const UWorld* World = GetWorld();
	ADroneMissionPlayerController* Controller = World
		? World->GetFirstPlayerController<ADroneMissionPlayerController>()
		: nullptr;
	return Controller ? Controller->GetMissionDirector() : nullptr;
}
