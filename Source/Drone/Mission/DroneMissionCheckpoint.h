#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneMissionCheckpoint.generated.h"

class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

/**
 * MISSION-CHECKPOINT-01: 미션 맵에 놓는 재출격 지점.
 *
 * 플레이어 기체가 상자를 지나가면 Mission Director의 다음 재출격 위치를 이 Actor의 위치·방향(화살표)으로 바꾼다.
 * 실제로 재출격할지는 Mission Definition의 FailureResponse가 정한다(결과 화면이면 위치만 기록되고 쓰이지 않는다).
 *
 * 예) M1 골든 타임: 정보단말 픽업 지점에 두고 RequiredObjectiveId를 픽업 다음 목표(귀환)로 지정하면,
 *     픽업 뒤 실패했을 때 그 지점에서 다시 시작한다(Figma 1번 미션).
 */
UCLASS(Blueprintable)
class DRONE_API ADroneMissionCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	ADroneMissionCheckpoint();

	/** 로그·스냅샷에 남는 이름. 비우면 Actor 이름을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Checkpoint")
	FName CheckpointId = NAME_None;

	/** 지정하면 그 목표가 진행 중일 때만 갱신된다. 너무 일찍 지나가서 재출격 위치가 앞당겨지는 것을 막는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Checkpoint")
	FName RequiredObjectiveId = NAME_None;

	/** 한 번 갱신하면 다시 지나가도 무시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Checkpoint")
	bool bActivateOnce = true;

	/** 다시 띄울 때 이 Actor 위치에서 더할 높이(cm). 바닥에 박혀 바로 충돌하지 않게 띄운다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Checkpoint", meta=(ClampMin="0"))
	float RestartHeightOffset = 150.0f;

	/** 조건을 확인하고 Director에 재출격 위치를 넘긴다. 테스트·BP에서 직접 불러도 된다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission|Checkpoint")
	bool TryActivate(AActor* ActivatingActor);

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Checkpoint")
	bool HasActivated() const { return bHasActivated; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Checkpoint")
	FTransform GetRestartTransform() const;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Mission|Checkpoint")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** 재출격 방향. 에디터에서만 보인다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Mission|Checkpoint")
	TObjectPtr<UArrowComponent> RestartDirection;

private:
	bool bHasActivated = false;
};
