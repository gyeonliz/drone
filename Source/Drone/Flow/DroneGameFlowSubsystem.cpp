#include "Flow/DroneGameFlowSubsystem.h"

#include "Mission/DroneDefinition.h"
#include "Mission/DroneMissionDefinition.h"

#define LOCTEXT_NAMESPACE "DroneGameFlow"

namespace DroneGameFlow
{
const TCHAR* DefaultDronePath =
	TEXT("/Game/Drone/Data/Drones/DA_Drone_Scout_Greybox.DA_Drone_Scout_Greybox");
const TCHAR* DefaultMissionPath =
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Training.DA_Mission_Tutorial_Training");
}

void UDroneGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DroneDefinitions.Reset();
	MissionDefinitions.Reset();
	Snapshot = FDroneGameFlowSnapshot();
	ClearRejection();

	// FLOW-02의 Front-end 화면이 열리기 전에 첫 Vertical Slice Catalog를 준비한다.
	// 실제 프로젝트가 여러 Mission을 갖게 되면 Asset Manager 검색으로 교체할 경계다.
	EnsureDefaultCatalog();
}

void UDroneGameFlowSubsystem::Deinitialize()
{
	DroneDefinitions.Reset();
	MissionDefinitions.Reset();
	Snapshot = FDroneGameFlowSnapshot();
	ClearRejection();
	Super::Deinitialize();
}

bool UDroneGameFlowSubsystem::EnsureDefaultCatalog()
{
	UDroneDefinition* DefaultDrone = LoadObject<UDroneDefinition>(nullptr, DroneGameFlow::DefaultDronePath);
	if (!DefaultDrone)
	{
		return Reject(LOCTEXT("DefaultDroneMissing", "기본 Drone Definition을 불러오지 못했습니다."));
	}

	if (UDroneDefinition* RegisteredDrone = FindDroneDefinition(DefaultDrone->DroneId))
	{
		if (RegisteredDrone != DefaultDrone)
		{
			return Reject(LOCTEXT("DefaultDroneConflict", "기본 Drone ID가 다른 Asset으로 이미 등록되어 있습니다."));
		}
	}
	else if (!RegisterDroneDefinition(DefaultDrone))
	{
		return false;
	}

	UDroneMissionDefinition* DefaultMission = LoadObject<UDroneMissionDefinition>(
		nullptr,
		DroneGameFlow::DefaultMissionPath);
	if (!DefaultMission)
	{
		return Reject(LOCTEXT("DefaultMissionMissing", "기본 Mission Definition을 불러오지 못했습니다."));
	}

	if (UDroneMissionDefinition* RegisteredMission = FindMissionDefinition(DefaultMission->MissionId))
	{
		if (RegisteredMission != DefaultMission)
		{
			return Reject(LOCTEXT("DefaultMissionConflict", "기본 Mission ID가 다른 Asset으로 이미 등록되어 있습니다."));
		}
	}
	else if (!RegisterMissionDefinition(DefaultMission))
	{
		return false;
	}

	ClearRejection();
	return true;
}

bool UDroneGameFlowSubsystem::RegisterDroneDefinition(UDroneDefinition* Definition)
{
	FString ValidationError;
	if (!IsValid(Definition) || !Definition->ValidateDefinition(ValidationError))
	{
		return Reject(FText::FromString(ValidationError.IsEmpty() ? TEXT("Drone Definition이 없습니다.") : ValidationError));
	}
	if (DroneDefinitions.Contains(Definition->DroneId))
	{
		return Reject(FText::Format(
			LOCTEXT("DuplicateDrone", "Drone ID '{0}'가 이미 등록되어 있습니다."),
			FText::FromName(Definition->DroneId)));
	}

	DroneDefinitions.Add(Definition->DroneId, Definition);
	ClearRejection();
	return true;
}

bool UDroneGameFlowSubsystem::RegisterMissionDefinition(UDroneMissionDefinition* Definition)
{
	FString ValidationError;
	if (!IsValid(Definition) || !Definition->ValidateDefinition(ValidationError))
	{
		return Reject(FText::FromString(ValidationError.IsEmpty() ? TEXT("Mission Definition이 없습니다.") : ValidationError));
	}
	if (MissionDefinitions.Contains(Definition->MissionId))
	{
		return Reject(FText::Format(
			LOCTEXT("DuplicateMission", "Mission ID '{0}'가 이미 등록되어 있습니다."),
			FText::FromName(Definition->MissionId)));
	}

	for (const FName DroneId : Definition->AllowedDroneIds)
	{
		if (!DroneDefinitions.Contains(DroneId))
		{
			return Reject(FText::Format(
				LOCTEXT("UnknownAllowedDrone", "Mission '{0}'가 등록되지 않은 Drone '{1}'를 참조합니다."),
				FText::FromName(Definition->MissionId),
				FText::FromName(DroneId)));
		}
	}

	MissionDefinitions.Add(Definition->MissionId, Definition);
	ClearRejection();
	return true;
}

UDroneDefinition* UDroneGameFlowSubsystem::FindDroneDefinition(const FName DroneId) const
{
	const TObjectPtr<UDroneDefinition>* Found = DroneDefinitions.Find(DroneId);
	return Found ? Found->Get() : nullptr;
}

UDroneMissionDefinition* UDroneGameFlowSubsystem::FindMissionDefinition(const FName MissionId) const
{
	const TObjectPtr<UDroneMissionDefinition>* Found = MissionDefinitions.Find(MissionId);
	return Found ? Found->Get() : nullptr;
}

TArray<FName> UDroneGameFlowSubsystem::GetRegisteredMissionIds() const
{
	TArray<FName> MissionIds;
	MissionDefinitions.GenerateKeyArray(MissionIds);
	MissionIds.Sort([](const FName Left, const FName Right)
	{
		return Left.Compare(Right) < 0;
	});
	return MissionIds;
}

bool UDroneGameFlowSubsystem::BeginOpeningTrailer()
{
	return ChangeState(EDroneGameFlowState::Boot, EDroneGameFlowState::OpeningTrailer);
}

bool UDroneGameFlowSubsystem::EnterLobbyFromOpeningTrailer()
{
	if (Snapshot.State != EDroneGameFlowState::OpeningTrailer)
	{
		return Reject(LOCTEXT("EnterLobbyWrongState", "Opening Trailer 상태에서만 로비로 이동할 수 있습니다."));
	}
	ResetRuntimeSelection(true);
	return ChangeState(EDroneGameFlowState::OpeningTrailer, EDroneGameFlowState::LobbyMissionSelect);
}

bool UDroneGameFlowSubsystem::SelectMission(const FName MissionId)
{
	if (Snapshot.State != EDroneGameFlowState::LobbyMissionSelect)
	{
		return Reject(LOCTEXT("SelectMissionWrongState", "로비의 Mission 선택 상태에서만 Mission을 선택할 수 있습니다."));
	}

	UDroneMissionDefinition* Mission = FindMissionDefinition(MissionId);
	if (!Mission)
	{
		return Reject(FText::Format(LOCTEXT("MissionNotFound", "Mission ID '{0}'를 찾을 수 없습니다."), FText::FromName(MissionId)));
	}

	Snapshot.SelectedMissionId = MissionId;
	Snapshot.AvailableDroneIds = Mission->AllowedDroneIds;
	Snapshot.SelectedDroneId = NAME_None;
	Snapshot.bMissionStartRequested = false;
	Snapshot.LastMissionOutcome = EDroneMissionOutcome::None;
	Snapshot.bLobbyReturnRequested = false;
	ClearRejection();
	BroadcastSnapshot();
	return true;
}

bool UDroneGameFlowSubsystem::ConfirmMissionSelection()
{
	if (Snapshot.SelectedMissionId.IsNone() || !FindMissionDefinition(Snapshot.SelectedMissionId))
	{
		return Reject(LOCTEXT("MissionSelectionMissing", "유효한 Mission을 먼저 선택해야 합니다."));
	}
	return ChangeState(EDroneGameFlowState::LobbyMissionSelect, EDroneGameFlowState::MissionTrailer);
}

bool UDroneGameFlowSubsystem::NotifyMissionTrailerFinished()
{
	return ChangeState(EDroneGameFlowState::MissionTrailer, EDroneGameFlowState::LoadingMissionMap);
}

bool UDroneGameFlowSubsystem::NotifyMissionMapReady()
{
	return ChangeState(EDroneGameFlowState::LoadingMissionMap, EDroneGameFlowState::DroneSelect);
}

bool UDroneGameFlowSubsystem::SelectDrone(const FName DroneId)
{
	if (Snapshot.State != EDroneGameFlowState::DroneSelect)
	{
		return Reject(LOCTEXT("SelectDroneWrongState", "Mission Map의 Drone 선택 상태에서만 Drone을 선택할 수 있습니다."));
	}
	if (!Snapshot.AvailableDroneIds.Contains(DroneId))
	{
		return Reject(FText::Format(LOCTEXT("DroneNotAllowed", "Drone ID '{0}'는 현재 Mission에서 허용되지 않습니다."), FText::FromName(DroneId)));
	}

	UDroneDefinition* Drone = FindDroneDefinition(DroneId);
	if (!Drone || Drone->bLocked)
	{
		return Reject(FText::Format(LOCTEXT("DroneUnavailable", "Drone ID '{0}'를 선택할 수 없습니다."), FText::FromName(DroneId)));
	}

	Snapshot.SelectedDroneId = DroneId;
	ClearRejection();
	BroadcastSnapshot();
	return true;
}

bool UDroneGameFlowSubsystem::RequestMissionStart()
{
	if (Snapshot.State != EDroneGameFlowState::DroneSelect
		|| Snapshot.SelectedDroneId.IsNone()
		|| !FindDroneDefinition(Snapshot.SelectedDroneId))
	{
		return Reject(LOCTEXT("MissionStartInvalid", "허용된 Drone을 확정한 뒤에만 Mission을 시작할 수 있습니다."));
	}

	Snapshot.bMissionStartRequested = true;
	return ChangeState(EDroneGameFlowState::DroneSelect, EDroneGameFlowState::InMission);
}

bool UDroneGameFlowSubsystem::ConsumeMissionStartRequest()
{
	if (Snapshot.State != EDroneGameFlowState::InMission || !Snapshot.bMissionStartRequested)
	{
		return Reject(LOCTEXT("NoMissionStartRequest", "소비할 Mission 시작 요청이 없습니다."));
	}
	Snapshot.bMissionStartRequested = false;
	ClearRejection();
	BroadcastSnapshot();
	return true;
}

bool UDroneGameFlowSubsystem::CompleteMission(const EDroneMissionOutcome Outcome)
{
	if (Snapshot.State != EDroneGameFlowState::InMission
		|| Outcome == EDroneMissionOutcome::None)
	{
		return Reject(LOCTEXT("MissionCompleteInvalid", "진행 중 Mission에는 Success 또는 Failure 결과가 필요합니다."));
	}

	Snapshot.bMissionStartRequested = false;
	Snapshot.LastMissionOutcome = Outcome;
	return ChangeState(EDroneGameFlowState::InMission, EDroneGameFlowState::MissionResult);
}

bool UDroneGameFlowSubsystem::RequestRetry()
{
	if (Snapshot.State != EDroneGameFlowState::MissionResult
		|| Snapshot.SelectedMissionId.IsNone()
		|| !FindMissionDefinition(Snapshot.SelectedMissionId))
	{
		return Reject(LOCTEXT("RetryInvalid", "결과 상태의 유효한 Mission만 재도전할 수 있습니다."));
	}

	Snapshot.SelectedDroneId = NAME_None;
	Snapshot.bMissionStartRequested = false;
	Snapshot.LastMissionOutcome = EDroneMissionOutcome::None;
	Snapshot.bLobbyReturnRequested = false;
	return ChangeState(EDroneGameFlowState::MissionResult, EDroneGameFlowState::LoadingMissionMap);
}

bool UDroneGameFlowSubsystem::RequestReturnToLobby()
{
	if (Snapshot.State != EDroneGameFlowState::MissionResult)
	{
		return Reject(LOCTEXT("ReturnLobbyInvalid", "Mission 결과 상태에서만 로비 복귀를 요청할 수 있습니다."));
	}

	ResetRuntimeSelection(true);
	Snapshot.bLobbyReturnRequested = true;
	return ChangeState(EDroneGameFlowState::MissionResult, EDroneGameFlowState::LobbyMissionSelect);
}

bool UDroneGameFlowSubsystem::ConsumeLobbyReturnRequest()
{
	if (Snapshot.State != EDroneGameFlowState::LobbyMissionSelect || !Snapshot.bLobbyReturnRequested)
	{
		return Reject(LOCTEXT("NoLobbyReturnRequest", "소비할 로비 복귀 요청이 없습니다."));
	}
	Snapshot.bLobbyReturnRequested = false;
	ClearRejection();
	BroadcastSnapshot();
	return true;
}

bool UDroneGameFlowSubsystem::ChangeState(
	const EDroneGameFlowState ExpectedState,
	const EDroneGameFlowState NewState)
{
	if (Snapshot.State != ExpectedState)
	{
		return Reject(FText::Format(
			LOCTEXT("UnexpectedState", "현재 상태 {0}에서는 요청한 전환을 실행할 수 없습니다."),
			FText::AsNumber(static_cast<uint8>(Snapshot.State))));
	}

	const EDroneGameFlowState PreviousState = Snapshot.State;
	Snapshot.State = NewState;
	ClearRejection();
	OnFlowStateChanged.Broadcast(PreviousState, NewState);
	BroadcastSnapshot();
	return true;
}

bool UDroneGameFlowSubsystem::Reject(const FText& Reason)
{
	LastRejectionReason = Reason;
	return false;
}

void UDroneGameFlowSubsystem::ClearRejection()
{
	LastRejectionReason = FText::GetEmpty();
}

void UDroneGameFlowSubsystem::BroadcastSnapshot()
{
	OnFlowSnapshotChanged.Broadcast(Snapshot);
}

void UDroneGameFlowSubsystem::ResetRuntimeSelection(const bool bClearMission)
{
	if (bClearMission)
	{
		Snapshot.SelectedMissionId = NAME_None;
		Snapshot.AvailableDroneIds.Reset();
	}
	Snapshot.SelectedDroneId = NAME_None;
	Snapshot.bMissionStartRequested = false;
	Snapshot.LastMissionOutcome = EDroneMissionOutcome::None;
	Snapshot.bLobbyReturnRequested = false;
}

#undef LOCTEXT_NAMESPACE
