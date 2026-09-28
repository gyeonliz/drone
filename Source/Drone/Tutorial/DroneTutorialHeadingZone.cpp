#include "Tutorial/DroneTutorialHeadingZone.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Mission/DroneMissionDirector.h"
#include "Mission/DroneMissionObjectiveTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "TimerManager.h"

ADroneTutorialHeadingZone::ADroneTutorialHeadingZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	HeadingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HeadingBox"));
	RootComponent = HeadingBox;
	HeadingBox->InitBoxExtent(FVector(350.0f, 350.0f, 200.0f));
	HeadingBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadingBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HeadingBox->SetGenerateOverlapEvents(true);

	TargetHeadingArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("TargetHeadingArrow"));
	TargetHeadingArrow->SetupAttachment(HeadingBox);
	TargetHeadingArrow->SetRelativeLocation(FVector(0.0f, 0.0f, -175.0f));
	TargetHeadingArrow->ArrowLength = 250.0f;
	TargetHeadingArrow->ArrowSize = 2.5f;
	TargetHeadingArrow->SetHiddenInGame(false);
}

void ADroneTutorialHeadingZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (TargetHeadingArrow)
	{
		const float LocalYaw = FRotator::NormalizeAxis(TargetHeadingDegrees - GetActorRotation().Yaw);
		TargetHeadingArrow->SetRelativeRotation(FRotator(0.0f, LocalYaw, 0.0f));
	}
}

void ADroneTutorialHeadingZone::BeginPlay()
{
	Super::BeginPlay();
	HeadingBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ADroneTutorialHeadingZone::HandleHeadingBeginOverlap);
	HeadingBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ADroneTutorialHeadingZone::HandleHeadingEndOverlap);
}

void ADroneTutorialHeadingZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEvaluation();
	HeadingBox->OnComponentBeginOverlap.RemoveDynamic(this, &ADroneTutorialHeadingZone::HandleHeadingBeginOverlap);
	HeadingBox->OnComponentEndOverlap.RemoveDynamic(this, &ADroneTutorialHeadingZone::HandleHeadingEndOverlap);
	Super::EndPlay(EndPlayReason);
}

void ADroneTutorialHeadingZone::ResetHeadingProgress()
{
	CurrentHoldSeconds = 0.0f;
	bHeadingCompleted = false;
	OnHeadingProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds, 0.0f);
}

float ADroneTutorialHeadingZone::GetAbsoluteHeadingErrorDegrees(const ADronePrototypePawn* Drone) const
{
	return IsValid(Drone)
		? FMath::Abs(FMath::FindDeltaAngleDegrees(Drone->GetActorRotation().Yaw, TargetHeadingDegrees))
		: 180.0f;
}

void ADroneTutorialHeadingZone::HandleHeadingBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(OtherActor);
	if (bHeadingCompleted || !IsActiveMissionDrone(Drone))
	{
		return;
	}

	OverlappingDrone = Drone;
	CurrentHoldSeconds = 0.0f;
	OnHeadingProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds, GetAbsoluteHeadingErrorDegrees(Drone));
	GetWorldTimerManager().SetTimer(
		EvaluationTimerHandle,
		this,
		&ADroneTutorialHeadingZone::EvaluateHeadingProgress,
		FMath::Clamp(EvaluationIntervalSeconds, 0.02f, 1.0f),
		true);
}

void ADroneTutorialHeadingZone::HandleHeadingEndOverlap(
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
	if (!bHeadingCompleted && bResetProgressWhenMisaligned)
	{
		CurrentHoldSeconds = 0.0f;
		OnHeadingProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds, 180.0f);
	}
}

void ADroneTutorialHeadingZone::EvaluateHeadingProgress()
{
	ADronePrototypePawn* Drone = OverlappingDrone.Get();
	if (!IsActiveMissionDrone(Drone) || !HeadingBox->IsOverlappingActor(Drone))
	{
		StopEvaluation();
		return;
	}

	const float HeadingError = GetAbsoluteHeadingErrorDegrees(Drone);
	if (HeadingError > FMath::Max(0.1f, HeadingToleranceDegrees))
	{
		if (bResetProgressWhenMisaligned && CurrentHoldSeconds > 0.0f)
		{
			CurrentHoldSeconds = 0.0f;
		}
		OnHeadingProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds, HeadingError);
		return;
	}

	CurrentHoldSeconds = FMath::Min(
		FMath::Max(0.1f, RequiredHoldSeconds),
		CurrentHoldSeconds + FMath::Clamp(EvaluationIntervalSeconds, 0.02f, 1.0f));
	OnHeadingProgressChanged.Broadcast(CurrentHoldSeconds, RequiredHoldSeconds, HeadingError);
	if (CurrentHoldSeconds + KINDA_SMALL_NUMBER < FMath::Max(0.1f, RequiredHoldSeconds))
	{
		return;
	}

	ADroneMissionDirector* Director = ResolveMissionDirector(Drone);
	if (Director && Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::HeadingAligned, this))
	{
		bHeadingCompleted = true;
		StopEvaluation();
		OnHeadingCompleted.Broadcast(Drone);
	}
}

bool ADroneTutorialHeadingZone::IsActiveMissionDrone(const ADronePrototypePawn* Drone) const
{
	const ADroneMissionPlayerController* Controller = Drone
		? Cast<ADroneMissionPlayerController>(Drone->GetController())
		: nullptr;
	return Controller && Controller->GetSpawnedDrone() == Drone;
}

ADroneMissionDirector* ADroneTutorialHeadingZone::ResolveMissionDirector(const ADronePrototypePawn* Drone) const
{
	ADroneMissionPlayerController* Controller = Drone
		? Cast<ADroneMissionPlayerController>(Drone->GetController())
		: nullptr;
	return Controller ? Controller->GetMissionDirector() : nullptr;
}

void ADroneTutorialHeadingZone::StopEvaluation()
{
	GetWorldTimerManager().ClearTimer(EvaluationTimerHandle);
	OverlappingDrone.Reset();
}
