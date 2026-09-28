#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneTutorialHoverZone.generated.h"

class ADroneMissionDirector;
class ADronePrototypePawn;
class UBoxComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneTutorialHoverProgressSignature,
	float, CurrentHoldSeconds,
	float, RequiredHoldSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneTutorialHoverCompletedSignature,
	ADronePrototypePawn*, HoveringDrone);

/**
 * Tutorial 맵의 호버링 과제를 위한 배치형 Box다.
 * 플레이어 Drone이 Box 안에서 저속·수평 상태를 연속 유지했을 때만 Mission Event를 한 번 보고한다.
 * 검사는 Overlap 중 Timer로만 실행하므로 맵 전체 상시 Tick을 만들지 않는다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneTutorialHoverZone : public AActor
{
	GENERATED_BODY()

public:
	ADroneTutorialHoverZone();

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Hover")
	UBoxComponent* GetHoverBox() const { return HoverBox; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Hover")
	float GetCurrentHoldSeconds() const { return CurrentHoldSeconds; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Hover")
	float GetRequiredHoldSeconds() const { return RequiredHoldSeconds; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Hover")
	float GetMaximumSpeedCentimetersPerSecond() const { return MaximumSpeedCentimetersPerSecond; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Hover")
	float GetMaximumVerticalSpeedCentimetersPerSecond() const { return MaximumVerticalSpeedCentimetersPerSecond; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Hover")
	float GetMaximumTiltDegrees() const { return MaximumTiltDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Hover")
	bool IsHoverCompleted() const { return bHoverCompleted; }

	/** Blueprint 시험과 재시작에서 같은 진행 상태를 초기화한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Tutorial|Hover")
	void ResetHoverProgress();

	/** 속도와 자세만 판정한다. Box 포함 여부와 현재 출격 Drone인지는 호출 경계에서 검사한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Tutorial|Hover")
	bool IsStableHoverCandidate(const ADronePrototypePawn* Drone) const;

	UPROPERTY(BlueprintAssignable, Category="Drone|Tutorial|Hover")
	FDroneTutorialHoverProgressSignature OnHoverProgressChanged;

	UPROPERTY(BlueprintAssignable, Category="Drone|Tutorial|Hover")
	FDroneTutorialHoverCompletedSignature OnHoverCompleted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Hover")
	TObjectPtr<UBoxComponent> HoverBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Hover", meta=(ClampMin="0.1", ForceUnits="s"))
	float RequiredHoldSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Hover", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MaximumSpeedCentimetersPerSecond = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Hover", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MaximumVerticalSpeedCentimetersPerSecond = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Hover", meta=(ClampMin="0.0", ClampMax="90.0", ForceUnits="deg"))
	float MaximumTiltDegrees = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Hover", meta=(ClampMin="0.02", ClampMax="1.0", ForceUnits="s"))
	float EvaluationIntervalSeconds = 0.10f;

	/** false면 잠깐 흔들려도 누적값을 보존한다. 기본은 연속 호버 교육을 위해 true다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Tutorial|Hover")
	bool bResetProgressWhenUnstable = true;

private:
	UFUNCTION()
	void HandleHoverBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHoverEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void EvaluateHoverProgress();
	bool IsActiveMissionDrone(const ADronePrototypePawn* Drone) const;
	ADroneMissionDirector* ResolveMissionDirector(const ADronePrototypePawn* Drone) const;
	void StopEvaluation();

	UPROPERTY(Transient)
	TWeakObjectPtr<ADronePrototypePawn> OverlappingDrone;

	FTimerHandle EvaluationTimerHandle;
	float CurrentHoldSeconds = 0.0f;
	bool bHoverCompleted = false;
};
