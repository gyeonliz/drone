#pragma once

#include "CoreMinimal.h"
#include "Flow/DroneGameFlowTypes.h"
#include "Prototype/DroneFlightControlTypes.h"
#include "Prototype/DronePrototypePlayerController.h"
#include "DroneMissionPlayerController.generated.h"

class ADroneMissionDirector;
class ADronePrototypePawn;
class UDroneGameFlowSubsystem;
class UDroneMissionObjectiveWidget;
class UDroneMissionResultWidget;
class UDroneSelectionWidget;

/**
 * Mission Map의 Drone 선택 UI와 선택 완료 후 단 한 대의 Spawn/Possess를 소유한다.
 * 비행 HUD 수명주기는 기존 Prototype Controller를 재사용하고 선택 중에는 숨긴다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneMissionPlayerController : public ADronePrototypePlayerController
{
	GENERATED_BODY()

public:
	ADroneMissionPlayerController();

	UFUNCTION(BlueprintPure, Category="Drone|Mission Entry")
	UDroneSelectionWidget* GetDroneSelectionWidget() const { return DroneSelectionWidget; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Entry")
	TSubclassOf<UDroneSelectionWidget> GetDroneSelectionWidgetClass() const { return DroneSelectionWidgetClass; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Entry")
	ADronePrototypePawn* GetSpawnedDrone() const { return SpawnedDrone; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Entry")
	int32 GetSuccessfulDroneSpawnCount() const { return SuccessfulDroneSpawnCount; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Runtime")
	ADroneMissionDirector* GetMissionDirector() const { return MissionDirector; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Runtime")
	TSubclassOf<ADroneMissionDirector> GetMissionDirectorClass() const { return MissionDirectorClass; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Runtime")
	UDroneMissionObjectiveWidget* GetMissionObjectiveWidget() const { return MissionObjectiveWidget; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission Runtime")
	UDroneMissionResultWidget* GetMissionResultWidget() const { return MissionResultWidget; }

	/**
	 * 선택된 Definition의 Pawn을 적용·빙의한 뒤에만 Flow를 InMission으로 전환한다.
	 * 중간 단계가 실패하면 새 Pawn을 제거하고 DroneSelect 상태를 유지한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Entry")
	bool StartSelectedDrone(EDroneControlMode ControlMode, EDroneHandlingPreset HandlingPreset);

	/** 기체를 아직 생성하지 않은 선택 화면에서만 FrontEnd의 미션 설명으로 돌아간다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Entry")
	bool BackToMissionBriefing();

	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool RetrySelectedMission();

	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool ReturnToFrontEndLobby();

	/** 성공 결과에서 다음 수업(NextMissionId)의 브리핑으로 간다. 브리핑은 FrontEnd 맵에서 보여 준다(TUT-PROGRESS-01). */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool StartNextMission();

	/** 로비의 미션(스토리) 탭 첫 미션으로 간다. 튜토리얼 전체 완료 화면의 [미션 진행]. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool ContinueToMissionLobby();

	/** 시작 메뉴(타이틀)로 간다. 튜토리얼 전체 완료 화면의 [시작 메뉴]. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission Result")
	bool ReturnToTitleMenu();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CreateDroneSelectionWidget(UDroneGameFlowSubsystem* Flow);
	void CreateMissionObjectiveWidget(ADroneMissionDirector* InMissionDirector);
	void CreateMissionResultWidget(EDroneMissionOutcome Outcome);
	FTransform ResolveDroneSpawnTransform() const;

	UFUNCTION()
	void HandleMissionFinished(EDroneMissionOutcome Outcome);

	/** MISSION-CHECKPOINT-01: Director가 실패를 재출격으로 돌렸다. 사망 이벤트 처리 도중이라 다음 프레임에 기체를 바꾼다. */
	UFUNCTION()
	void HandleMissionRestartRequested(const FTransform& RestartTransform);
	void PerformCheckpointRestart();
	FTransform PendingRestartTransform = FTransform::Identity;
	bool bCheckpointRestartPending = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Mission Entry", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UDroneSelectionWidget> DroneSelectionWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Mission Runtime", meta=(AllowPrivateAccess="true"))
	TSubclassOf<ADroneMissionDirector> MissionDirectorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Mission Runtime", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UDroneMissionObjectiveWidget> MissionObjectiveWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Mission Result", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UDroneMissionResultWidget> MissionResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDroneSelectionWidget> DroneSelectionWidget;

	UPROPERTY(Transient)
	TObjectPtr<ADronePrototypePawn> SpawnedDrone;

	UPROPERTY(Transient)
	TObjectPtr<ADroneMissionDirector> MissionDirector;

	UPROPERTY(Transient)
	TObjectPtr<UDroneMissionObjectiveWidget> MissionObjectiveWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDroneMissionResultWidget> MissionResultWidget;

	UPROPERTY(Transient)
	int32 SuccessfulDroneSpawnCount = 0;
};
