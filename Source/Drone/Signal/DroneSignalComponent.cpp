#include "Signal/DroneSignalComponent.h"

UDroneSignalComponent::UDroneSignalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDroneSignalComponent::SetJammingSource(AActor* SourceActor, const float NormalizedStrength)
{
	if (!IsValid(SourceActor) || !FMath::IsFinite(NormalizedStrength))
	{
		return false;
	}
	const float Strength = FMath::Clamp(NormalizedStrength, 0.0f, 1.0f);
	if (Strength <= 0.0f)
	{
		RemoveJammingSource(SourceActor);
		return true;
	}
	Sources.Add(SourceActor, Strength);
	RecalculateSnapshot();
	return true;
}

void UDroneSignalComponent::RemoveJammingSource(AActor* SourceActor)
{
	if (SourceActor && Sources.Remove(SourceActor) > 0)
	{
		RecalculateSnapshot();
	}
}

void UDroneSignalComponent::ConfigureJammingImmunity(const bool bNewImmune)
{
	if (bJammingImmune != bNewImmune)
	{
		bJammingImmune = bNewImmune;
		RecalculateSnapshot();
	}
}

void UDroneSignalComponent::RecalculateSnapshot()
{
	float MaxStrength = 0.0f;
	for (auto It = Sources.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		if (!bJammingImmune)
		{
			MaxStrength = FMath::Max(MaxStrength, It.Value());
		}
	}

	FDroneSignalSnapshot Next;
	Next.NormalizedJammingStrength = MaxStrength;
	Next.NormalizedSignalQuality = 1.0f - MaxStrength;
	const float Weak = FMath::Clamp(WeakThreshold, 0.0f, 1.0f);
	const float Moderate = FMath::Max(Weak, FMath::Clamp(ModerateThreshold, 0.0f, 1.0f));
	const float Strong = FMath::Max(Moderate, FMath::Clamp(StrongThreshold, 0.0f, 1.0f));
	if (MaxStrength >= Strong && Strong > 0.0f)
	{
		Next.Stage = EDroneSignalInterferenceStage::Strong;
		Next.ControlResponseMultiplier = FMath::Clamp(StrongControlResponseMultiplier, 0.1f, 1.0f);
		Next.VideoNoiseIntensity = FMath::Clamp(StrongVideoNoiseIntensity, 0.0f, 1.0f);
	}
	else if (MaxStrength >= Moderate && Moderate > 0.0f)
	{
		Next.Stage = EDroneSignalInterferenceStage::Moderate;
		Next.VideoNoiseIntensity = FMath::Clamp(ModerateVideoNoiseIntensity, 0.0f, 1.0f);
	}
	else if (MaxStrength >= Weak && Weak > 0.0f)
	{
		Next.Stage = EDroneSignalInterferenceStage::Weak;
	}

	if (Snapshot.Stage != Next.Stage
		|| !FMath::IsNearlyEqual(Snapshot.NormalizedJammingStrength, Next.NormalizedJammingStrength)
		|| !FMath::IsNearlyEqual(Snapshot.ControlResponseMultiplier, Next.ControlResponseMultiplier)
		|| !FMath::IsNearlyEqual(Snapshot.VideoNoiseIntensity, Next.VideoNoiseIntensity))
	{
		Snapshot = Next;
		OnSignalSnapshotChanged.Broadcast(Snapshot);
	}
}
