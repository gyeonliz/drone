#include "Tutorial/DroneTutorialHoverZone.h"

#include "Components/BoxComponent.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Mission/DroneMissionDirector.h"
#include "Mission/DroneMissionObjectiveTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "TimerManager.h"

ADroneTutorialHoverZone::ADroneTutorialHoverZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	HoverBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HoverBox"));
	RootComponent = HoverBox;
	HoverBox->InitBoxExtent(FVector(350.0f, 350.0f, 175.0f));
	HoverBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HoverBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HoverBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HoverBox->SetGenerateOverlapEvents(true);
}

void ADroneTutorialHoverZone::BeginPlay()
{
	Super::BeginPlay();
	HoverBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ADroneTutorialHoverZone::HandleHoverBeginOverlap);
	HoverBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ADroneTutorialHoverZone::HandleHoverEndOverlap);
}

void ADroneTutorialHoverZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEvaluation();
	HoverBox->OnComponentBeginOverlap.RemoveDynamic(this, &ADroneTutorialHoverZone::HandleHoverBeginOverlap);
	HoverBox->OnComponentEndOverlap.RemoveDynamic(this, &ADroneTutorialHoverZone::HandleHoverEndOverlap);
	Super::EndPlay(EndPlayReason);
}

void ADroneTutorialHoverZone::ResetHoverProgress()
{
	CurrentHoldSeconds = 0.0f;
	bHoverCompleted = false;
	OnHoverProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds);
}

bool ADroneTutorialHoverZone::IsStableHoverCandidate(const ADronePrototypePawn* Drone) const
{
	if (!IsValid(Drone))
	{
		return false;
	}

	const FVector Velocity = Drone->GetVelocity();
	const FRotator Rotation = Drone->GetActorRotation();
	return Velocity.Size() <= FMath::Max(0.0f, MaximumSpeedCentimetersPerSecond)
		&& FMath::Abs(Velocity.Z) <= FMath::Max(0.0f, MaximumVerticalSpeedCentimetersPerSecond)
		&& FMath::Abs(FRotator::NormalizeAxis(Rotation.Pitch)) <= MaximumTiltDegrees
		&& FMath::Abs(FRotator::NormalizeAxis(Rotation.Roll)) <= MaximumTiltDegrees;
}

void ADroneTutorialHoverZone::HandleHoverBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(OtherActor);
	if (bHoverCompleted || !IsActiveMissionDrone(Drone))
	{
		return;
	}

	OverlappingDrone = Drone;
	CurrentHoldSeconds = 0.0f;
	OnHoverProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds);
	GetWorldTimerManager().SetTimer(
		EvaluationTimerHandle,
		this,
		&ADroneTutorialHoverZone::EvaluateHoverProgress,
		FMath::Clamp(EvaluationIntervalSeconds, 0.02f, 1.0f),
		true);
}

void ADroneTutorialHoverZone::HandleHoverEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/,
	int32 /*OtherBodyIndex*/)
{
	if (OtherActor != OverlappingDrone.Get())
	{
		return;
	}
	StopEvaluation();
	if (!bHoverCompleted && bResetProgressWhenUnstable)
	{
		CurrentHoldSeconds = 0.0f;
		OnHoverProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds);
	}
}

void ADroneTutorialHoverZone::EvaluateHoverProgress()
{
	ADronePrototypePawn* Drone = OverlappingDrone.Get();
	if (!IsActiveMissionDrone(Drone) || !HoverBox->IsOverlappingActor(Drone))
	{
		StopEvaluation();
		return;
	}

	if (!IsStableHoverCandidate(Drone))
	{
		if (bResetProgressWhenUnstable && CurrentHoldSeconds > 0.0f)
		{
			CurrentHoldSeconds = 0.0f;
			OnHoverProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds);
		}
		return;
	}

	CurrentHoldSeconds = FMath::Min(
		FMath::Max(0.1f, RequiredHoldSeconds),
		CurrentHoldSeconds + FMath::Clamp(EvaluationIntervalSeconds, 0.02f, 1.0f));
	OnHoverProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds);
	if (CurrentHoldSeconds + KINDA_SMALL_NUMBER < FMath::Max(0.1f, RequiredHoldSeconds))
	{
		return;
	}

	ADroneMissionDirector* Director = ResolveMissionDirector(Drone);
	if (Director && Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::HoverMaintained, this))
	{
		bHoverCompleted = true;
		StopEvaluation();
		OnHoverCompleted.Broadcast(Drone);
	}
}

bool ADroneTutorialHoverZone::IsActiveMissionDrone(const ADronePrototypePawn* Drone) const
{
	const ADroneMissionPlayerController* Controller = Drone
		? Cast<ADroneMissionPlayerController>(Drone->GetController())
		: nullptr;
	return Controller && Controller->GetSpawnedDrone() == Drone;
}

ADroneMissionDirector* ADroneTutorialHoverZone::ResolveMissionDirector(const ADronePrototypePawn* Drone) const
{
	ADroneMissionPlayerController* Controller = Drone
		? Cast<ADroneMissionPlayerController>(Drone->GetController())
		: nullptr;
	return Controller ? Controller->GetMissionDirector() : nullptr;
}

void ADroneTutorialHoverZone::StopEvaluation()
{
	GetWorldTimerManager().ClearTimer(EvaluationTimerHandle);
	OverlappingDrone.Reset();
}
