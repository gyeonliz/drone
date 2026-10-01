#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "DroneRainPerformanceProbe.generated.h"

class UNiagaraComponent;
class ADroneRainVisualActor;

/** OilRig 원본을 수정하지 않는 비교 맵 전용. 원본/끔/근거리 원본/프로젝트 비를 같은 시점에서 비교한다. */
UCLASS(Blueprintable)
class DRONE_API ADroneRainPerformanceProbe : public ACameraActor
{
	GENERATED_BODY()
public:
	ADroneRainPerformanceProbe();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** 0=끔, 1=원본 전체, 2=가까운 원본 제한, 3=카메라 주변 프로젝트 비. */
	UFUNCTION(BlueprintCallable, Category="Drone|Weather|Performance")
	void SetRainComparisonMode(int32 Mode);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Weather|Performance", meta=(ClampMin="1", ClampMax="25"))
	int32 MaximumNearbyOriginalSystems = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Weather|Performance", meta=(ClampMin="100"))
	float OriginalRainCullDistanceCentimeters = 7000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Weather|Performance")
	bool bUseFixedComparisonCamera = true;
private:
	void CycleMode();
	void SaveMeasurements();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> OriginalRain;
	UPROPERTY(Transient)
	TObjectPtr<ADroneRainVisualActor> ProjectRain;
	TArray<float> FrameSamples;
	TArray<FString> MeasurementRows;
	int32 CurrentMode = 1;
	int32 AutomatedStage = 0;
	int32 ActiveOriginalCount = 0;
	float StageElapsedSeconds = 0.f;
	double PreviousFrameTime = 0.0;
	bool bAutomated = false;
};
