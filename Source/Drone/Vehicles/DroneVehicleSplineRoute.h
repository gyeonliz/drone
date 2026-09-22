#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneVehicleSplineRoute.generated.h"

class USceneComponent;
class USplineComponent;

/**
 * 맵에서 차량 주행선을 직접 그리는 경량 Route Actor다.
 * Spline Point는 UE 기본 Visualizer(Alt+이동 기즈모 드래그/선분 우클릭 추가)로 편집한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneVehicleSplineRoute : public AActor
{
	GENERATED_BODY()

public:
	ADroneVehicleSplineRoute();

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|Route")
	USplineComponent* GetRouteSpline() const { return RouteSpline; }

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|Route")
	float GetRouteLength() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Route")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Route")
	TObjectPtr<USplineComponent> RouteSpline;
};
