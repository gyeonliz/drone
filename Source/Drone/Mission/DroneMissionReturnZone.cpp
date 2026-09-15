#include "Mission/DroneMissionReturnZone.h"

#include "Components/BoxComponent.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Mission/DroneMissionDirector.h"
#include "Prototype/DronePrototypePawn.h"

ADroneMissionReturnZone::ADroneMissionReturnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	ReturnTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ReturnTrigger"));
	RootComponent = ReturnTrigger;
	ReturnTrigger->InitBoxExtent(FVector(300.0f, 300.0f, 150.0f));
	ReturnTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ReturnTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ReturnTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ReturnTrigger->SetGenerateOverlapEvents(true);
}

void ADroneMissionReturnZone::BeginPlay()
{
	Super::BeginPlay();
	ReturnTrigger->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ADroneMissionReturnZone::HandleReturnOverlap);
}

void ADroneMissionReturnZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReturnTrigger->OnComponentBeginOverlap.RemoveDynamic(
		this,
		&ADroneMissionReturnZone::HandleReturnOverlap);
	Super::EndPlay(EndPlayReason);
}

void ADroneMissionReturnZone::HandleReturnOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(OtherActor);
	ADroneMissionPlayerController* Controller = Drone
		? Cast<ADroneMissionPlayerController>(Drone->GetController())
		: nullptr;
	ADroneMissionDirector* Director = Controller ? Controller->GetMissionDirector() : nullptr;
	if (Controller && Controller->GetSpawnedDrone() == Drone && Director && Director->IsMissionActive())
	{
		// Rule TargetId가 있으면 이 Zone Actor Tags와 일치해야 한다. 중복 Overlap은 Director가 거절한다.
		Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::ReturnToBase, this);
	}
}
