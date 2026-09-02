#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AI/DroneAITypes.h"
#include "DroneNPCProfileComponent.generated.h"

/**
 * NPC의 진영·무기·MG 사용 가능 여부를 한 곳에서 공급한다.
 *
 * Character, AIController, Spawn Point, StateTree가 각자 역할 값을 중복하지 않도록
 * 프로젝트 소유 NPC Blueprint에는 이 Component를 하나만 사용한다.
 */
UCLASS(ClassGroup=(DroneAI), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneNPCProfileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneNPCProfileComponent();

	UFUNCTION(BlueprintPure, Category="Drone|AI|Profile")
	const FDroneNPCProfile& GetProfile() const { return Profile; }

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Profile")
	void SetProfile(const FDroneNPCProfile& NewProfile);

	/** Smart Object Request의 UserTags에 넣을 확정 역할 Tag를 만든다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Profile")
	FGameplayTagContainer BuildSmartObjectUserTags() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Profile")
	bool IsHostile() const { return Profile.Faction == EDroneNPCFaction::Hostile; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Profile")
	bool IsFriendly() const { return Profile.Faction == EDroneNPCFaction::Friendly; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone AI Profile")
	FDroneNPCProfile Profile;
};
