#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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

	/** Media Source 또는 Level Sequence 형식은 FLOW-04에서 확정한다. 현재는 선택 참조만 보존한다. */
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

	/** 실제 Rule Object 형식은 Mission Director 카드에서 확정하고 지금은 안정적인 ID만 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	FName SuccessRuleId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	FName FailureRuleId = NAME_None;

	bool ValidateDefinition(FString& OutError) const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	bool IsDefinitionValid() const;
};

