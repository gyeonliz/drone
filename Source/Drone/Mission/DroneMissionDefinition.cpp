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
	if (!HasUsableObjectives())
	{
		OutError = FString::Printf(TEXT("Mission '%s'에 사용할 수 있는 목표가 없습니다."), *MissionId.ToString());
		return false;
	}

	TSet<FName> UniqueObjectiveIds;
	for (const FDroneMissionObjectiveRule& Rule : ObjectiveRules)
	{
		if (Rule.ObjectiveId.IsNone() || Rule.Description.IsEmpty()
			|| Rule.RequiredProgress < 1 || !FMath::IsFinite(Rule.TimeLimitSeconds)
			|| Rule.TimeLimitSeconds < 0.0f
			|| (Rule.StoryFactCondition == EDroneMissionStoryFactCondition::Always && !Rule.StoryFactId.IsNone())
			|| (Rule.StoryFactCondition != EDroneMissionStoryFactCondition::Always && Rule.StoryFactId.IsNone()))
		{
			OutError = FString::Printf(TEXT("Mission '%s'의 목표 Rule ID·설명·수량·제한 시간을 확인하세요."), *MissionId.ToString());
			return false;
		}
		if (UniqueObjectiveIds.Contains(Rule.ObjectiveId))
		{
			OutError = FString::Printf(TEXT("Mission '%s'의 목표 ID '%s'가 중복됐습니다."), *MissionId.ToString(), *Rule.ObjectiveId.ToString());
			return false;
		}
		UniqueObjectiveIds.Add(Rule.ObjectiveId);
	}

	TSet<FName> GrantedFacts;
	for (const FName FactId : StoryFactsGrantedOnSuccess)
	{
		if (FactId.IsNone() || GrantedFacts.Contains(FactId))
		{
			OutError = FString::Printf(TEXT("Mission '%s'의 성공 Story Fact가 비어 있거나 중복됐습니다."), *MissionId.ToString());
			return false;
		}
		GrantedFacts.Add(FactId);
	}
	TSet<FName> RemovedFacts;
	for (const FName FactId : StoryFactsRemovedOnSuccess)
	{
		if (FactId.IsNone() || RemovedFacts.Contains(FactId) || GrantedFacts.Contains(FactId))
		{
			OutError = FString::Printf(TEXT("Mission '%s'의 제거 Story Fact가 비어 있거나 중복/충돌했습니다."), *MissionId.ToString());
			return false;
		}
		RemovedFacts.Add(FactId);
	}

	OutError.Reset();
	return true;
}

bool UDroneMissionDefinition::HasUsableObjectives() const
{
	if (!ObjectiveRules.IsEmpty())
	{
		return true;
	}
	for (const FText& Objective : InitialObjectives)
	{
		if (!Objective.IsEmpty())
		{
			return true;
		}
	}
	return false;
}

bool UDroneMissionDefinition::IsDefinitionValid() const
{
	FString Error;
	return ValidateDefinition(Error);
}
