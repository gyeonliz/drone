#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneWeatherController.generated.h"

class UDroneWeatherProfile;

/** Level에 배치해 지정 Profile을 BeginPlay에 적용하는 Tick 없는 연결 Actor다. */
UCLASS(Blueprintable)
class DRONE_API ADroneWeatherController : public AActor
{
	GENERATED_BODY()

public:
	ADroneWeatherController();

	UFUNCTION(BlueprintCallable, Category="Drone|Weather")
	bool ApplyConfiguredWeather();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather")
	TSoftObjectPtr<UDroneWeatherProfile> WeatherProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather")
	bool bApplyInstantly = false;

protected:
	virtual void BeginPlay() override;
};
