#include "Abilities/DronePayloadDropComponent.h"

#include "Abilities/DroneDroppedPayload.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Prototype/DronePrototypePawn.h"

UDronePayloadDropComponent::UDronePayloadDropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PayloadClass = ADroneDroppedPayload::StaticClass();
}

void UDronePayloadDropComponent::ConfigureFeatureEnabled(const bool bEnabled)
{
	bFeatureEnabled = bEnabled;
	DropTarget.Reset();
	SuccessfulDeliveryCount = 0;
	RemainingPayloadCount = bEnabled ? FMath::Max(1, InitialPayloadCount) : 0;
	if (!bEnabled)
	{
		SetDropViewEnabled(false);
	}
}

void UDronePayloadDropComponent::SetDropTarget(AActor* NewTargetActor)
{
	DropTarget = IsValid(NewTargetActor) ? NewTargetActor : nullptr;
}

void UDronePayloadDropComponent::SetDropViewEnabled(const bool bEnabled)
{
	if (ADronePrototypePawn* DronePawn = Cast<ADronePrototypePawn>(GetOwner()))
	{
		DronePawn->SetDropCameraViewEnabled(bFeatureEnabled && bEnabled);
	}
}

ADroneDroppedPayload* UDronePayloadDropComponent::DropPayload()
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!bFeatureEnabled || RemainingPayloadCount <= 0 || !OwnerActor || !World || !PayloadClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLocation = OwnerActor->GetActorTransform().TransformPosition(PayloadSpawnOffset);
	ADroneDroppedPayload* Payload = World->SpawnActor<ADroneDroppedPayload>(
		PayloadClass,
		SpawnLocation,
		OwnerActor->GetActorRotation(),
		SpawnParameters);
	if (!Payload)
	{
		return nullptr;
	}

	--RemainingPayloadCount;
	Payload->InitializePayload(DropTarget.Get(), InitialDownwardSpeedCentimetersPerSecond);
	Payload->OnPayloadImpact.AddUniqueDynamic(this, &UDronePayloadDropComponent::HandlePayloadImpact);
	OnPayloadDropped.Broadcast(Payload);
	return Payload;
}

void UDronePayloadDropComponent::ReloadPayloadsForMission()
{
	RemainingPayloadCount = bFeatureEnabled ? FMath::Max(1, InitialPayloadCount) : 0;
}

void UDronePayloadDropComponent::HandlePayloadImpact(
	ADroneDroppedPayload* PayloadActor,
	AActor* HitActor,
	const bool bHitIntendedTarget)
{
	if (bHitIntendedTarget)
	{
		++SuccessfulDeliveryCount;
	}
	OnPayloadResolved.Broadcast(PayloadActor, HitActor, bHitIntendedTarget);
}
