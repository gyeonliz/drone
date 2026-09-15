#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "Weather/DroneWeatherTypes.h"
#include "DroneWeatherWorldSubsystem.generated.h"

class UDroneWeatherProfile;

/** World에 활성 기상 하나를 유지하고 저빈도 Timer로 결정적 돌풍 Snapshot을 공급한다. */
UCLASS()
class DRONE_API UDroneWeatherWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Drone|Weather")
	bool ApplyWeatherProfile(UDroneWeatherProfile* Profile, bool bInstantTransition = false);

	UFUNCTION(BlueprintCallable, Category="Drone|Weather")
	void ClearWeather(bool bInstantTransition = false);

	UFUNCTION(BlueprintPure, Category="Drone|Weather")
	FDroneWeatherSnapshot GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category="Drone|Weather")
	UDroneWeatherProfile* GetActiveProfile() const { return ActiveProfile; }

	/** 자동화와 명시적 연출에서 Timer를 기다리지 않고 동일 계산을 한 단계 진행한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Weather|Debug")
	void ForceWeatherUpdate(float DeltaSeconds);

	UPROPERTY(BlueprintAssignable, Category="Drone|Weather")
	FDroneWeatherSnapshotChangedSignature OnWeatherSnapshotChanged;

private:
	void RestartUpdateTimer();
	void HandleWeatherTimer();
	FDroneWeatherSnapshot BuildTargetSnapshot(float DeltaSeconds);
	void PublishSnapshot(const FDroneWeatherSnapshot& NewSnapshot);
	static FDroneWeatherSnapshot InterpolateSnapshot(
		const FDroneWeatherSnapshot& From,
		const FDroneWeatherSnapshot& To,
		float Alpha);

	UPROPERTY(Transient)
	TObjectPtr<UDroneWeatherProfile> ActiveProfile;

	FDroneWeatherSnapshot Snapshot;
	FDroneWeatherSnapshot TransitionStartSnapshot;
	FRandomStream GustRandomStream;
	FTimerHandle WeatherUpdateTimer;
	float TransitionElapsedSeconds = 0.0f;
	float SecondsUntilNextGust = 0.0f;
	float CurrentGustSpeedMetersPerSecond = 0.0f;
	float TargetGustSpeedMetersPerSecond = 0.0f;
	float CurrentGustYawOffsetDegrees = 0.0f;
	float TargetGustYawOffsetDegrees = 0.0f;
	float CurrentVerticalGustMetersPerSecond = 0.0f;
	float TargetVerticalGustMetersPerSecond = 0.0f;
	bool bClearingWeather = false;
};
