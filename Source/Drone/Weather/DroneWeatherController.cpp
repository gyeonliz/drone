#include "Weather/DroneWeatherController.h"

#include "Engine/World.h"
#include "Weather/DroneWeatherProfile.h"
#include "Weather/DroneWeatherWorldSubsystem.h"

ADroneWeatherController::ADroneWeatherController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADroneWeatherController::BeginPlay()
{
	Super::BeginPlay();
	ApplyConfiguredWeather();
}

bool ADroneWeatherController::ApplyConfiguredWeather()
{
	UWorld* World = GetWorld();
	UDroneWeatherProfile* Profile = WeatherProfile.LoadSynchronous();
	UDroneWeatherWorldSubsystem* Subsystem = World
		? World->GetSubsystem<UDroneWeatherWorldSubsystem>()
		: nullptr;
	return Subsystem && Profile && Subsystem->ApplyWeatherProfile(Profile, bApplyInstantly);
}
