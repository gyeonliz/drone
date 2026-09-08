#include "Abilities/DroneReconScanTargetComponent.h"

UDroneReconScanTargetComponent::UDroneReconScanTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDroneReconScanTargetComponent::MarkScanned(AActor* ScannerActor)
{
	if (bScanCompleted || !IsValid(ScannerActor) || !IsValid(GetOwner()))
	{
		return false;
	}

	bScanCompleted = true;
	OnTargetScanned.Broadcast(GetOwner(), ScannerActor);
	return true;
}

void UDroneReconScanTargetComponent::ResetScanState()
{
	bScanCompleted = false;
}
