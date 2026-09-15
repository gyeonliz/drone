#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Signal/DroneSignalTypes.h"
#include "DroneSignalComponent.generated.h"

/** 여러 재밍 Volume의 강도 중 최대값을 신호 상태로 변환한다. Tick/World 검색 없이 overlap Event만 받는다. */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneSignalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneSignalComponent();

	/** Source Actor별 강도를 갱신한다. 0은 해당 Source 제거와 같다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Signal")
	bool SetJammingSource(AActor* SourceActor, float NormalizedStrength);

	UFUNCTION(BlueprintCallable, Category="Drone|Signal")
	void RemoveJammingSource(AActor* SourceActor);

	UFUNCTION(BlueprintPure, Category="Drone|Signal")
	FDroneSignalSnapshot GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category="Drone|Signal")
	int32 GetActiveJammingSourceCount() const { return Sources.Num(); }

	/** 광섬유 등 Definition이 구현 완료로 표시한 기체만 true를 받는다. Source는 보존해 해제 시 즉시 재평가한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Signal")
	void ConfigureJammingImmunity(bool bNewImmune);

	UFUNCTION(BlueprintPure, Category="Drone|Signal")
	bool IsJammingImmune() const { return bJammingImmune; }

	/** 최종 밸런스는 BP Class Defaults에서 조정한다. 단계 Threshold는 약함 < 중간 < 강함이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Signal|Thresholds", meta=(ClampMin="0.0", ClampMax="1.0"))
	float WeakThreshold = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Signal|Thresholds", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ModerateThreshold = 0.50f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Signal|Thresholds", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StrongThreshold = 0.80f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Signal|Presentation", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ModerateVideoNoiseIntensity = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Signal|Presentation", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StrongVideoNoiseIntensity = 0.70f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Signal|Control", meta=(ClampMin="0.1", ClampMax="1.0"))
	float StrongControlResponseMultiplier = 0.70f;

	UPROPERTY(BlueprintAssignable, Category="Drone|Signal")
	FDroneSignalSnapshotChangedSignature OnSignalSnapshotChanged;

private:
	void RecalculateSnapshot();

	TMap<TWeakObjectPtr<AActor>, float> Sources;
	bool bJammingImmune = false;

	UPROPERTY(Transient)
	FDroneSignalSnapshot Snapshot;
};
