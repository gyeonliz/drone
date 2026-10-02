#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Mission/DroneMissionRuntimeTypes.h"
#include "DroneMissionDirector.generated.h"

class ADronePrototypePawn;
class ADroneDroppedPayload;
class ADroneJammingVolume;
class AController;
class UDroneGameFlowSubsystem;
class UDroneHealthComponent;
class UDroneBatteryComponent;
class UDroneMissionDefinition;
class UDroneReconScanComponent;
class UDronePayloadDropComponent;
class UDroneTrainingLapRecorderComponent;
struct FDroneTrainingLapRecord;

/**
 * 선택된 Mission Definition에서 목표를 만들고 성공·실패를 Flow에 한 번만 보고한다.
 *
 * 기존 Training의 수동/Lap 계약은 유지하고, 새 Mission Rule은 사건 종류·수량·대상 Tag·제한 시간으로 진행한다.
 * Return 지점은 맵/BP가 ReportObjectiveEvent를 호출하며 아직 최종 배치·트리거 방식은 정하지 않았다.
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

	/** HUD도 판정기와 같은 코스를 읽는다. 맵의 첫 Actor에 의존하지 않는다. */
	UFUNCTION(BlueprintPure, Category="Drone|Mission")
	UDroneTrainingLapRecorderComponent* GetTrainingLapRecorder() const { return TrainingLapRecorder; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission")
	bool IsMissionActive() const { return Snapshot.State == EDroneMissionRuntimeState::Active; }

	/** 출격부터 지금까지(끝났으면 끝난 순간까지) 걸린 게임 시간. 재출격 시간도 포함한다(TUT-PROGRESS-01). */
	UFUNCTION(BlueprintPure, Category="Drone|Mission")
	double GetMissionElapsedSeconds() const;

	/** 현재 목표를 완료하고 다음 목표로 이동한다. 마지막 목표면 Mission Success가 된다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission")
	bool CompleteCurrentObjective();

	/** 수량형 목표가 필요할 때 사용하는 Event 기반 갱신 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission")
	bool SetCurrentObjectiveProgress(int32 NewProgress);

	/** 사건 종류와 Actor Tags의 선택적 TargetId가 현재 Rule과 맞을 때만 1회 진행한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission")
	bool ReportObjectiveEvent(EDroneMissionObjectiveEvent Event, AActor* EventActor);

	UFUNCTION(BlueprintPure, Category="Drone|Mission")
	float GetCurrentObjectiveTimeRemainingSeconds() const;

	/** Crash, 제한 시간, 명시적 Rule Actor가 공통으로 사용하는 실패 경계다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission")
	bool ReportMissionFailure();

	/** Mission 시작 뒤 Spawn된 차량·포탑·시설의 Health 사망 Event를 파괴 목표에 연결한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission|Targets")
	bool RegisterObjectiveTarget(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Drone|Mission|Targets")
	void UnregisterObjectiveTarget(AActor* TargetActor);

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Debug")
	int32 GetFinishEventCount() const { return FinishEventCount; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Mission")
	FDroneMissionRuntimeSnapshotChangedSignature OnMissionSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category="Drone|Mission")
	FDroneMissionRuntimeFinishedSignature OnMissionFinished;

	/**
	 * MISSION-CHECKPOINT-01. 기체가 체크포인트를 지나면 다음 재출격 위치를 바꾼다. 진행 중일 때만 받는다.
	 * 맵의 DroneMissionCheckpoint가 부르며, BP에서 직접 불러도 된다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission|Checkpoint")
	bool ActivateCheckpoint(FName CheckpointId, const FTransform& InRestartTransform);

	/** 지금 실패하면 결과 화면 대신 재출격할지: DA 설정, 남은 횟수, 재출격을 처리할 Controller 연결을 모두 본다. */
	UFUNCTION(BlueprintPure, Category="Drone|Mission|Checkpoint")
	bool CanRestartFromCheckpoint() const;

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Checkpoint")
	FTransform GetRestartTransform() const { return RestartTransform; }

	/** 재출격으로 새 기체를 띄운 Controller가 부른다. 사망·정찰·투하 이벤트를 새 기체로 옮기고 현재 목표 제한 시간을 다시 센다. */
	bool RebindActiveDrone(ADronePrototypePawn* NewDrone);

	ADronePrototypePawn* GetActiveDrone() const { return ActiveDrone; }

	/** 실패를 재출격으로 처리할 때 Controller에게 다시 띄울 위치를 알린다. */
	UPROPERTY(BlueprintAssignable, Category="Drone|Mission|Checkpoint")
	FDroneMissionRestartRequestedSignature OnMissionRestartRequested;

private:
	UFUNCTION()
	void HandleDroneDeath(AActor* DeadActor, AController* InstigatorController, AActor* DamageCauser);

	/** HUD-FIGMA-01: 배터리 소진. 기체 배터리 설정이 FailMission일 때만 실패로 보고한다(기본은 경고만). */
	UFUNCTION()
	void HandleBatteryDepleted();

	UFUNCTION()
	void HandleTrainingLapCompleted(FDroneTrainingLapRecord LapRecord);

	UFUNCTION()
	void HandleReconScanCompleted(AActor* TargetActor);

	UFUNCTION()
	void HandlePayloadResolved(ADroneDroppedPayload* PayloadActor, AActor* HitActor, bool bHitIntendedTarget);

	UFUNCTION()
	void HandleObjectiveTargetDeath(AActor* DeadActor, AController* InstigatorController, AActor* DamageCauser);

	UFUNCTION()
	void HandleJammerDisabled(AActor* JammerActor);

	UFUNCTION()
	void HandleDroneExitedJamming(AActor* DroneActor, AActor* JammerActor);

	UFUNCTION()
	void HandleObjectiveTimeExpired();

	bool FinishMission(EDroneMissionOutcome Outcome);
	void BindOptionalTrainingCourse();
	void BindObjectiveEvents();
	void StartCurrentObjectiveTimeout();
	void ClearRuntimeBindings();
	/** 사망·정찰·투하처럼 기체에 달린 이벤트만 풀거나 묶는다(재출격 때 기체를 바꾸기 위해). */
	void BindActiveDroneEvents();
	void UnbindActiveDroneEvents();
	bool RequestCheckpointRestart();
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

	TWeakObjectPtr<UDroneReconScanComponent> ReconScan;
	TWeakObjectPtr<UDronePayloadDropComponent> PayloadDrop;
	TWeakObjectPtr<UDroneBatteryComponent> ActiveDroneBattery;
	TArray<TWeakObjectPtr<UDroneHealthComponent>> ObjectiveTargetHealthBindings;
	TArray<TWeakObjectPtr<ADroneJammingVolume>> JammingVolumeBindings;
	TSet<FName> CountedObjectiveActorNames;
	FTimerHandle ObjectiveTimeoutHandle;
	/** 다음 재출격 위치. 처음엔 출격한 위치, 체크포인트를 지나면 그 위치. */
	FTransform RestartTransform = FTransform::Identity;

	UPROPERTY(Transient)
	FDroneMissionRuntimeSnapshot Snapshot;

	UPROPERTY(Transient)
	int32 FinishEventCount = 0;

	/** 클리어 시간 계산용 World 시각. Widget이 직접 재지 않는다. */
	double MissionStartWorldSeconds = 0.0;
	double MissionEndWorldSeconds = -1.0;
};
