#include "Vehicles/DroneVehicleSplineRoute.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

ADroneVehicleSplineRoute::ADroneVehicleSplineRoute()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RouteSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RouteSpline"));
	RouteSpline->SetupAttachment(SceneRoot);
	RouteSpline->SetClosedLoop(false);
}

float ADroneVehicleSplineRoute::GetRouteLength() const
{
	return RouteSpline ? RouteSpline->GetSplineLength() : 0.0f;
}
