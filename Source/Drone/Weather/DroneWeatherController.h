#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneWeatherController.generated.h"

class UDroneWeatherProfile;
class ADroneRainVisualActor;
class USceneComponent;
class UStaticMeshComponent;

/** Level에 배치해 Profile과 Random Wind를 적용하는 Weather Manager Actor다. */
UCLASS(Blueprintable)
class DRONE_API ADroneWeatherController : public AActor
{
	GENERATED_BODY()

public:
	ADroneWeatherController();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Drone|Weather")
	bool ApplyConfiguredWeather();

	/** Profile의 강우 값을 유지한 채 이 Manager가 비를 즉시 켜거나 끈다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Weather|Rain")
	void SetRainEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Rain")
	bool IsRainEnabled() const { return bEnableRain; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather")
	TSoftObjectPtr<UDroneWeatherProfile> WeatherProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather")
	bool bApplyInstantly = false;

	/** false면 Wind는 유지하고 Rain 관련 Snapshot 값만 0으로 만든다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain")
	bool bEnableRain = true;

	/** 카메라 주변 강우 표현 Actor를 자동으로 한 개 생성한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain")
	bool bSpawnRainVisual = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain")
	TSubclassOf<ADroneRainVisualActor> RainVisualClass;

	/** Actor 기준 Transform을 제공하는 Root다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Weather|Editor")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	/** 배치 위치만 알려 주는 Editor 전용 Cone. Play/Package에는 존재하지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Weather|Editor")
	TObjectPtr<UStaticMeshComponent> EditorPlacementCone;
#endif

	/** 켜면 이 Blueprint를 배치하는 것만으로 8방향+무풍 Random Weather가 시작된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind")
	bool bEnableRandomWind = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind")
	bool bIncludeCalm = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind")
	int32 RandomWindSeed = 260922;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind|Direction", meta=(ClampMin="0.1", ForceUnits="s"))
	float MinimumDirectionChangeIntervalSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind|Direction", meta=(ClampMin="0.1", ForceUnits="s"))
	float MaximumDirectionChangeIntervalSeconds = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind|Speed", meta=(ClampMin="0.1", ForceUnits="s"))
	float MinimumSpeedChangeIntervalSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind|Speed", meta=(ClampMin="0.1", ForceUnits="s"))
	float MaximumSpeedChangeIntervalSeconds = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind|Speed", meta=(ClampMin="0.0", ClampMax="50.0", ForceUnits="m/s"))
	float MinimumWindSpeedMetersPerSecond = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind|Speed", meta=(ClampMin="0.0", ClampMax="50.0", ForceUnits="m/s"))
	float MaximumWindSpeedMetersPerSecond = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind|Smoothing", meta=(ClampMin="0.01", ForceUnits="s"))
	float DirectionBlendSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|RandomWind|Smoothing", meta=(ClampMin="0.01", ForceUnits="s"))
	float SpeedBlendSeconds = 1.0f;

	UFUNCTION(BlueprintPure, Category="Drone|Weather|RandomWind")
	float GetCurrentRandomWindDirectionYawDegrees() const { return CurrentWindDirectionYawDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Weather|RandomWind")
	float GetCurrentRandomWindSpeedMetersPerSecond() const { return bCurrentDirectionIsCalm ? 0.0f : CurrentWindSpeedMetersPerSecond; }

	UFUNCTION(BlueprintCallable, Category="Drone|Weather|RandomWind")
	void ChooseNewRandomWindDirection();

	UFUNCTION(BlueprintCallable, Category="Drone|Weather|RandomWind")
	void ChooseNewRandomWindSpeed();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitializeRandomWind();
	void PublishRandomWind(float DeltaSeconds);
	float RandomInterval(float MinimumSeconds, float MaximumSeconds);

	FRandomStream RandomWindStream;
	float DirectionChangeRemainingSeconds = 0.0f;
	float SpeedChangeRemainingSeconds = 0.0f;
	float CurrentWindDirectionYawDegrees = 0.0f;
	float TargetWindDirectionYawDegrees = 0.0f;
	float CurrentWindSpeedMetersPerSecond = 0.0f;
	float TargetWindSpeedMetersPerSecond = 0.0f;
	bool bCurrentDirectionIsCalm = false;
	bool bTargetDirectionIsCalm = false;

	UPROPERTY(Transient)
	TObjectPtr<ADroneRainVisualActor> SpawnedRainVisual;
};
