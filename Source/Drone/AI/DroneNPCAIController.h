#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "DroneNPCAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UDroneNPCProfileComponent;
class UDroneNPCWeaponComponent;
class UDroneSmartObjectReservationComponent;
class UStateTreeAIComponent;
class ADroneSmartObjectStation;

/** Hostile NPC의 현재 Greybox 대응 상태다. StateTree 전환을 PIE에서 명확히 검증하는 데도 사용한다. */
UENUM(BlueprintType)
enum class EDroneNPCAIResponseState : uint8
{
	Patrol,
	DroneDetected,
	MoveToMGTurret,
	HoldMGTurret,
	UseMGTurret,
	MoveToCover,
	UseCover,
	Search,
	Dead,
	/** 개인화기 사거리·시야를 확보하기 위해 교전 원점의 리시 안에서 이동 중이다. */
	PursueDrone
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneTargetPerceptionChangedSignature,
	AActor*, TargetActor,
	bool, bSuccessfullySensed);

/**
 * Hostile/Friendly NPC가 함께 사용하는 StateTree·Perception·Smart Object Controller다.
 *
 * Hostile NPC만 Drone Prototype을 교전 대상으로 받아들인다. 감지 순간 순찰 Claim을
 * 해제하고 DroneDetected Event를 StateTree에 보낸다. Friendly NPC는 Base Patrol/Ambient
 * Activity만 기본 검색하며 이 감지 Event로 전투 전환하지 않는다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneNPCAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADroneNPCAIController();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	UStateTreeAIComponent* GetStateTreeAIComponent() const { return StateTreeAIComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	UAIPerceptionComponent* GetDronePerceptionComponent() const { return DronePerceptionComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	UAISenseConfig_Sight* GetDroneSightConfig() const { return SightConfig; }

	/** Controller Blueprint의 감지 수치를 Sight Config에 다시 적용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Perception")
	void RefreshDroneSightTuning();

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	UDroneSmartObjectReservationComponent* GetReservationComponent() const { return ReservationComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	AActor* GetDetectedDrone() const { return DetectedDrone.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	bool HasDetectedDrone() const { return DetectedDrone.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	bool UsesRifle() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	bool UsesShotgun() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	bool CanUseMGTurret() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	UDroneNPCWeaponComponent* GetPossessedWeaponComponent() const;

	/** DetectedDrone와 그 현재 위치를 공용 Weapon 계약으로 전달한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Weapon")
	bool CanFirePersonalWeapon() const;

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	bool StartPersonalWeaponFire();

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	void StopPersonalWeaponFire();

	UFUNCTION(BlueprintCallable, Category="Drone|AI|Weapon")
	bool ReloadPersonalWeapon();

	/**
	 * 개인화기 교전 판단을 한 번 갱신한다. 사격 구간이면 정지·재장전/사격하고,
	 * 사거리 밖이면 리시 안에서 추적하며, 리시/경로 제한을 넘으면 순찰로 복귀한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Engagement")
	bool UpdatePersonalWeaponEngagement(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	float GetPersonalWeaponRange() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	float GetPersonalWeaponCombatLeashRadius() const { return PersonalWeaponCombatLeashRadius; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	float GetPersonalWeaponOutOfRangeConfirmationSeconds() const
	{
		return PersonalWeaponOutOfRangeConfirmationSeconds;
	}

	/** Drone 최초 감지 뒤 개인화기 첫 발을 허용하기 전 조준 시간이다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	float GetPersonalWeaponInitialAimDelaySeconds() const { return PersonalWeaponInitialAimDelaySeconds; }

	/** 현재 표적을 처음 감지한 뒤 지난 시간이다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	float GetPersonalWeaponInitialAimElapsedSeconds() const;

	/** 현재 표적에 대한 최초 조준 지연이 끝났는지 반환한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	bool HasCompletedPersonalWeaponInitialAimDelay() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	bool HasPersonalWeaponCombatOrigin() const { return bHasPersonalWeaponCombatOrigin; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	FVector GetPersonalWeaponCombatOrigin() const { return PersonalWeaponCombatOrigin; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	int32 GetPersonalWeaponPursuitStartCount() const { return PersonalWeaponPursuitStartCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	int32 GetPersonalWeaponPursuitMoveRequestCount() const { return PersonalWeaponPursuitMoveRequestCount; }

	/** 현재 개인화기 추적 MoveTo가 실제로 진행 중인지 반환한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	bool IsPersonalWeaponPursuitMoveActive() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Engagement")
	int32 GetPersonalWeaponDisengageCount() const { return PersonalWeaponDisengageCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	bool IsHostileNPC() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	bool IsFriendlyNPC() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	EDroneNPCAIResponseState GetResponseState() const { return ResponseState; }

	/** 현재 대응 상태에 진입한 뒤 지난 시간이다. StateTree 상태 왕복 진단에도 사용한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|State Stability")
	float GetResponseStateElapsedSeconds() const;

	/** 일시 실패 뒤 다른 상태로 넘어가기 전에 현재 행동을 재확인할 최소 시간이다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|State Stability")
	float GetMinimumResponseStateDurationSeconds() const { return MinimumResponseStateDurationSeconds; }

	/** 현재 상태의 최소 유지시간이 지났는지 확인한다. 사망 등 강제 정리는 이 조건을 사용하지 않는다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|State Stability")
	bool HasSatisfiedMinimumResponseStateDuration() const;

	/** 최소 유지시간 중이라면 현재 상태의 사격·점유·이동 조건을 한 번 더 점검한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|State Stability")
	bool MaintainCurrentResponseStateAction();

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	bool HasLastKnownDroneLocation() const { return bHasLastKnownDroneLocation; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	FVector GetLastKnownDroneLocation() const { return LastKnownDroneLocation; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetDroneDetectionCount() const { return DroneDetectionCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetDroneLostCount() const { return DroneLostCount; }

	/** Sight가 잠깐 끊겨 DroneLost 확정을 기다리는 중인지 확인한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	bool IsDroneSightLossPending() const { return PendingLostDrone.IsValid(); }

	/** 짧은 가림·회전에 의한 Sight 깜빡임을 무시하는 기본 유예 시간이다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	float GetDroneSightLossGracePeriod() const { return DroneSightLossGracePeriod; }

	/** AnimBP가 사용하는, Pawn 로컬 기준의 제한·보간된 Drone 시선 회전이다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Gaze")
	FRotator GetSmoothedDroneLookRotation() const { return SmoothedDroneLookRotation; }

	/** 0은 정면 기본 자세, 1은 Drone/마지막 감지 위치 추적 자세다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Gaze")
	float GetDroneLookAlpha() const { return DroneLookAlpha; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Gaze")
	bool HasActiveDroneLookTarget() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Gaze")
	float GetMaxDroneLookYawDegrees() const { return MaxDroneLookYawDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Gaze")
	float GetMaxDroneLookPitchUpDegrees() const { return MaxDroneLookPitchUpDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Gaze")
	float GetMaxDroneLookPitchDownDegrees() const { return MaxDroneLookPitchDownDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetDroneSearchStartCount() const { return DroneSearchStartCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetCompletedDroneSearchCount() const { return CompletedDroneSearchCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetDroneDestroyedResponseCount() const { return DroneDestroyedResponseCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	int32 GetMGTurretClaimCount() const { return MGTurretClaimCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	int32 GetMGTurretArrivalCount() const { return MGTurretArrivalCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	int32 GetMGTurretUseCount() const { return MGTurretUseCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Cover")
	int32 GetCoverClaimCount() const { return CoverClaimCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Cover")
	int32 GetCoverUseCount() const { return CoverUseCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	ADroneSmartObjectStation* GetActiveMGTurretStation() const { return ActiveMGTurretStation.Get(); }

	/** 예약된 MG의 Blueprint 조정 가능 조작점 Transform을 반환한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	bool GetReservedMGTurretOperatorTransform(FTransform& OutOperatorTransform) const;

	/** 소유 NPC 체력이 0이 되었을 때 전투·이동·점유를 정리하고 StateTree를 중단한다. */
	void HandlePossessedPawnDeath();

	/** 현재 감지 중인 Drone이 파괴되면 교전 자원을 정리하고 Patrol Tree를 다시 시작한다. */
	void HandleDetectedDroneDestroyed(AActor* DestroyedDrone);

	/** StateTree의 DroneDetected Task가 호출한다. 중복 진입은 감지 횟수로 세지 않는다. */
	void EnterDroneDetectedResponse();

	/** 마지막 감지 위치로 이동을 요청하고 Search 상태를 시작한다. */
	bool BeginDroneSearch(float AcceptanceRadius);

	/** Search 체류가 끝나 Patrol 상태로 복귀할 준비를 한다. */
	void CompleteDroneSearch();

	/** Profile에 따라 Enemy Patrol 또는 Friendly Base Patrol 검색 Tag를 다시 설정한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	void ConfigureDefaultPatrolActivities();

	/** 현재 예약 Slot의 Yaw에 Pawn과 Controller를 맞춘다. Station 화살표 방향이 실제 도착 방향이 된다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	bool AlignPawnToReservedSlot();

	/** Hostile이며 MG 사용 허용 Profile일 때만 MG Turret Activity 검색으로 전환한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	bool PrepareMGTurretSearch();

	/** 감지 중인 MG 사용 가능 Hostile만 빈 MGTurret 1-Slot을 예약한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	bool ClaimAvailableMGTurret(FTransform& OutSlotTransform);

	/** 예약한 MG 위치에 도착한 뒤 Occupied 전환 직전 상태로 이동한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	bool CompleteMGTurretMove();

	/** Claim을 Occupied로 바꾸고 Station의 Greybox 조준·Trace 사격을 시작한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	bool BeginMGTurretOperation();

	/** StateTree Tick에서 현재 드론을 향한 조준·Cooldown 사격을 갱신한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	bool UpdateMGTurretOperation();

	/** Station 사용자 상태를 정리한다. Slot Free 전환은 Reservation Component가 담당한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	void StopMGTurretOperation();

	/** 이동 실패·감지 실종·StateTree 중단 시 MG Claim과 이동을 함께 정리한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	void AbortMGTurretResponse();

	/** MG를 잡지 못한 감지 중 Hostile이 가장 가까운 빈 Cover 1-Slot을 예약한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Cover")
	bool ClaimAvailableCover(FTransform& OutSlotTransform);

	/** Cover 도착 뒤 Slot을 Occupied로 바꾸고 개인 무기 사격을 시작한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Cover")
	bool CompleteCoverMove();

	/** Cover 점유·감지·개인 무기 상태가 계속 유효한지 확인한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Cover")
	bool UpdateCoverResponse();

	/** Cover 이동·점유를 정리하고 감지 중이면 제자리 개인 무기 대응으로 복귀한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Cover")
	void AbortCoverResponse();

	/** EnemyPatrol만 검색해 직전 완료 지점과 다른 다음 Slot을 예약한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Patrol")
	bool ClaimNextEnemyPatrolSlot(FTransform& OutSlotTransform);

	/** 정상 도착·대기 뒤 현재 Slot을 방문 기록에 남기고 해제한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Patrol")
	void CompleteCurrentPatrolSlot();

	/** FriendlyBasePatrol과 Ambient를 번갈아 검색해 다음 기지 활동 Slot을 예약한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Friendly")
	bool ClaimNextFriendlyActivitySlot(FTransform& OutSlotTransform);

	/** 아군 활동 완료 기록을 남기고 현재 Slot을 해제한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Friendly")
	void CompleteCurrentFriendlyActivitySlot();

	UFUNCTION(BlueprintPure, Category="Drone|AI|Patrol")
	int32 GetCompletedPatrolCycles() const { return CompletedPatrolCycles; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Patrol")
	int32 GetVisitedPatrolSlotCount() const { return VisitedPatrolSlotLocations.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Patrol")
	const TArray<FVector>& GetVisitedPatrolSlotLocations() const { return VisitedPatrolSlotLocations; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Friendly")
	int32 GetCompletedFriendlyRoutineCycles() const { return CompletedFriendlyRoutineCycles; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Friendly")
	int32 GetVisitedFriendlySlotCount() const { return VisitedFriendlySlotLocations.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Friendly")
	bool HasVisitedFriendlyActivity(FGameplayTag ActivityTag) const;

	UPROPERTY(BlueprintAssignable, Category="Drone|AI|Perception")
	FDroneTargetPerceptionChangedSignature OnDronePerceptionChanged;

protected:
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/**
	 * WorldSubsystem의 Smart Object Runtime 초기화가 끝난 뒤 역할별 StateTree를 시작한다.
	 * 레벨 로딩 중 OnPossess에서 바로 조회하면 아직 초기화되지 않은 Runtime을 건드릴 수 있다.
	 */
	void TryStartAssignedStateTree();

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** 실패 Sight 자극을 즉시 상태 전환하지 않고 잠시 보류한다. */
	void QueueDroneLostConfirmation(AActor* Actor);

	/** 같은 Drone이 유예 시간 안에 다시 보이면 보류 중인 Lost를 취소한다. */
	void CancelPendingDroneLost();

	/** 유예 시간 뒤에도 보이지 않을 때만 전투 자원을 정리하고 DroneLost Event를 보낸다. */
	void ConfirmPendingDroneLost();

	/** 현재 감지 Actor 또는 Search 마지막 위치를 향한 AnimBP용 시선 값을 갱신한다. */
	void UpdateDroneGaze(float DeltaSeconds);

	/** 추적 중 몸 Yaw를 실제 수평 이동 벡터에 맞춘다. */
	void UpdatePursuitFacing(float DeltaSeconds);

	/** 화면 재현 뒤 Saved/Logs/Drone.log에서 상태·경로·몸 방향을 함께 비교하는 저빈도 진단이다. */
	void LogMovementDiagnostics(float DeltaSeconds);

	/** 개인화기 교전 중 몸 Yaw를 Drone 쪽으로 돌려 상체 시선 제한 밖 표적도 바라보게 한다. */
	void UpdatePersonalWeaponFacing(float DeltaSeconds);
	float GetPersonalWeaponFacingDeadZoneDegrees() const;
	float GetPersonalWeaponFacingHysteresisDegrees() const;
	float GetPersonalWeaponFacingTurnSpeedDegreesPerSecond() const;

	/** MG 점유자는 포탑 뒤 조작점에 고정하고 포탑 중심을 바라보게 한다. */
	bool AlignPawnToMGTurretOperator();

	/** 남아 있을 수 있는 Gameplay Focus를 정리한다. 머리 시선은 감지/Search 상태에서 별도로 계산한다. */
	void ClearDroneGameplayFocus();

	/** 기존 MG 사망 직후 다른 적이 빈 포탑을 일정 시간 재검색하도록 예약한다. */
	void BeginMGTurretReassignmentRetry();

	/** 단발 StateTree Event가 이동 실패와 겹쳐도 재점유 기회를 잃지 않게 제한 시간 동안 재시도한다. */
	void UpdateMGTurretReassignmentRetry(float DeltaSeconds);

	void ClearMGTurretReassignmentRetry();

	/** 최초 감지 지점을 기준으로 한 개인화기 추적 진행값을 초기화한다. */
	void ResetPersonalWeaponEngagement(bool bClearIgnoredDrone = true);

	/** 새 표적을 감지한 시점을 기록해 첫 사격 전 조준 지연을 시작한다. */
	void BeginPersonalWeaponInitialAimDelay(AActor* TargetActor);

	/** 리시 초과·경로 정체 시 표적을 놓고 재감지 Cooldown 뒤 순찰 Tree로 복귀한다. */
	void DisengagePersonalWeaponTarget();

	/** 방금 포기한 표적을 즉시 다시 감지해 추적 왕복하는 것을 막는다. */
	void UpdatePersonalWeaponDisengageCooldown(float DeltaSeconds);
	void RestartStateTreeAfterPersonalWeaponDisengage();

	/** 상태 진입 시각을 한 곳에서 기록해 모든 태스크가 같은 최소 유지시간을 사용하게 한다. */
	void SetResponseState(EDroneNPCAIResponseState NewState, bool bRestartDuration = false);

	UDroneNPCProfileComponent* GetPossessedProfile() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Components")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Components")
	TObjectPtr<UAIPerceptionComponent> DronePerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Components")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Perception|Sight", meta=(ClampMin="1.0", ForceUnits="cm"))
	float DroneSightRadius = 4000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Perception|Sight", meta=(ClampMin="1.0", ForceUnits="cm"))
	float DroneLoseSightRadius = 4500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Perception|Sight", meta=(ClampMin="0.0", ClampMax="180.0", ForceUnits="deg"))
	float DronePeripheralVisionAngleDegrees = 70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Perception|Sight", meta=(ClampMin="0.0", ForceUnits="s"))
	float DroneSightStimulusMaxAgeSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Perception|Sight")
	bool bDroneSightDetectEnemies = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Perception|Sight")
	bool bDroneSightDetectFriendlies = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Perception|Sight")
	bool bDroneSightDetectNeutrals = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Components")
	TObjectPtr<UDroneSmartObjectReservationComponent> ReservationComponent;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	TWeakObjectPtr<AActor> DetectedDrone;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	EDroneNPCAIResponseState ResponseState = EDroneNPCAIResponseState::Patrol;

	/**
	 * 사격·엄폐·MG 동작이 한 프레임 실패했을 때 StateTree가 즉시 왕복하지 않도록 하는 공통 안정화 시간이다.
	 * 유지 중에는 현재 행동을 계속 점검하고, 시간이 지난 뒤에도 실패일 때만 기존 실패 전환을 허용한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|State Stability",
		meta=(ClampMin="0.0", ClampMax="10.0", UIMin="0.0", UIMax="5.0", ForceUnits="s"))
	float MinimumResponseStateDurationSeconds = 1.0f;

	/** 최초 교전 위치에서 NPC와 표적 모두 벗어나지 않아야 하는 수평 추적 반경이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="100.0", UIMin="500.0", UIMax="10000.0", ForceUnits="cm"))
	float PersonalWeaponCombatLeashRadius = 3000.0f;

	/** 사거리 밖 판정이 이 시간 이상 이어질 때만 추적을 시작한다. 사거리 안 복귀는 즉시 정지·사격한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="1.0", ForceUnits="s"))
	float PersonalWeaponOutOfRangeConfirmationSeconds = 0.20f;

	/** Drone을 처음 감지한 뒤 Rifle/Shotgun 첫 발을 허용하기 전 조준 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="5.0", ForceUnits="s"))
	float PersonalWeaponInitialAimDelaySeconds = 1.0f;

	/** Drone에서 이 사거리 비율만큼 떨어진 지상 정지점을 추적 목표로 계산한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.1", ClampMax="0.95", UIMin="0.25", UIMax="0.9"))
	float PersonalWeaponPursuitRangeRatio = 0.75f;

	/** 계산된 사거리 정지점에 대한 실제 Nav MoveTo 도착 반경이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="10.0", UIMin="25.0", UIMax="300.0", ForceUnits="cm"))
	float PersonalWeaponPursuitDestinationAcceptanceRadius = 75.0f;

	/** 움직이지 못하거나 같은 거리를 유지하면 추적을 포기하기까지 허용할 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.1", UIMin="0.5", UIMax="10.0", ForceUnits="s"))
	float PersonalWeaponPursuitNoProgressTimeoutSeconds = 2.5f;

	/** 이 거리 이상 가까워졌을 때만 실제 추적 진행으로 간주한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="100.0", ForceUnits="cm"))
	float PersonalWeaponPursuitProgressTolerance = 10.0f;

	/** 이동 목표를 다시 계산하는 주기다. 매 Tick MoveTo 재요청으로 인한 경로 흔들림을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.05", UIMin="0.1", UIMax="2.0", ForceUnits="s"))
	float PersonalWeaponPursuitRepathIntervalSeconds = 0.35f;

	/** Nav 경로가 일시 정지됐을 때 같은 MoveTo를 다시 만들기 전에 기다리는 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="3.0", ForceUnits="s"))
	float PersonalWeaponPursuitPausedRecoverySeconds = 0.75f;

	/** 투영된 지상 목표가 이 거리 이상 움직였을 때만 진행 중인 MoveTo 경로를 다시 만든다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="1000.0", ForceUnits="cm"))
	float PersonalWeaponPursuitRepathDistance = 150.0f;

	/** 공중 Drone 위치를 지상 NavMesh 추적점으로 내릴 때 사용하는 검색 범위다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement")
	FVector PersonalWeaponPursuitNavigationProjectionExtent = FVector(500.0f, 500.0f, 2000.0f);

	/** 포기한 표적을 바로 다시 감지해 추적/순찰을 왕복하지 않게 하는 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0", ForceUnits="s"))
	float PersonalWeaponDisengageCooldownSeconds = 3.0f;

	/** 포기한 표적은 기존 교전 원점의 이 비율 안으로 돌아와야 다시 감지한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement",
		meta=(ClampMin="0.1", ClampMax="1.0", UIMin="0.5", UIMax="1.0"))
	float PersonalWeaponDisengageReturnRadiusRatio = 0.85f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	FVector PersonalWeaponCombatOrigin = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	bool bHasPersonalWeaponCombatOrigin = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	float PersonalWeaponPursuitNoProgressSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	float PersonalWeaponPursuitRepathRemainingSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	float PersonalWeaponPursuitPausedElapsedSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	float PersonalWeaponOutOfRangeElapsedSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	TWeakObjectPtr<AActor> PersonalWeaponInitialAimTarget;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	float PersonalWeaponInitialAimStartedWorldTimeSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	float LastPersonalWeaponPursuitDistance = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	FVector PersonalWeaponPursuitNavigationDestination = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	bool bHasPersonalWeaponPursuitNavigationDestination = false;

	/** 현재 투영 목표의 도달 가능한 끝점까지 성공적으로 이동해 같은 경로 재요청을 막는 상태다. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	bool bPersonalWeaponPursuitMoveSettled = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	float PersonalWeaponDisengageCooldownRemainingSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	TWeakObjectPtr<AActor> IgnoredDisengagedDrone;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	FVector IgnoredDisengageCombatOrigin = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	bool bHasIgnoredDisengageCombatOrigin = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	int32 PersonalWeaponPursuitStartCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	int32 PersonalWeaponPursuitMoveRequestCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Engagement")
	int32 PersonalWeaponDisengageCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|State Stability")
	float ResponseStateEnteredWorldTimeSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	FVector LastKnownDroneLocation = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	bool bHasLastKnownDroneLocation = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 DroneDetectionCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 DroneLostCount = 0;

	/**
	 * AI가 MG·Cover로 이동하며 잠깐 등을 돌리거나 장애물에 가려져도 상태가 왕복하지 않게 한다.
	 * 0이면 기존처럼 Sight 실패를 즉시 확정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Perception", meta=(ClampMin="0.0", ForceUnits="s"))
	float DroneSightLossGracePeriod = 1.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	TWeakObjectPtr<AActor> PendingLostDrone;

	FTimerHandle DroneLostGraceTimerHandle;

	/** 고개만 비틀 수 있는 Greybox 좌우 제한각. 제한 밖 느린 몸 회전은 후속 카드다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze", meta=(ClampMin="0.0", ClampMax="180.0", ForceUnits="deg"))
	float MaxDroneLookYawDegrees = 65.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze", meta=(ClampMin="0.0", ClampMax="90.0", ForceUnits="deg"))
	float MaxDroneLookPitchUpDegrees = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze", meta=(ClampMin="0.0", ClampMax="90.0", ForceUnits="deg"))
	float MaxDroneLookPitchDownDegrees = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze", meta=(ClampMin="0.0"))
	float DroneLookTrackingInterpolationSpeed = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze", meta=(ClampMin="0.0"))
	float DroneLookReturnInterpolationSpeed = 3.5f;

	/** MG가 아닌 개인화기 교전 상태에서 병사 몸을 Drone 방향으로 돌린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze")
	bool bFaceDroneDuringPersonalWeaponResponse = true;

	/** 이동 중 몸을 진행 방향으로 돌리는 최대 속도다. Controller BP에서 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze",
		meta=(ClampMin="1.0", UIMin="90.0", UIMax="1080.0", ForceUnits="deg/s"))
	float PursuitFacingTurnSpeedDegreesPerSecond = 720.0f;

	/** 이 수평 속도보다 느린 잔류 움직임은 몸 방향 기준으로 사용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Gaze",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="200.0", ForceUnits="cm/s"))
	float PursuitFacingMinimumSpeed = 20.0f;

	/**
	 * 개인화기 추적 중 Nav 요청 속도에 CharacterMovement 가속을 다시 적용할지 여부다.
	 * false면 짧은 경로점을 지나쳐 공전하지 않으며 순찰 등 다른 상태에는 영향을 주지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Engagement")
	bool bUseAccelerationForPersonalWeaponPursuitMoves = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Gaze")
	FRotator SmoothedDroneLookRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Gaze")
	float DroneLookAlpha = 0.0f;

	/** 시작각과 정지각을 분리해 경계에서 몸/고개가 좌우 왕복하지 않게 하는 런타임 상태다. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Gaze")
	bool bPersonalWeaponFacingTurnActive = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 DroneSearchStartCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 CompletedDroneSearchCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 DroneDestroyedResponseCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG")
	int32 MGTurretClaimCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG")
	int32 MGTurretArrivalCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG")
	int32 MGTurretUseCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG")
	TWeakObjectPtr<ADroneSmartObjectStation> ActiveMGTurretStation;

	/** MG 운용자 사망 뒤 다른 적이 빈 Slot을 다시 확인하는 간격이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|MG|Reassignment", meta=(ClampMin="0.1", ForceUnits="s"))
	float MGTurretReassignmentRetryIntervalSeconds = 0.75f;

	/** 이동 실패·일시적인 StateTree 전환 경합을 허용하되 무한 재시도를 막는 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|MG|Reassignment", meta=(ClampMin="0.1", ForceUnits="s"))
	float MGTurretReassignmentRetryWindowSeconds = 15.0f;

	UPROPERTY(Transient)
	bool bMGTurretReassignmentRetryPending = false;

	UPROPERTY(Transient)
	float MGTurretReassignmentRetryElapsedSeconds = 0.0f;

	UPROPERTY(Transient)
	float MGTurretReassignmentRetryRemainingSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Cover")
	int32 CoverClaimCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Cover")
	int32 CoverUseCount = 0;

	/** 직전 지점 바로 재선택을 막는 Greybox 기준값. 최종 맵 규모에 맞춰 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Patrol", meta=(ClampMin="0.0", ForceUnits="cm"))
	float PatrolRepeatAvoidanceRadius = 250.0f;

	/** Greybox 수동 재현 동안 [NPC-MOVE]/[NPC-STATE] 로그를 남긴다. 원인 확정 뒤 BP에서 끌 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Diagnostics")
	bool bEnableMovementDiagnostics = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Diagnostics",
		meta=(ClampMin="0.1", UIMin="0.1", UIMax="2.0", ForceUnits="s"))
	float MovementDiagnosticLogIntervalSeconds = 0.5f;

	UPROPERTY(Transient)
	float MovementDiagnosticLogRemainingSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Patrol")
	int32 CompletedPatrolCycles = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Patrol")
	TArray<FVector> VisitedPatrolSlotLocations;

	UPROPERTY(Transient)
	FVector LastCompletedPatrolSlotLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasCompletedPatrolSlot = false;

	/** 아군 루틴은 전투 순찰과 별도로 기록해 역할별 자동화 검증에서 구분한다. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Friendly")
	int32 CompletedFriendlyRoutineCycles = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Friendly")
	TArray<FVector> VisitedFriendlySlotLocations;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Friendly")
	FGameplayTagContainer VisitedFriendlyActivities;

	UPROPERTY(Transient)
	FGameplayTag CurrentFriendlyActivity;

	UPROPERTY(Transient)
	FVector LastCompletedFriendlySlotLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasCompletedFriendlySlot = false;

	/** false면 Base Patrol, true면 Ambient를 먼저 시도한다. 완료할 때마다 전환한다. */
	UPROPERTY(Transient)
	bool bPreferAmbientActivity = false;
};
