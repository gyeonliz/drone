#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weather/DroneWeatherTypes.h"
#include "DroneWeatherDebugVisualizer.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;
class UDroneWeatherWorldSubsystem;

/**
 * Weather TestMap에서 바람을 숫자와 움직이는 Bead로 즉시 판독하는 개발용 Actor다.
 * Gameplay Weather 계산에는 관여하지 않으며 Blueprint/배치 인스턴스에서 표현값을 조절한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneWeatherDebugVisualizer : public AActor
{
	GENERATED_BODY()

public:
	ADroneWeatherDebugVisualizer();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Debug")
	int32 GetFlowBeadCount() const { return FlowBeadCount; }

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Debug")
	UInstancedStaticMeshComponent* GetFlowBeads() const { return FlowBeads; }

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Debug")
	FVector GetDisplayedWindVelocityCentimetersPerSecond() const { return DisplayedWindVelocity; }

	/** TestMap 전용: 0=Clear, 1=LightWind, 2=RainStorm. 저장 자산/맵은 수정하지 않는다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Weather|Debug")
	bool ApplyTestWeatherPreset(int32 PresetIndex);

	/** 테스트 맵에서만 Snapshot의 강우·바람을 읽어 그리는 임시 디버그 빗줄기 개수다. Niagara 성능 수치가 아니다. */
	static int32 CalculateRainPreviewStreakCount(float RainIntensity01, float RainSpawnScale01, int32 MaxStreakCount);

	/** 풍향 변경 때 이전 누적 거리를 새 방향으로 재투영하지 않는 프레임 독립 적분 함수다. */
	static FVector IntegrateFlowTravelOffset(
		const FVector& CurrentOffset,
		const FVector& LocalWindVelocityCentimetersPerSecond,
		float DeltaSeconds,
		float PlaybackScale);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Blueprint에서 Sphere 대신 Arrow/Particle용 Mesh로 바꿀 수 있는 바람 비드 묶음이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Components")
	TObjectPtr<UInstancedStaticMeshComponent> FlowBeads;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual", meta=(ClampMin="4", ClampMax="128"))
	int32 FlowBeadCount = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual")
	FVector FlowAreaExtent = FVector(900.0f, 650.0f, 300.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual", meta=(ClampMin="0.01", ClampMax="1.0"))
	float FlowBeadScale = 0.12f;

	/** 실제 cm/s를 화면상 이동량으로 바꾸는 TestMap 전용 배율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual", meta=(ClampMin="0.05", ClampMax="5.0"))
	float FlowPlaybackScale = 1.0f;

	/** Snapshot이 바뀌었을 때 비드 속도·방향이 새 값으로 부드럽게 수렴하는 시간이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual", meta=(ClampMin="0.01", ClampMax="5.0", ForceUnits="s"))
	float FlowVelocityResponseSeconds = 0.35f;

	/** 이 풍속에서 비드 길이가 최대가 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual", meta=(ClampMin="100.0", ForceUnits="cm/s"))
	float FlowReferenceWindSpeedCentimetersPerSecond = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual", meta=(ClampMin="0.1", ClampMax="2.0"))
	float FlowBeadCalmLengthScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual", meta=(ClampMin="0.5", ClampMax="8.0"))
	float FlowBeadMaximumLengthScale = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Visual", meta=(ClampMin="0.1", ClampMax="1.0"))
	float FlowBeadCrossSectionScale = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Readout")
	bool bShowOnScreenReadout = true;

	/** 1=Easy, 2=Manual, 3=Rate/Acro로 바꿔 같은 바람에서 Drift 보정량을 비교한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Input")
	bool bEnableControlModeHotkeys = true;

	/** 7=Clear, 8=LightWind, 9=RainStorm. 비 표현이 아닌 Snapshot 시험 키다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Input")
	bool bEnableWeatherPresetHotkeys = true;

	/** 전용 Weather TestMap에서만 켜지는 저비용 화면 확인용 선분 프리뷰다. 실제 Niagara 비 효과가 아니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Debug|Rain", meta=(ClampMin="0", ClampMax="80"))
	int32 RainPreviewMaxStreakCount = 80;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleWeatherSnapshotChanged(FDroneWeatherSnapshot NewSnapshot);

	void RebuildFlowBeads();
	void UpdateFlowBeads(float DeltaSeconds);
	void UpdateRainDebugPreview(float DeltaSeconds);
	void HandleControlModeHotkeys();
	void UpdateOnScreenReadout() const;
	static float WrapCoordinate(float Value, float Extent);

	TWeakObjectPtr<UDroneWeatherWorldSubsystem> WeatherSubsystem;
	FDroneWeatherSnapshot CachedSnapshot;
	TArray<FVector> BaseBeadLocations;
	FVector DisplayedWindVelocity = FVector::ZeroVector;
	FVector FlowTravelOffset = FVector::ZeroVector;
	FRandomStream RainPreviewRandomStream;
	float RainPreviewElapsedSeconds = 0.0f;
	int32 CurrentRainPreviewStreakCount = 0;
	bool bRainDebugPreviewMap = false;
};
