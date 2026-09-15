#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Mission/DroneMissionObjectiveTypes.h"
#include "UObject/SoftObjectPath.h"
#include "DroneMissionDefinition.generated.h"

class UTexture2D;
class UWorld;

/** 로비 설명, 브리핑, Map, 허용 Drone과 시작 목표의 단일 Mission 데이터다. */
UCLASS(BlueprintType)
class DRONE_API UDroneMissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Identity")
	FName MissionId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display", meta=(MultiLine="true"))
	FText LobbyDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	FText RegionText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	FText DifficultyText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/** 선택적 영상/Sequence 참조다. 비어 있으면 FLOW-04 정적 Briefing을 사용한다. 최종 Media 형식은 현재 미정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Briefing")
	FSoftObjectPath BriefingAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Runtime")
	TSoftObjectPtr<UWorld> MissionMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Runtime")
	TArray<FName> AllowedDroneIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Runtime")
	FName DefaultDroneId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Runtime", meta=(MultiLine="true"))
	TArray<FText> InitialObjectives;

	/** 새 미션은 이 Rule 배열을 사용한다. 비어 있으면 기존 InitialObjectives를 1회 수동 목표로 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	TArray<FDroneMissionObjectiveRule> ObjectiveRules;

	/** Mission 성공 시 GameInstance 수명 Story Fact에 추가한다. 실패/재도전에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Story Branch")
	TArray<FName> StoryFactsGrantedOnSuccess;

	/** 서로 배타적인 분기를 바꿀 때 성공 시 제거할 기존 Story Fact다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Story Branch")
	TArray<FName> StoryFactsRemovedOnSuccess;

	/** 실제 Rule Object 형식은 Mission Director 카드에서 확정하고 지금은 안정적인 ID만 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	FName SuccessRuleId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	FName FailureRuleId = NAME_None;

	bool ValidateDefinition(FString& OutError) const;
	bool HasUsableObjectives() const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	bool IsDefinitionValid() const;
};
