#include "Abilities/DronePayloadDropComponent.h"

#include "Abilities/DroneDroppedPayload.h"
#include "Abilities/DronePayloadTargetComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
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
	if (ADroneDroppedPayload* Carried = CarriedPayloadActor.Get())
	{
		Carried->ActivateCarryablePickup();
	}
	bFeatureEnabled = bEnabled;
	DropTarget.Reset();
	SuccessfulDeliveryCount = 0;
	RemainingPayloadCount = bEnabled ? FMath::Max(1, InitialPayloadCount) : 0;
	CarriedPayloadActor.Reset();
	if (!bEnabled)
	{
		SetDropViewEnabled(false);
	}
	UpdateCarriedPayloadVisual();
}

void UDronePayloadDropComponent::SetDropTarget(AActor* NewTargetActor)
{
	DropTarget = IsValid(NewTargetActor) ? NewTargetActor : nullptr;
}

void UDronePayloadDropComponent::SetDropViewEnabled(const bool bEnabled)
{
	const bool bResolvedEnabled = bFeatureEnabled && bEnabled;
	if (ADronePrototypePawn* DronePawn = Cast<ADronePrototypePawn>(GetOwner()))
	{
		DronePawn->SetDropCameraViewEnabled(bResolvedEnabled);
	}
	OnDropViewChanged.Broadcast(bResolvedEnabled);
}

bool UDronePayloadDropComponent::ActivatePrimaryPayloadAction()
{
	if (!bFeatureEnabled)
	{
		return false;
	}
	return RemainingPayloadCount > 0
		? DropPayload() != nullptr
		: TryPickupNearestCarryablePayload() != nullptr;
}

ADroneDroppedPayload* UDronePayloadDropComponent::FindBestAvailableCarryablePayload() const
{
	const AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || RemainingPayloadCount > 0)
	{
		return nullptr;
	}

	ADroneDroppedPayload* BestPayload = nullptr;
	float BestDistanceSquared = FMath::Square(FMath::Max(1.0f, CarryablePickupRangeCentimeters));
	for (TActorIterator<ADroneDroppedPayload> It(World); It; ++It)
	{
		ADroneDroppedPayload* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->IsAvailableForPickup())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(OwnerActor->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestPayload = Candidate;
		}
	}
	return BestPayload;
}

ADroneDroppedPayload* UDronePayloadDropComponent::TryPickupNearestCarryablePayload()
{
	if (!bFeatureEnabled || RemainingPayloadCount > 0)
	{
		return nullptr;
	}
	AActor* OwnerActor = GetOwner();
	USceneComponent* CarryAnchor = FindCarriedPayloadAnchor();
	ADroneDroppedPayload* Payload = FindBestAvailableCarryablePayload();
	if (!OwnerActor || !CarryAnchor || !Payload || !Payload->PrepareForCarry(OwnerActor, CarryAnchor))
	{
		return nullptr;
	}

	CarriedPayloadActor = Payload;
	RemainingPayloadCount = 1;
	UpdateCarriedPayloadVisual();
	OnPayloadPickedUp.Broadcast(Payload);
	return Payload;
}

AActor* UDronePayloadDropComponent::FindBestAvailablePayloadTarget() const
{
	const AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		UDronePayloadTargetComponent* TargetComponent = Candidate
			? Candidate->FindComponentByClass<UDronePayloadTargetComponent>()
			: nullptr;
		if (!TargetComponent || TargetComponent->IsPayloadDelivered())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			OwnerActor->GetActorLocation(),
			Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}
	return BestTarget;
}

ADroneDroppedPayload* UDronePayloadDropComponent::DropPayload()
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!bFeatureEnabled || RemainingPayloadCount <= 0 || !OwnerActor || !World || !PayloadClass)
	{
		return nullptr;
	}
	if (!DropTarget.IsValid()
		|| (DropTarget->FindComponentByClass<UDronePayloadTargetComponent>()
			&& DropTarget->FindComponentByClass<UDronePayloadTargetComponent>()->IsPayloadDelivered()))
	{
		DropTarget = FindBestAvailablePayloadTarget();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLocation = OwnerActor->GetActorTransform().TransformPosition(PayloadSpawnOffset);
	ADroneDroppedPayload* Payload = CarriedPayloadActor.Get();
	if (Payload)
	{
		Payload->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Payload->SetActorLocationAndRotation(
			SpawnLocation,
			OwnerActor->GetActorRotation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Payload->SetOwner(OwnerActor);
	}
	else
	{
		Payload = World->SpawnActor<ADroneDroppedPayload>(
			PayloadClass,
			SpawnLocation,
			OwnerActor->GetActorRotation(),
			SpawnParameters);
	}
	if (!Payload)
	{
		return nullptr;
	}

	CarriedPayloadActor.Reset();
	--RemainingPayloadCount;
	UpdateCarriedPayloadVisual();
	Payload->InitializePayload(DropTarget.Get(), InitialDownwardSpeedCentimetersPerSecond);
	Payload->OnPayloadImpact.AddUniqueDynamic(this, &UDronePayloadDropComponent::HandlePayloadImpact);
	OnPayloadDropped.Broadcast(Payload);
	return Payload;
}

void UDronePayloadDropComponent::ReloadPayloadsForMission()
{
	if (ADroneDroppedPayload* Carried = CarriedPayloadActor.Get())
	{
		Carried->Destroy();
	}
	CarriedPayloadActor.Reset();
	RemainingPayloadCount = bFeatureEnabled ? FMath::Max(1, InitialPayloadCount) : 0;
	UpdateCarriedPayloadVisual();
}

void UDronePayloadDropComponent::UpdateCarriedPayloadVisual()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents(PrimitiveComponents);
	const bool bShouldBeVisible = bFeatureEnabled && RemainingPayloadCount > 0 && !CarriedPayloadActor.IsValid();
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (Primitive && Primitive->ComponentHasTag(CarriedPayloadVisualTag))
		{
			Primitive->SetVisibility(bShouldBeVisible, true);
			Primitive->SetHiddenInGame(!bShouldBeVisible, true);
		}
	}
}

USceneComponent* UDronePayloadDropComponent::FindCarriedPayloadAnchor() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}
	if (const ADronePrototypePawn* DronePawn = Cast<ADronePrototypePawn>(OwnerActor))
	{
		if (USceneComponent* NativeAnchor = DronePawn->GetPayloadCarryAnchor())
		{
			return NativeAnchor;
		}
	}
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents(PrimitiveComponents);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (Primitive && Primitive->ComponentHasTag(CarriedPayloadVisualTag))
		{
			return Primitive;
		}
	}
	return OwnerActor->GetRootComponent();
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
