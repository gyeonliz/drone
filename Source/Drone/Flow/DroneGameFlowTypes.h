#pragma once

#include "CoreMinimal.h"
#include "DroneGameFlowTypes.generated.h"

/**
 * 게임 실행부터 Mission 결과까지의 단일 Front-end 상태다.
 * Widget이나 Level Blueprint가 별도 Boolean으로 같은 상태를 중복 소유하지 않는다.
 */
UENUM(BlueprintType)
enum class EDroneGameFlowState : uint8
{
	Boot UMETA(DisplayName="Boot"),
	OpeningTrailer UMETA(DisplayName="Opening Trailer"),
	LobbyMissionSelect UMETA(DisplayName="Lobby Mission Select"),
	MissionTrailer UMETA(DisplayName="Mission Trailer"),
	LoadingMissionMap UMETA(DisplayName="Loading Mission Map"),
	DroneSelect UMETA(DisplayName="Drone Select"),
	InMission UMETA(DisplayName="In Mission"),
	MissionResult UMETA(DisplayName="Mission Result")
};

/** Mission 결과 화면과 재도전/로비 분기가 공유하는 최소 결과다. */
UENUM(BlueprintType)
enum class EDroneMissionOutcome : uint8
{
	None UMETA(DisplayName="None"),
	Success UMETA(DisplayName="Success"),
	Failure UMETA(DisplayName="Failure")
};

/**
 * UI가 Tick이나 Actor 검색 없이 한 번에 읽는 현재 흐름 사본이다.
 * 실제 Definition Asset은 Subsystem 조회 함수로 가져오고 여기에는 안정적인 ID만 보존한다.
 */
USTRUCT(BlueprintType)
struct FDroneGameFlowSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Drone Flow")
	EDroneGameFlowState State = EDroneGameFlowState::Boot;

	UPROPERTY(BlueprintReadOnly, Category="Drone Flow")
	FName SelectedMissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="Drone Flow")
	TArray<FName> AvailableDroneIds;

	UPROPERTY(BlueprintReadOnly, Category="Drone Flow")
	FName SelectedDroneId = NAME_None;

	/** Mission Director가 한 번 소비할 수 있는 시작 요청이다. */
	UPROPERTY(BlueprintReadOnly, Category="Drone Flow")
	bool bMissionStartRequested = false;

	UPROPERTY(BlueprintReadOnly, Category="Drone Flow")
	EDroneMissionOutcome LastMissionOutcome = EDroneMissionOutcome::None;

	/** Front-end 계층이 한 번 소비할 수 있는 로비 복귀 요청이다. */
	UPROPERTY(BlueprintReadOnly, Category="Drone Flow")
	bool bLobbyReturnRequested = false;
};

