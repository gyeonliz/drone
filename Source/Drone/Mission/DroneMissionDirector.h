#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mission/DroneMissionRuntimeTypes.h"
#include "DroneMissionDirector.generated.h"

class ADronePrototypePawn;
class UDroneGameFlowSubsystem;
class UDroneHealthComponent;
class UDroneMissionDefinition;
class UDroneTrainingLapRecorderComponent;
struct FDroneTrainingLapRecord;

/**
 * 선택된 Mission Definition에서 목표를 만들고 성공·실패를 Flow에 한 번만 보고한다.
 *
 * 첫 Greybox 규칙은 명시적 Blueprint 완료 호출, Training Course Lap 완료, 기체 사망이다.
 * 최종 정찰·드랍·귀환 규칙은 같은 공개 API/Event 경계에 단계적으로 추가한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneMissionDirector : public AActor
{
	GENERATED_BODY()

public:
	ADroneMissionDirector();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Controller가 Pawn Spawn/Possess와 Flow 시작 요청을 끝낸 직후 정확히 한 번 호출한다. */
	bool InitializeMission(
		UDroneGameFlowSubsystem* InFlowSubsystem,
		UDroneMissionDefinition* InMissionDefinition,
		ADronePrototypePawn* InDrone);

	UFUNCTION(BlueprintPure, Category="Drone|Mission")
	FDroneMissionRuntimeSnapshot GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission")
	FDroneMissionObjectiveSnapshot GetCurrentObjective() const;

	UFUNCTION(BlueprintPure, Category="Drone|Mission")
	bool IsMissionActive() const { return Snapshot.State == EDroneMissionRuntimeState::Active; }

	/** 현재 목표를 완료하고 다음 목표로 이동한다. 마지막 목표면 Mission Success가 된다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission")
	bool CompleteCurrentObjective();

	/** 수량형 목표가 필요할 때 사용하는 Event 기반 갱신 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission")
	bool SetCurrentObjectiveProgress(int32 NewProgress);

	/** Crash, 제한 시간, 명시적 Rule Actor가 공통으로 사용하는 실패 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission")
	bool ReportMissionFailure();

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Debug")
	int32 GetFinishEventCount() const { return FinishEventCount; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Mission")
	FDroneMissionRuntimeSnapshotChangedSignature OnMissionSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category="Drone|Mission")
	FDroneMissionRuntimeFinishedSignature OnMissionFinished;

private:
	UFUNCTION()
	void HandleDroneDeath(AActor* DeadActor, AController* InstigatorController, AActor* DamageCauser);

	UFUNCTION()
	void HandleTrainingLapCompleted(FDroneTrainingLapRecord LapRecord);

	bool FinishMission(EDroneMissionOutcome Outcome);
	void BindOptionalTrainingCourse();
	void ClearRuntimeBindings();
	void BroadcastSnapshot();

	TWeakObjectPtr<UDroneGameFlowSubsystem> FlowSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UDroneMissionDefinition> MissionDefinition;

	UPROPERTY(Transient)
	TObjectPtr<ADronePrototypePawn> ActiveDrone;

	UPROPERTY(Transient)
	TObjectPtr<UDroneHealthComponent> ActiveDroneHealth;

	UPROPERTY(Transient)
	TObjectPtr<UDroneTrainingLapRecorderComponent> TrainingLapRecorder;

	UPROPERTY(Transient)
	FDroneMissionRuntimeSnapshot Snapshot;

	UPROPERTY(Transient)
	int32 FinishEventCount = 0;
};
