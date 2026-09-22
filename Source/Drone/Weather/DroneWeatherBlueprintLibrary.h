#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DroneWeatherBlueprintLibrary.generated.h"

/** Weather UI와 Blueprint가 같은 8방향/무풍 표기를 사용하게 하는 공용 함수다. */
UCLASS()
class DRONE_API UDroneWeatherBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Drone|Weather|UI")
	static FString GetWindCardinalLabel(FVector WindVelocityCentimetersPerSecond);

	UFUNCTION(BlueprintPure, Category="Drone|Weather|UI")
	static float GetHorizontalWindSpeedMetersPerSecond(FVector WindVelocityCentimetersPerSecond);

	UFUNCTION(BlueprintPure, Category="Drone|Weather|UI")
	static FText FormatWindReadout(FVector WindVelocityCentimetersPerSecond);
};
