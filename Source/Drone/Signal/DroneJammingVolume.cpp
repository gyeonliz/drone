#include "Signal/DroneJammingVolume.h"

#include "Components/BoxComponent.h"
#include "Signal/DroneSignalComponent.h"

ADroneJammingVolume::ADroneJammingVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	JammingBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("JammingBounds"));
	RootComponent = JammingBounds;
	JammingBounds->InitBoxExtent(FVector(1000.0f, 1000.0f, 500.0f));
	JammingBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	JammingBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	JammingBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	JammingBounds->SetGenerateOverlapEvents(true);
}

void ADroneJammingVolume::BeginPlay()
{
	Super::BeginPlay();
	JammingBounds->OnComponentBeginOverlap.AddUniqueDynamic(this, &ADroneJammingVolume::HandleBeginOverlap);
	JammingBounds->OnComponentEndOverlap.AddUniqueDynamic(this, &ADroneJammingVolume::HandleEndOverlap);
	RefreshOverlappingDroneSignals(!bJammerActive || NormalizedJammingStrength <= 0.0f);
}

void ADroneJammingVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	JammingBounds->OnComponentBeginOverlap.RemoveDynamic(this, &ADroneJammingVolume::HandleBeginOverlap);
	JammingBounds->OnComponentEndOverlap.RemoveDynamic(this, &ADroneJammingVolume::HandleEndOverlap);
	RefreshOverlappingDroneSignals(true);
	Super::EndPlay(EndPlayReason);
}

void ADroneJammingVolume::SetNormalizedJammingStrength(const float NewStrength)
{
	if (!FMath::IsFinite(NewStrength))
	{
		return;
	}
	NormalizedJammingStrength = FMath::Clamp(NewStrength, 0.0f, 1.0f);
	RefreshOverlappingDroneSignals(!bJammerActive || NormalizedJammingStrength <= 0.0f);
}

bool ADroneJammingVolume::DisableJammer()
{
	if (!bJammerActive)
	{
		return false;
	}
	bJammerActive = false;
	RefreshOverlappingDroneSignals(true);
	OnJammerDisabledNative.Broadcast(this);
	OnJammerDisabled.Broadcast(this);
	return true;
}

void ADroneJammingVolume::HandleBeginOverlap(
	UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (bJammerActive && OtherActor)
	{
		if (UDroneSignalComponent* Signal = OtherActor->FindComponentByClass<UDroneSignalComponent>())
		{
			Signal->SetJammingSource(this, NormalizedJammingStrength);
		}
	}
}

void ADroneJammingVolume::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (OtherActor)
	{
		if (UDroneSignalComponent* Signal = OtherActor->FindComponentByClass<UDroneSignalComponent>())
		{
			Signal->RemoveJammingSource(this);
			if (bJammerActive && NormalizedJammingStrength > 0.0f)
			{
				OnDroneExitedJammingNative.Broadcast(OtherActor, this);
				OnDroneExitedJamming.Broadcast(OtherActor, this);
			}
		}
	}
}

void ADroneJammingVolume::RefreshOverlappingDroneSignals(const bool bRemoveSource)
{
	TArray<AActor*> OverlappingActors;
	JammingBounds->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		UDroneSignalComponent* Signal = Actor ? Actor->FindComponentByClass<UDroneSignalComponent>() : nullptr;
		if (!Signal)
		{
			continue;
		}
		if (bRemoveSource)
		{
			Signal->RemoveJammingSource(this);
		}
		else
		{
			Signal->SetJammingSource(this, NormalizedJammingStrength);
		}
	}
}
