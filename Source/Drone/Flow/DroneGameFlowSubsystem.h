#pragma once

#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DroneGameFlowSubsystem.generated.h"

class UDroneDefinition;
class UDroneMissionDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneGameFlowStateChangedSignature,
	EDroneGameFlowState, PreviousState,
	EDroneGameFlowState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneGameFlowSnapshotChangedSignature,
	const FDroneGameFlowSnapshot&, Snapshot);

/**
 * Map 전환 사이에 Front-end 선택과 Mission 진입 상태를 한 곳에서 보존한다.
 * FLOW-01에서는 실제 OpenLevel/Widget/Pawn Spawn을 수행하지 않고 허용된 요청만 검증한다.
 */
UCLASS()
class DRONE_API UDroneGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 첫 Vertical Slice의 저장 Data Asset을 GameInstance Catalog에 한 번 등록한다.
	 * 여러 화면이 다시 호출해도 같은 Asset이면 중복 등록으로 취급하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|Flow|Data")
	bool EnsureDefaultCatalog();

	/** Drone을 먼저 등록한다. 중복 ID와 잘못된 Definition은 기존 Catalog를 바꾸지 않고 거부한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flow|Data")
	bool RegisterDroneDefinition(UDroneDefinition* Definition);

	/** 모든 AllowedDroneIds가 이미 등록된 Mission만 Catalog에 넣는다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flow|Data")
	bool RegisterMissionDefinition(UDroneMissionDefinition* Definition);

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	UDroneDefinition* FindDroneDefinition(FName DroneId) const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	UDroneMissionDefinition* FindMissionDefinition(FName MissionId) const;

	/** 로비 목록이 Map 순서에 의존하지 않도록 ID를 이름순으로 반환한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	TArray<FName> GetRegisteredMissionIds() const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	int32 GetRegisteredDroneCount() const { return DroneDefinitions.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	int32 GetRegisteredMissionCount() const { return MissionDefinitions.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Flow")
	FDroneGameFlowSnapshot GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category="Drone|Flow")
	FText GetLastRejectionReason() const { return LastRejectionReason; }

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool BeginOpeningTrailer();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool EnterLobbyFromOpeningTrailer();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool SelectMission(FName MissionId);

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool ConfirmMissionSelection();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool NotifyMissionTrailerFinished();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool NotifyMissionMapReady();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool SelectDrone(FName DroneId);

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestMissionStart();

	/** Mission Director가 true를 한 번만 받도록 시작 요청을 소비한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool ConsumeMissionStartRequest();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool CompleteMission(EDroneMissionOutcome Outcome);

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestRetry();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestReturnToLobby();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool ConsumeLobbyReturnRequest();

	UPROPERTY(BlueprintAssignable, Category="Drone|Flow")
	FDroneGameFlowStateChangedSignature OnFlowStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Drone|Flow")
	FDroneGameFlowSnapshotChangedSignature OnFlowSnapshotChanged;

private:
	bool ChangeState(EDroneGameFlowState ExpectedState, EDroneGameFlowState NewState);
	bool Reject(const FText& Reason);
	void ClearRejection();
	void BroadcastSnapshot();
	void ResetRuntimeSelection(bool bClearMission);

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDroneDefinition>> DroneDefinitions;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDroneMissionDefinition>> MissionDefinitions;

	UPROPERTY(Transient)
	FDroneGameFlowSnapshot Snapshot;

	UPROPERTY(Transient)
	FText LastRejectionReason;
};
