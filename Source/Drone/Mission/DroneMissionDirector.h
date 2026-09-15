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

	UFUNCTION(BlueprintPure, Category="Drone|Mission")
	bool IsMissionActive() const { return Snapshot.State == EDroneMissionRuntimeState::Active; }

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
	TArray<TWeakObjectPtr<UDroneHealthComponent>> ObjectiveTargetHealthBindings;
	TArray<TWeakObjectPtr<ADroneJammingVolume>> JammingVolumeBindings;
	TSet<FName> CountedObjectiveActorNames;
	FTimerHandle ObjectiveTimeoutHandle;

	UPROPERTY(Transient)
	FDroneMissionRuntimeSnapshot Snapshot;

	UPROPERTY(Transient)
	int32 FinishEventCount = 0;
};
