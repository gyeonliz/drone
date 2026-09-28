#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneTutorialHeadingZone.generated.h"

class ADroneMissionDirector;
class ADronePrototypePawn;
class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDroneTutorialHeadingProgressSignature,
	float, CurrentHoldSeconds,
	float, RequiredHoldSeconds,
	float, AbsoluteHeadingErrorDegrees);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneTutorialHeadingCompletedSignature,
	ADronePrototypePawn*, AlignedDrone);

/**
 * Tutorial 회전 수업에서 플레이어 Drone이 지정 방향을 일정 시간 유지했는지 판정한다.
 * Box에 플레이어가 들어온 동안만 저빈도 Timer를 사용하며 Actor Tick은 만들지 않는다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneTutorialHeadingZone : public AActor
{
	GENERATED_BODY()

public:
	ADroneTutorialHeadingZone();

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Heading")
	UBoxComponent* GetHeadingBox() const { return HeadingBox; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Heading")
	UArrowComponent* GetTargetHeadingArrow() const { return TargetHeadingArrow; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Heading")
	float GetTargetHeadingDegrees() const { return TargetHeadingDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Heading")
	float GetHeadingToleranceDegrees() const { return HeadingToleranceDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Heading")
	float GetCurrentHoldSeconds() const { return CurrentHoldSeconds; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Heading")
	float GetRequiredHoldSeconds() const { return RequiredHoldSeconds; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Heading")
	bool IsHeadingCompleted() const { return bHeadingCompleted; }

	UFUNCTION(BlueprintCallable, Category="Drone|Tutorial|Heading")
	void ResetHeadingProgress();

	/** World Yaw의 최단 각도 차이를 사용하므로 -180/180 경계에서도 안정적으로 동작한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Heading")
	float GetAbsoluteHeadingErrorDegrees(const ADronePrototypePawn* Drone) const;

	UPROPERTY(BlueprintAssignable, Category="Drone|Tutorial|Heading")
	FDroneTutorialHeadingProgressSignature OnHeadingProgressChanged;

	UPROPERTY(BlueprintAssignable, Category="Drone|Tutorial|Heading")
	FDroneTutorialHeadingCompletedSignature OnHeadingCompleted;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Heading")
	TObjectPtr<UBoxComponent> HeadingBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Heading")
	TObjectPtr<UArrowComponent> TargetHeadingArrow;

	/** World 기준 목표 Yaw다. 화살표도 같은 방향을 표시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Heading", meta=(ClampMin="-180.0", ClampMax="180.0", ForceUnits="deg"))
	float TargetHeadingDegrees = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Heading", meta=(ClampMin="0.1", ClampMax="90.0", ForceUnits="deg"))
	float HeadingToleranceDegrees = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Heading", meta=(ClampMin="0.1", ForceUnits="s"))
	float RequiredHoldSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Heading", meta=(ClampMin="0.02", ClampMax="1.0", ForceUnits="s"))
	float EvaluationIntervalSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Heading")
	bool bResetProgressWhenMisaligned = true;

private:
	UFUNCTION()
	void HandleHeadingBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHeadingEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void EvaluateHeadingProgress();
	bool IsActiveMissionDrone(const ADronePrototypePawn* Drone) const;
	ADroneMissionDirector* ResolveMissionDirector(const ADronePrototypePawn* Drone) const;
	void StopEvaluation();

	UPROPERTY(Transient)
	TWeakObjectPtr<ADronePrototypePawn> OverlappingDrone;

	FTimerHandle EvaluationTimerHandle;
	float CurrentHoldSeconds = 0.0f;
	bool bHeadingCompleted = false;
};
