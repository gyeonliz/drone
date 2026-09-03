#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DroneDefinition.generated.h"

class AActor;
class APawn;
class UStaticMesh;

/** Drone 선택 화면과 실제 Spawn이 함께 참조하는 프로젝트 소유 데이터 계약이다. */
UCLASS(BlueprintType)
class DRONE_API UDroneDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** 비어 있지 않고 전체 Drone Catalog에서 유일해야 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Identity")
	FName DroneId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Display", meta=(MultiLine="true"))
	FText Description;

	/** 선택 화면의 가벼운 Preview 후보다. 비워 두고 PreviewActorClass를 사용할 수도 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Preview")
	TSoftObjectPtr<UStaticMesh> PreviewMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Preview")
	TSoftClassPtr<AActor> PreviewActorClass;

	/** Drone 확정 뒤 Spawn/Possess할 프로젝트 Integration Pawn Class다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	TSoftClassPtr<APawn> PawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	FGameplayTagContainer RoleTags;

	/** 잠긴 Drone은 Mission 허용 목록에 있어도 선택할 수 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	bool bLocked = false;

	bool ValidateDefinition(FString& OutError) const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	bool IsDefinitionValid() const;
};

