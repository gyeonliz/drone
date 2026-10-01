#include "Flow/DroneGameFlowSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Drone.h"
#include "Mission/DroneDefinition.h"
#include "Mission/DroneMissionDefinition.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "DroneGameFlow"

namespace DroneGameFlow
{
// 기본 목록 밖에 새로 만든 Definition을 자동 등록하는 폴더다.
// 새 Mission은 이 폴더에 DA_Mission_* 를 만들고 MissionMap·LobbyCategory 등을 채우면 C++ 수정 없이 로비에 뜬다.
const TCHAR* AutoRegisterDronePath = TEXT("/Game/Drone/Data/Drones");
const TCHAR* AutoRegisterMissionPath = TEXT("/Game/Drone/Data/Missions");

/** PackagePath 아래(하위 폴더 포함)의 TDefinition Data Asset을 불러온다. */
template <typename TDefinition>
TArray<TDefinition*> LoadDefinitionsUnderPath(const TCHAR* PackagePath)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
#if WITH_EDITOR
	// Editor를 막 켰을 때는 비동기 스캔이 끝나지 않았을 수 있어 이 폴더만 동기 스캔한다.
	Registry.ScanPathsSynchronous({FString(PackagePath)});
#endif
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(PackagePath), Assets, /*bRecursive*/ true);

	TArray<TDefinition*> Result;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.IsInstanceOf(TDefinition::StaticClass()))
		{
			if (TDefinition* Definition = Cast<TDefinition>(Asset.GetAsset()))
			{
				Result.Add(Definition);
			}
		}
	}
	return Result;
}

const TCHAR* DefaultDronePaths[] =
{
	TEXT("/Game/Drone/Data/Drones/DA_Drone_Scout_Greybox.DA_Drone_Scout_Greybox"),
	TEXT("/Game/Drone/Data/Drones/DA_Drone_FPVStrike_Greybox.DA_Drone_FPVStrike_Greybox"),
	TEXT("/Game/Drone/Data/Drones/DA_Drone_Drop_Greybox.DA_Drone_Drop_Greybox"),
	TEXT("/Game/Drone/Data/Drones/DA_Drone_FiberOptic_Greybox.DA_Drone_FiberOptic_Greybox"),
	TEXT("/Game/Drone/Data/Drones/DA_Drone_GroundUGV_Greybox.DA_Drone_GroundUGV_Greybox")
};
const TCHAR* DefaultMissionPaths[] =
{
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Racing_Circuit_Test.DA_Mission_Racing_Circuit_Test"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Training.DA_Mission_Tutorial_Training"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Hover.DA_Mission_Tutorial_Hover"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Forward.DA_Mission_Tutorial_Forward"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Heading.DA_Mission_Tutorial_Heading"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_GateFlight.DA_Mission_Tutorial_GateFlight"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_Payload.DA_Mission_Tutorial_Payload"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_FPV.DA_Mission_Tutorial_FPV"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_UGV_NPC.DA_Mission_Tutorial_UGV_NPC"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Tutorial_UGV_Turret.DA_Mission_Tutorial_UGV_Turret"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Story_GoldenTime_Test.DA_Mission_Story_GoldenTime_Test"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Story_Intercept_Test.DA_Mission_Story_Intercept_Test"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Story_VeilBreaker_Test.DA_Mission_Story_VeilBreaker_Test"),
	TEXT("/Game/Drone/Data/Missions/DA_Mission_Story_Endgame_Test.DA_Mission_Story_Endgame_Test")
};
}

void UDroneGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DroneDefinitions.Reset();
	MissionDefinitions.Reset();
	Snapshot = FDroneGameFlowSnapshot();
	LastLobbyMissionId = NAME_None;
	ClearRejection();

	// FLOW-02의 Front-end 화면이 열리기 전에 Catalog를 준비한다.
	// 기본 목록 + /Game/Drone/Data/{Drones,Missions} 폴더 자동 검색(2026-10-01).
	EnsureDefaultCatalog();
}

void UDroneGameFlowSubsystem::Deinitialize()
{
	DroneDefinitions.Reset();
	MissionDefinitions.Reset();
	Snapshot = FDroneGameFlowSnapshot();
	LastLobbyMissionId = NAME_None;
	ClearRejection();
	Super::Deinitialize();
}

bool UDroneGameFlowSubsystem::EnsureDefaultCatalog()
{
	for (const TCHAR* DefaultDronePath : DroneGameFlow::DefaultDronePaths)
	{
		UDroneDefinition* DefaultDrone = LoadObject<UDroneDefinition>(nullptr, DefaultDronePath);
		if (!DefaultDrone)
		{
			return Reject(FText::Format(
				LOCTEXT("DefaultDroneMissing", "기본 Drone Definition '{0}'을 불러오지 못했습니다."),
				FText::FromString(DefaultDronePath)));
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
	}

	for (const TCHAR* DefaultMissionPath : DroneGameFlow::DefaultMissionPaths)
	{
		UDroneMissionDefinition* DefaultMission = LoadObject<UDroneMissionDefinition>(nullptr, DefaultMissionPath);
		if (!DefaultMission)
		{
			return Reject(FText::Format(
				LOCTEXT("DefaultMissionMissing", "기본 Mission Definition '{0}'을 불러오지 못했습니다."),
				FText::FromString(DefaultMissionPath)));
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
	}

	// 기본 목록 밖의 Definition: 위 폴더에 새로 만든 DA를 C++ 수정 없이 등록한다.
	// 기본 목록은 계약 테스트의 기준이라 그대로 두고, 추가 DA 하나가 잘못돼도 기존 Catalog·로비는 유지한 채 경고만 남긴다.
	// Drone을 먼저 등록해야 Mission의 AllowedDroneIds 검증을 통과한다.
	for (UDroneDefinition* ExtraDrone : DroneGameFlow::LoadDefinitionsUnderPath<UDroneDefinition>(DroneGameFlow::AutoRegisterDronePath))
	{
		if (const UDroneDefinition* Registered = FindDroneDefinition(ExtraDrone->DroneId))
		{
			UE_CLOG(Registered != ExtraDrone, LogDrone, Warning,
				TEXT("Drone ID '%s'가 이미 다른 Asset으로 등록돼 '%s'를 건너뛴다."), *ExtraDrone->DroneId.ToString(), *GetPathNameSafe(ExtraDrone));
			continue;
		}
		if (!RegisterDroneDefinition(ExtraDrone))
		{
			UE_LOG(LogDrone, Warning, TEXT("Drone Definition '%s' 자동 등록 실패: %s"), *GetPathNameSafe(ExtraDrone), *LastRejectionReason.ToString());
		}
	}
	for (UDroneMissionDefinition* ExtraMission : DroneGameFlow::LoadDefinitionsUnderPath<UDroneMissionDefinition>(DroneGameFlow::AutoRegisterMissionPath))
	{
		if (const UDroneMissionDefinition* Registered = FindMissionDefinition(ExtraMission->MissionId))
		{
			UE_CLOG(Registered != ExtraMission, LogDrone, Warning,
				TEXT("Mission ID '%s'가 이미 다른 Asset으로 등록돼 '%s'를 건너뛴다."), *ExtraMission->MissionId.ToString(), *GetPathNameSafe(ExtraMission));
			continue;
		}
		if (!RegisterMissionDefinition(ExtraMission))
		{
			UE_LOG(LogDrone, Warning, TEXT("Mission Definition '%s' 자동 등록 실패: %s"), *GetPathNameSafe(ExtraMission), *LastRejectionReason.ToString());
		}
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

TArray<FName> UDroneGameFlowSubsystem::GetRegisteredDroneIds() const
{
	TArray<FName> DroneIds;
	DroneDefinitions.GenerateKeyArray(DroneIds);
	DroneIds.Sort([](const FName Left, const FName Right)
	{
		return Left.Compare(Right) < 0;
	});
	return DroneIds;
}

TArray<UDroneDefinition*> UDroneGameFlowSubsystem::GetAvailableDroneDefinitions() const
{
	TArray<UDroneDefinition*> AvailableDefinitions;
	AvailableDefinitions.Reserve(Snapshot.AvailableDroneIds.Num());
	for (const FName DroneId : Snapshot.AvailableDroneIds)
	{
		if (UDroneDefinition* Definition = FindDroneDefinition(DroneId);
			Definition && Definition->bPlayerControllableInCurrentBuild && !Definition->bLocked)
		{
			AvailableDefinitions.Add(Definition);
		}
	}
	return AvailableDefinitions;
}

UDroneDefinition* UDroneGameFlowSubsystem::GetSelectedDroneDefinition() const
{
	return Snapshot.SelectedDroneId.IsNone()
		? nullptr
		: FindDroneDefinition(Snapshot.SelectedDroneId);
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

bool UDroneGameFlowSubsystem::RequestBackNavigation()
{
	const EDroneGameFlowState Previous = Snapshot.State;
	switch (Previous)
	{
	case EDroneGameFlowState::LobbyMissionSelect:
		// 시작 화면으로 나갈 때만 미션 선택을 지운다. Catalog/Story Fact는 유지한다.
		ResetRuntimeSelection(true);
		return ChangeState(Previous, EDroneGameFlowState::OpeningTrailer);
	case EDroneGameFlowState::MissionTrailer:
		ResetRuntimeSelection(false);
		return ChangeState(Previous, EDroneGameFlowState::LobbyMissionSelect);
	case EDroneGameFlowState::DroneSelect:
		ResetRuntimeSelection(false);
		return ChangeState(Previous, EDroneGameFlowState::MissionTrailer);
	default:
		return Reject(LOCTEXT("BackNavigationInvalid", "출격 전 선택 화면에서만 뒤로갈 수 있습니다."));
	}
}

bool UDroneGameFlowSubsystem::SelectMission(const FName MissionId)
{
	if (Snapshot.State != EDroneGameFlowState::LobbyMissionSelect)
	{
		return Reject(LOCTEXT("SelectMissionWrongState", "로비의 Mission 선택 상태에서만 Mission을 선택할 수 있습니다."));
	}

	if (!ApplyMissionSelection(MissionId))
	{
		return Reject(FText::Format(LOCTEXT("MissionNotFound", "Mission ID '{0}'를 찾을 수 없습니다."), FText::FromName(MissionId)));
	}
	ClearRejection();
	BroadcastSnapshot();
	return true;
}

bool UDroneGameFlowSubsystem::ApplyMissionSelection(const FName MissionId)
{
	const UDroneMissionDefinition* Mission = FindMissionDefinition(MissionId);
	if (!Mission)
	{
		return false;
	}
	Snapshot.SelectedMissionId = MissionId;
	LastLobbyMissionId = MissionId;
	Snapshot.AvailableDroneIds.Reset();
	for (const FName AllowedDroneId : Mission->AllowedDroneIds)
	{
		const UDroneDefinition* Definition = FindDroneDefinition(AllowedDroneId);
		if (Definition && Definition->bPlayerControllableInCurrentBuild && !Definition->bLocked)
		{
			Snapshot.AvailableDroneIds.Add(AllowedDroneId);
		}
	}
	Snapshot.SelectedDroneId = NAME_None;
	Snapshot.bMissionStartRequested = false;
	Snapshot.LastMissionOutcome = EDroneMissionOutcome::None;
	Snapshot.LastMissionElapsedSeconds = -1.0;
	Snapshot.bLobbyReturnRequested = false;
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
	if (!Drone || Drone->bLocked || !Drone->bPlayerControllableInCurrentBuild)
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
	const UDroneDefinition* SelectedDrone = FindDroneDefinition(Snapshot.SelectedDroneId);
	if (Snapshot.State != EDroneGameFlowState::DroneSelect
		|| !SelectedDrone
		|| SelectedDrone->bLocked
		|| !SelectedDrone->bPlayerControllableInCurrentBuild)
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
	static const TArray<FName> NoFacts;
	return CompleteMissionWithStoryFacts(Outcome, NoFacts, NoFacts);
}

bool UDroneGameFlowSubsystem::CompleteMissionWithStoryFacts(
	const EDroneMissionOutcome Outcome,
	const TArray<FName>& GrantedFacts,
	const TArray<FName>& RemovedFacts,
	const double ElapsedSeconds)
{
	if (Snapshot.State != EDroneGameFlowState::InMission
		|| Outcome == EDroneMissionOutcome::None)
	{
		return Reject(LOCTEXT("MissionCompleteInvalid", "진행 중 Mission에는 Success 또는 Failure 결과가 필요합니다."));
	}
	if (Outcome == EDroneMissionOutcome::Success)
	{
		for (const FName FactId : RemovedFacts)
		{
			Snapshot.StoryFacts.Remove(FactId);
		}
		for (const FName FactId : GrantedFacts)
		{
			if (!FactId.IsNone())
			{
				Snapshot.StoryFacts.AddUnique(FactId);
			}
		}
		Snapshot.StoryFacts.Sort([](const FName Left, const FName Right)
		{
			return Left.LexicalLess(Right);
		});
		if (!Snapshot.SelectedMissionId.IsNone())
		{
			Snapshot.CompletedMissionIds.AddUnique(Snapshot.SelectedMissionId);
		}
	}

	Snapshot.bMissionStartRequested = false;
	Snapshot.LastMissionOutcome = Outcome;
	Snapshot.LastMissionElapsedSeconds = ElapsedSeconds;
	return ChangeState(EDroneGameFlowState::InMission, EDroneGameFlowState::MissionResult);
}

bool UDroneGameFlowSubsystem::HasStoryFact(const FName FactId) const
{
	return !FactId.IsNone() && Snapshot.StoryFacts.Contains(FactId);
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
	Snapshot.LastMissionElapsedSeconds = -1.0;
	Snapshot.bLobbyReturnRequested = false;
	return ChangeState(EDroneGameFlowState::MissionResult, EDroneGameFlowState::LoadingMissionMap);
}

bool UDroneGameFlowSubsystem::RequestNextMission()
{
	const FName NextId = GetNextMissionId();
	if (Snapshot.State != EDroneGameFlowState::MissionResult
		|| Snapshot.LastMissionOutcome != EDroneMissionOutcome::Success
		|| NextId.IsNone())
	{
		return Reject(LOCTEXT("NextMissionInvalid", "성공한 미션에 이어질 다음 미션이 있을 때만 넘어갈 수 있습니다."));
	}
	ApplyMissionSelection(NextId);
	// 다음 미션도 조작·목표 브리핑부터 본다(Tutorial 가이드: 브리핑 → 시작 → 플레이 → 클리어 → 다음 수업).
	return ChangeState(EDroneGameFlowState::MissionResult, EDroneGameFlowState::MissionTrailer);
}

FName UDroneGameFlowSubsystem::GetNextMissionId() const
{
	const UDroneMissionDefinition* Mission = FindMissionDefinition(Snapshot.SelectedMissionId);
	if (!Mission || Mission->NextMissionId == Snapshot.SelectedMissionId || !FindMissionDefinition(Mission->NextMissionId))
	{
		return NAME_None;
	}
	return Mission->NextMissionId;
}

TArray<FName> UDroneGameFlowSubsystem::GetMissionSequence(const FName MissionId) const
{
	TArray<FName> Sequence;
	if (!FindMissionDefinition(MissionId))
	{
		return Sequence;
	}
	// 앞 미션 표. 여러 미션이 같은 다음 미션을 가리키면 ID 순 첫 번째를 앞 미션으로 본다.
	TMap<FName, FName> Previous;
	for (const FName Id : GetRegisteredMissionIds())
	{
		const UDroneMissionDefinition* Definition = FindMissionDefinition(Id);
		if (Definition && Definition->NextMissionId != Id && FindMissionDefinition(Definition->NextMissionId)
			&& !Previous.Contains(Definition->NextMissionId))
		{
			Previous.Add(Definition->NextMissionId, Id);
		}
	}
	// 처음 미션까지 거슬러 올라간 뒤 끝까지 따라간다. DA를 고리로 잘못 이어도 멈추도록 방문 기록을 둔다.
	FName Head = MissionId;
	TSet<FName> Visited;
	Visited.Add(Head);
	while (const FName* Before = Previous.Find(Head))
	{
		if (Visited.Contains(*Before))
		{
			break;
		}
		Visited.Add(*Before);
		Head = *Before;
	}
	Visited.Reset();
	for (FName Id = Head; !Id.IsNone() && !Visited.Contains(Id);)
	{
		const UDroneMissionDefinition* Definition = FindMissionDefinition(Id);
		if (!Definition)
		{
			break;
		}
		Visited.Add(Id);
		Sequence.Add(Id);
		Id = Definition->NextMissionId;
	}
	return Sequence;
}

bool UDroneGameFlowSubsystem::GetMissionSequencePosition(
	const FName MissionId,
	int32& OutNumber,
	int32& OutCount,
	int32& OutCompletedCount) const
{
	const TArray<FName> Sequence = GetMissionSequence(MissionId);
	OutNumber = Sequence.IndexOfByKey(MissionId) + 1;
	OutCount = Sequence.Num();
	OutCompletedCount = 0;
	for (const FName Id : Sequence)
	{
		OutCompletedCount += IsMissionCompleted(Id) ? 1 : 0;
	}
	return OutCount >= 2 && OutNumber >= 1;
}

bool UDroneGameFlowSubsystem::IsMissionCompleted(const FName MissionId) const
{
	return !MissionId.IsNone() && Snapshot.CompletedMissionIds.Contains(MissionId);
}

TArray<FName> UDroneGameFlowSubsystem::GetMissionIdsInLobbyOrder(const EDroneMissionCategory Category) const
{
	TArray<FName> Result;
	for (const FName Id : GetRegisteredMissionIds())
	{
		const UDroneMissionDefinition* Definition = FindMissionDefinition(Id);
		if (Definition && Definition->GetLobbyCategory() == Category)
		{
			Result.Add(Id);
		}
	}
	TMap<FName, TPair<FName, int32>> SortKeys;
	for (const FName Id : Result)
	{
		const TArray<FName> Sequence = GetMissionSequence(Id);
		SortKeys.Add(Id, Sequence.Num() >= 2
			? TPair<FName, int32>(Sequence[0], Sequence.IndexOfByKey(Id))
			: TPair<FName, int32>(Id, 0));
	}
	Result.StableSort([&SortKeys](const FName Left, const FName Right)
	{
		const TPair<FName, int32>& L = SortKeys[Left];
		const TPair<FName, int32>& R = SortKeys[Right];
		const int32 HeadOrder = L.Key.Compare(R.Key);
		return HeadOrder != 0 ? HeadOrder < 0 : L.Value < R.Value;
	});
	return Result;
}

bool UDroneGameFlowSubsystem::RequestReturnToLobbyFocusing(const FName FocusMissionId)
{
	if (!RequestReturnToLobby())
	{
		return false;
	}
	// 로비는 LastLobbyMissionId가 속한 탭을 열고 그 미션에 포커스를 둔다.
	if (FindMissionDefinition(FocusMissionId))
	{
		LastLobbyMissionId = FocusMissionId;
		BroadcastSnapshot();
	}
	return true;
}

bool UDroneGameFlowSubsystem::RequestReturnToTitle()
{
	if (Snapshot.State != EDroneGameFlowState::MissionResult)
	{
		return Reject(LOCTEXT("ReturnTitleInvalid", "Mission 결과 상태에서만 시작 메뉴로 갈 수 있습니다."));
	}
	ResetRuntimeSelection(true);
	Snapshot.LastMissionElapsedSeconds = -1.0;
	return ChangeState(EDroneGameFlowState::MissionResult, EDroneGameFlowState::OpeningTrailer);
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
