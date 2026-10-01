#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneRainVisualActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

/**
 * 활성 카메라 주위에 가벼운 Greybox 빗줄기를 만들고, 위쪽 차폐가 있으면 로컬 강우만 줄인다.
 * World Weather 수치는 건드리지 않으므로 실내/실외 플레이어가 서로 영향을 주지 않는다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneRainVisualActor : public AActor
{
	GENERATED_BODY()

public:
	ADroneRainVisualActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	/** 실제 Tick과 같은 갱신 경로. Editor 자동화에서는 LocalPlayer 없이 지정 카메라 위치로 시험한다. */
	void UpdateRainForCamera(float DeltaSeconds, const FVector& CameraLocation);

	UFUNCTION(BlueprintCallable, Category="Drone|Weather|Rain")
	void SetRainEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Rain")
	bool IsRainEnabled() const { return bRainEnabled; }

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Rain|Indoor")
	bool IsCameraIndoors() const { return bCameraIndoors; }

	UFUNCTION(BlueprintPure, Category="Drone|Weather|Rain|Indoor")
	float GetLocalRainExposure01() const { return CurrentLocalExposure01; }

	/** 활성화/실내 진입 때도 열 추적 예산을 넘기지 않았는지 성능 테스트에서 확인한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Weather|Rain|Debug")
	int32 GetLastCeilingTraceColumnCount() const { return LastCeilingTraceColumnCount; }

	/** attenuation=1이면 지붕 아래 비 0%, 0이면 실내에서도 100%다. */
	UFUNCTION(BlueprintPure, Category="Drone|Weather|Rain|Indoor")
	static float CalculateIndoorExposure01(bool bRoofBlocked, float IndoorAttenuation01);

	/** 빗줄기 하단이 차폐 표면에 닿기 전까지만 표시한다. */
	static bool ShouldRenderStreakAboveSurface(
		float StreakCenterWorldZ,
		float StreakHalfLengthCentimeters,
		bool bBlockingSurfaceFound,
		float BlockingSurfaceWorldZ,
		float SurfaceClearanceCentimeters);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain")
	TObjectPtr<UInstancedStaticMeshComponent> RainStreaks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain")
	bool bRainEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual", meta=(ClampMin="8", ClampMax="512"))
	int32 MaximumStreakCount = 112;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual", meta=(ClampMin="100.0", ForceUnits="cm"))
	float FollowRadiusCentimeters = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual", meta=(ClampMin="100.0", ForceUnits="cm"))
	float FollowHalfHeightCentimeters = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual", meta=(ClampMin="1.0", ForceUnits="cm"))
	float StreakLengthCentimeters = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual", meta=(ClampMin="0.1", ForceUnits="cm"))
	float StreakWidthCentimeters = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual", meta=(ClampMin="100.0", ForceUnits="cm/s"))
	float FallSpeedCentimetersPerSecond = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual", meta=(ClampMin="0.0", ClampMax="2.0"))
	float WindVisualInfluence = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual")
	TObjectPtr<UStaticMesh> StreakMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Visual")
	TObjectPtr<UMaterialInterface> StreakMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor")
	bool bSuppressRainIndoors = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor", meta=(ClampMin="100.0", ForceUnits="cm"))
	float IndoorTraceDistanceCentimeters = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor", meta=(ClampMin="0.05", ForceUnits="s"))
	float IndoorCheckIntervalSeconds = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor", meta=(ClampMin="0.01", ForceUnits="s"))
	float IndoorBlendSeconds = 0.35f;

	/** 각 빗줄기의 수직 열에서 가장 높은 차폐 표면을 찾아 천장 아래 구간을 표시하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor")
	bool bClipStreaksAgainstCeilings = true;

	/** 활성화 순간도 이 예산을 지킨다. 아직 검사하지 않은 열은 숨겨 지붕 관통을 방지한다. 한 열당 Trace는 최대 두 번이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor", meta=(ClampMin="1", ClampMax="32"))
	int32 CeilingTraceBudgetPerFrame = 8;

	/** Streak 하단과 천장 사이에 남기는 작은 여유 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor", meta=(ClampMin="0.0", ForceUnits="cm"))
	float CeilingSurfaceClearanceCentimeters = 10.0f;

	/** 제공 맵의 복잡한 지붕 Mesh도 감지하도록 기본 Complex Trace를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor")
	bool bUseComplexCeilingTraces = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Indoor")
	TEnumAsByte<ECollisionChannel> CeilingTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Weather|Rain|Debug")
	bool bDrawIndoorTrace = false;

private:
	void RebuildStreakPool();
	void UpdateIndoorState(const FVector& CameraLocation);
	void UpdateCeilingSurfaceCache(const FVector& CameraLocation, int32 ActiveCount, bool bForceFullRefresh);
	void UpdateStreaks(float DeltaSeconds, const FVector& CameraLocation);

	TArray<FVector> StreakLocalPositions;
	TArray<FVector2D> StreakVisualScaleVariation;
	TArray<float> StreakBlockingSurfaceWorldZ;
	TArray<uint8> StreakHasBlockingSurface;
	TArray<uint8> StreakSurfaceCacheValid;
	TArray<FTransform> StreakTransforms;
	FRandomStream RainRandomStream;
	float IndoorCheckRemainingSeconds = 0.0f;
	float CurrentLocalExposure01 = 1.0f;
	int32 NextCeilingTraceIndex = 0;
	int32 PreviousActiveStreakCount = 0;
	int32 LastCeilingTraceColumnCount = 0;
	bool bCameraIndoors = false;
	bool bForceCeilingCacheRefresh = true;
};
