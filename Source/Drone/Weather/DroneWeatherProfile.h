#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weather/DroneWeatherTypes.h"
#include "DroneWeatherProfile.generated.h"

/** Mission과 TestMap이 선택하는 프로젝트 소유 기상 Data Asset이다. */
UCLASS(BlueprintType)
class DRONE_API UDroneWeatherProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Identity")
	FName WeatherId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Runtime", meta=(ClampMin="1.0", ClampMax="30.0", ForceUnits="Hz"))
	float GameplayUpdateHertz = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Runtime", meta=(ClampMin="0.0", ClampMax="60.0", ForceUnits="s"))
	float TransitionSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Runtime")
	int32 RandomSeed = 1337;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather", meta=(ShowOnlyInnerProperties))
	FDroneWindSettings Wind;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather", meta=(ShowOnlyInnerProperties))
	FDroneRainSettings Rain;

	bool ValidateProfile(FString& OutError) const;

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Data")
	bool IsProfileValid() const;
};
