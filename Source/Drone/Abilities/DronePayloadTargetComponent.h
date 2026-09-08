#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DronePayloadTargetComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDronePayloadDeliveredSignature,
	AActor*, TargetActor,
	AActor*, PayloadActor);

/** 드랍 Payload가 접촉해야 하는 목표 Actor에 붙이는 표식 Component다. */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDronePayloadTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDronePayloadTargetComponent();

	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Target")
	bool MarkPayloadDelivered(AActor* PayloadActor);

	UFUNCTION(BlueprintCallable, Category="Drone|Drop|Target")
	void ResetDeliveryState();

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Target")
	bool IsPayloadDelivered() const { return bPayloadDelivered; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Drop|Target")
	FDronePayloadDeliveredSignature OnPayloadDelivered;

private:
	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Drop|Target")
	bool bPayloadDelivered = false;
};
