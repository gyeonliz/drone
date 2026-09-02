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

/** Hostile NPC의 현재 Greybox 대응 상태다. StateTree 전환을 PIE에서 명확히 검증하는 데도 사용한다. */
UENUM(BlueprintType)
enum class EDroneNPCAIResponseState : uint8
{
	Patrol,
	DroneDetected,
	Search
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

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	UStateTreeAIComponent* GetStateTreeAIComponent() const { return StateTreeAIComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	UAIPerceptionComponent* GetDronePerceptionComponent() const { return DronePerceptionComponent; }

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

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	bool IsHostileNPC() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI")
	bool IsFriendlyNPC() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	EDroneNPCAIResponseState GetResponseState() const { return ResponseState; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	bool HasLastKnownDroneLocation() const { return bHasLastKnownDroneLocation; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	FVector GetLastKnownDroneLocation() const { return LastKnownDroneLocation; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetDroneDetectionCount() const { return DroneDetectionCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetDroneLostCount() const { return DroneLostCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetDroneSearchStartCount() const { return DroneSearchStartCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|Perception")
	int32 GetCompletedDroneSearchCount() const { return CompletedDroneSearchCount; }

	/** StateTree의 DroneDetected Task가 호출한다. 중복 진입은 감지 횟수로 세지 않는다. */
	void EnterDroneDetectedResponse();

	/** 마지막 감지 위치로 이동을 요청하고 Search 상태를 시작한다. */
	bool BeginDroneSearch(float AcceptanceRadius);

	/** Search 체류가 끝나 Patrol 상태로 복귀할 준비를 한다. */
	void CompleteDroneSearch();

	/** Profile에 따라 Enemy Patrol 또는 Friendly Base Patrol 검색 Tag를 다시 설정한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	void ConfigureDefaultPatrolActivities();

	/** Hostile이며 MG 사용 허용 Profile일 때만 MG Turret Activity 검색으로 전환한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	bool PrepareMGTurretSearch();

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

	UDroneNPCProfileComponent* GetPossessedProfile() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Components")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Components")
	TObjectPtr<UAIPerceptionComponent> DronePerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Components")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Components")
	TObjectPtr<UDroneSmartObjectReservationComponent> ReservationComponent;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	TWeakObjectPtr<AActor> DetectedDrone;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	EDroneNPCAIResponseState ResponseState = EDroneNPCAIResponseState::Patrol;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	FVector LastKnownDroneLocation = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	bool bHasLastKnownDroneLocation = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 DroneDetectionCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 DroneLostCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 DroneSearchStartCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|Perception")
	int32 CompletedDroneSearchCount = 0;

	/** 직전 지점 바로 재선택을 막는 Greybox 기준값. 최종 맵 규모에 맞춰 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|Patrol", meta=(ClampMin="0.0", ForceUnits="cm"))
	float PatrolRepeatAvoidanceRadius = 250.0f;

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
