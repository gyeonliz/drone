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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleWeatherSnapshotChanged(FDroneWeatherSnapshot NewSnapshot);

	void RebuildFlowBeads();
	void UpdateFlowBeads(float DeltaSeconds);
	void HandleControlModeHotkeys() const;
	void UpdateOnScreenReadout() const;
	static float WrapCoordinate(float Value, float Extent);

	TWeakObjectPtr<UDroneWeatherWorldSubsystem> WeatherSubsystem;
	FDroneWeatherSnapshot CachedSnapshot;
	TArray<FVector> BaseBeadLocations;
	FVector DisplayedWindVelocity = FVector::ZeroVector;
	FVector FlowTravelOffset = FVector::ZeroVector;
};
