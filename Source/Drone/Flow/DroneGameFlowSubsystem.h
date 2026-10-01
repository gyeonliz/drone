#pragma once

#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Mission/DroneMissionDefinition.h"
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
 * 실제 OpenLevel·Widget·Pawn 수명은 각 PlayerController가 맡고 이 Subsystem은 상태와 데이터만 검증한다.
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

	/** FLOW-05 선택 카드 생성용. Catalog의 Drone ID를 이름순으로 반환한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	TArray<FName> GetRegisteredDroneIds() const;

	/** 현재 Mission이 허용한 순서를 보존해 선택 카드가 그대로 사용할 Definition을 반환한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	TArray<UDroneDefinition*> GetAvailableDroneDefinitions() const;

	/** 선택 전에는 nullptr이며 FLOW-05 Spawn/Possess가 사용할 단일 Definition이다. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	UDroneDefinition* GetSelectedDroneDefinition() const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	int32 GetRegisteredDroneCount() const { return DroneDefinitions.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	int32 GetRegisteredMissionCount() const { return MissionDefinitions.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Flow")
	FDroneGameFlowSnapshot GetSnapshot() const { return Snapshot; }

	/** 결과→로비에서 선택을 지워도 원래 Training/Story 그룹은 새 Widget이 복원한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow")
	FName GetLastLobbyMissionId() const { return LastLobbyMissionId; }

	UFUNCTION(BlueprintPure, Category="Drone|Flow")
	FText GetLastRejectionReason() const { return LastRejectionReason; }

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool BeginOpeningTrailer();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool EnterLobbyFromOpeningTrailer();

	/** 출격 전 화면을 한 단계 되돌린다. 비행/로딩/결과에는 적용하지 않는다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestBackNavigation();

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

	/** Director가 성공과 Story Fact 변경을 한 Snapshot 전환으로 확정하는 C++ 경계다. */
	bool CompleteMissionWithStoryFacts(
		EDroneMissionOutcome Outcome,
		const TArray<FName>& GrantedFacts,
		const TArray<FName>& RemovedFacts,
		double ElapsedSeconds = -1.0);

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Story")
	bool HasStoryFact(FName FactId) const;

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestRetry();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestReturnToLobby();

	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool ConsumeLobbyReturnRequest();

	/**
	 * TUT-PROGRESS-01: 성공 결과에서 선택 미션의 NextMissionId로 넘어가 그 미션의 브리핑 상태가 된다.
	 * 실패 결과이거나 다음 미션이 없으면 거절한다. 맵 전환(FrontEnd 브리핑)은 Controller가 한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestNextMission();

	/** 선택 미션의 다음 미션. 등록되지 않았거나 없으면 None. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Progression")
	FName GetNextMissionId() const;

	/** 이 미션이 속한 NextMissionId 연결 전체(처음→끝). 연결이 없으면 자기 하나만, 미등록이면 빈 배열. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Progression")
	TArray<FName> GetMissionSequence(FName MissionId) const;

	/** 연결 안 순번(1부터)과 전체 수, 이번 실행에서 완료한 수. 두 개 이상 이어진 과정이 아니면 false. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Progression")
	bool GetMissionSequencePosition(FName MissionId, int32& OutNumber, int32& OutCount, int32& OutCompletedCount) const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Progression")
	bool IsMissionCompleted(FName MissionId) const;

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동화 테스트 전용: 플레이 없이 완료 기록을 넣는다(전체 완료 화면 확인용). */
	void MarkMissionCompletedForTesting(const FName MissionId) { Snapshot.CompletedMissionIds.AddUnique(MissionId); }
#endif

	/** 로비 탭에 보일 순서. NextMissionId로 이어진 미션은 첫 미션 이름 자리에 연결 순서대로, 나머지는 ID 순. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Progression")
	TArray<FName> GetMissionIdsInLobbyOrder(EDroneMissionCategory Category) const;

	/** 결과 화면에서 로비로 가되 이 미션이 있는 탭·선택으로 연다(Figma S49 [미션 진행]). */
	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestReturnToLobbyFocusing(FName FocusMissionId);

	/** 결과 화면에서 시작 메뉴(타이틀)로 간다(Figma S49 [시작 메뉴]). 선택은 지우고 Story Fact·완료 기록은 유지한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flow")
	bool RequestReturnToTitle();

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
	/** 미션 선택과 허용 기체 목록을 채운다(로비 선택과 [다음 수업]이 같이 쓴다). 상태 전환·방송은 하지 않는다. */
	bool ApplyMissionSelection(FName MissionId);

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDroneDefinition>> DroneDefinitions;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDroneMissionDefinition>> MissionDefinitions;

	UPROPERTY(Transient)
	FDroneGameFlowSnapshot Snapshot;

	UPROPERTY(Transient)
	FName LastLobbyMissionId = NAME_None;

	UPROPERTY(Transient)
	FText LastRejectionReason;
};
