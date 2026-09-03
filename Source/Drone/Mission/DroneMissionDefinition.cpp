#include "Mission/DroneMissionDefinition.h"

FPrimaryAssetId UDroneMissionDefinition::GetPrimaryAssetId() const
{
	return MissionId.IsNone()
		? Super::GetPrimaryAssetId()
		: FPrimaryAssetId(FPrimaryAssetType(TEXT("DroneMission")), MissionId);
}

bool UDroneMissionDefinition::ValidateDefinition(FString& OutError) const
{
	if (MissionId.IsNone())
	{
		OutError = TEXT("MissionId가 비어 있습니다.");
		return false;
	}
	if (DisplayName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Mission '%s'의 표시명이 비어 있습니다."), *MissionId.ToString());
		return false;
	}
	if (MissionMap.IsNull())
	{
		OutError = FString::Printf(TEXT("Mission '%s'의 MissionMap이 비어 있습니다."), *MissionId.ToString());
		return false;
	}
	if (AllowedDroneIds.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Mission '%s'의 허용 Drone 목록이 비어 있습니다."), *MissionId.ToString());
		return false;
	}

	TSet<FName> UniqueDroneIds;
	for (const FName DroneId : AllowedDroneIds)
	{
		if (DroneId.IsNone())
		{
			OutError = FString::Printf(TEXT("Mission '%s'의 허용 Drone ID가 비어 있습니다."), *MissionId.ToString());
			return false;
		}
		if (UniqueDroneIds.Contains(DroneId))
		{
			OutError = FString::Printf(TEXT("Mission '%s'에 Drone '%s'가 중복 등록됐습니다."), *MissionId.ToString(), *DroneId.ToString());
			return false;
		}
		UniqueDroneIds.Add(DroneId);
	}

	if (DefaultDroneId.IsNone() || !UniqueDroneIds.Contains(DefaultDroneId))
	{
		OutError = FString::Printf(TEXT("Mission '%s'의 기본 Drone이 허용 목록에 없습니다."), *MissionId.ToString());
		return false;
	}

	OutError.Reset();
	return true;
}

bool UDroneMissionDefinition::IsDefinitionValid() const
{
	FString Error;
	return ValidateDefinition(Error);
}

