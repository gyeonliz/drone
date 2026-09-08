#include "Abilities/DronePayloadTargetComponent.h"

UDronePayloadTargetComponent::UDronePayloadTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDronePayloadTargetComponent::MarkPayloadDelivered(AActor* PayloadActor)
{
	if (bPayloadDelivered || !IsValid(PayloadActor) || !IsValid(GetOwner()))
	{
		return false;
	}
	bPayloadDelivered = true;
	OnPayloadDelivered.Broadcast(GetOwner(), PayloadActor);
	return true;
}

void UDronePayloadTargetComponent::ResetDeliveryState()
{
	bPayloadDelivered = false;
}
