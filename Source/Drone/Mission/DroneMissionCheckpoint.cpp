#include "Mission/DroneMissionCheckpoint.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Drone.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Mission/DroneMissionDirector.h"
#include "Prototype/DronePrototypePawn.h"

ADroneMissionCheckpoint::ADroneMissionCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetBoxExtent(FVector(300.0f, 300.0f, 250.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->ShapeColor = FColor(80, 220, 255);
	RootComponent = TriggerBox;

	RestartDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("RestartDirection"));
	RestartDirection->SetupAttachment(TriggerBox);
	RestartDirection->ArrowSize = 3.0f;
	RestartDirection->ArrowColor = FColor(80, 220, 255);
	RestartDirection->SetHiddenInGame(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADroneMissionCheckpoint::HandleBeginOverlap);
}

FTransform ADroneMissionCheckpoint::GetRestartTransform() const
{
	// 방향은 Yaw만 쓴다. 기울어진 채로 다시 띄우지 않는다.
	return FTransform(
		FRotator(0.0f, GetActorRotation().Yaw, 0.0f),
		GetActorLocation() + FVector::UpVector * RestartHeightOffset);
}

void ADroneMissionCheckpoint::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	TryActivate(OtherActor);
}

bool ADroneMissionCheckpoint::TryActivate(AActor* ActivatingActor)
{
	if (bActivateOnce && bHasActivated)
	{
		return false;
	}
	// 플레이어가 조종 중인 미션 기체만 받는다(NPC 탄환·차량 등은 무시).
	const ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(ActivatingActor);
	ADroneMissionPlayerController* Controller = Drone ? Cast<ADroneMissionPlayerController>(Drone->GetController()) : nullptr;
	ADroneMissionDirector* Director = Controller ? Controller->GetMissionDirector() : nullptr;
	if (!Director || !Director->IsMissionActive() || Director->GetActiveDrone() != Drone)
	{
		return false;
	}
	if (!RequiredObjectiveId.IsNone() && Director->GetCurrentObjective().ObjectiveId != RequiredObjectiveId)
	{
		return false;
	}
	const FName Id = CheckpointId.IsNone() ? GetFName() : CheckpointId;
	if (!Director->ActivateCheckpoint(Id, GetRestartTransform()))
	{
		return false;
	}
	bHasActivated = true;
	UE_LOG(LogDrone, Log, TEXT("[MISSION-CHECKPOINT] '%s' activated."), *Id.ToString());
	return true;
}
