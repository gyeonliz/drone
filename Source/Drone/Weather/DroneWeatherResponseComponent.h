#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Prototype/DroneFlightControlTypes.h"
#include "Weather/DroneWeatherTypes.h"
#include "DroneWeatherResponseComponent.generated.h"

class ADronePrototypePawn;
class UDroneWeatherWorldSubsystem;

/** Weather Snapshot을 Drone 위치 Drift로 적용한다. Niagara 비 표현과는 독립적이다. */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneWeatherResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneWeatherResponseComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Wind")
	FVector GetCurrentWindDriftVelocityCentimetersPerSecond() const { return CurrentWindDriftVelocity; }

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Wind")
	FVector CalculateTargetWindDriftVelocity(
		const FDroneWeatherSnapshot& WeatherSnapshot,
		EDroneControlMode ControlMode) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Weather|Wind", meta=(ClampMin="0.0", ClampMax="2.0"))
	float DroneWindResponseMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Weather|Wind", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AssistedCompensation01 = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Weather|Wind", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LimitedAttitudeCompensation01 = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Weather|Wind", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AcroCompensation01 = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Weather|Wind", meta=(ClampMin="0.1", ForceUnits="s"))
	float ResponseTimeSeconds = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Weather|Wind", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MaximumDriftSpeedCentimetersPerSecond = 1200.0f;

private:
	UFUNCTION()
	void HandleWeatherSnapshotChanged(FDroneWeatherSnapshot NewSnapshot);

	TWeakObjectPtr<ADronePrototypePawn> OwnerDrone;
	TWeakObjectPtr<UDroneWeatherWorldSubsystem> WeatherSubsystem;
	FDroneWeatherSnapshot CachedWeatherSnapshot;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Weather|Wind")
	FVector CurrentWindDriftVelocity = FVector::ZeroVector;
};
