#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneMissionTestEntry.generated.h"
class UDroneMissionDefinition;

/** 시험맵 직접 Play의 기본 미션. 로비에서 전달된 선택은 덮어쓰지 않는다. Production에서 사용하지 않는다. */
UCLASS(Blueprintable)
class DRONE_API ADroneMissionTestEntry : public AActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Test Entry")
	TObjectPtr<UDroneMissionDefinition> DefaultTestMission;
};
