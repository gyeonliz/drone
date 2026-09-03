#include "Mission/DroneDefinition.h"

FPrimaryAssetId UDroneDefinition::GetPrimaryAssetId() const
{
	return DroneId.IsNone()
		? Super::GetPrimaryAssetId()
		: FPrimaryAssetId(FPrimaryAssetType(TEXT("DroneDefinition")), DroneId);
}

bool UDroneDefinition::ValidateDefinition(FString& OutError) const
{
	if (DroneId.IsNone())
	{
		OutError = TEXT("DroneId가 비어 있습니다.");
		return false;
	}
	if (DisplayName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Drone '%s'의 표시명이 비어 있습니다."), *DroneId.ToString());
		return false;
	}
	if (PawnClass.IsNull())
	{
		OutError = FString::Printf(TEXT("Drone '%s'의 PawnClass가 비어 있습니다."), *DroneId.ToString());
		return false;
	}

	OutError.Reset();
	return true;
}

bool UDroneDefinition::IsDefinitionValid() const
{
	FString Error;
	return ValidateDefinition(Error);
}

