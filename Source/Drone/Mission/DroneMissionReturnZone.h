#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneMissionReturnZone.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

/** 맵에 배치할 수 있는 귀환 Greybox Trigger. 최종 기지 위치와 Box 크기는 Blueprint/배치 Instance가 정한다. */
UCLASS(Blueprintable)
class DRONE_API ADroneMissionReturnZone : public AActor
{
	GENERATED_BODY()

public:
	ADroneMissionReturnZone();

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Return")
	UBoxComponent* GetReturnTrigger() const { return ReturnTrigger; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Mission|Return")
	TObjectPtr<UBoxComponent> ReturnTrigger;

private:
	UFUNCTION()
	void HandleReturnOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
