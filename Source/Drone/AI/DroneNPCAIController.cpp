#include "AI/DroneNPCAIController.h"

#include "AI/DroneAITags.h"
#include "AI/DroneMGTurretStation.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/DroneSmartObjectReservationComponent.h"
#include "AI/DroneSmartObjectStation.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Health/DroneHealthComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Prototype/DronePrototypePawn.h"
#include "StateTree.h"
#include "TimerManager.h"

namespace
{
	constexpr const TCHAR* HostilePatrolStateTreePath =
		TEXT("/Game/Drone/AI/StateTrees/ST_NPC_HostilePatrol.ST_NPC_HostilePatrol");
	constexpr const TCHAR* FriendlyBaseRoutineStateTreePath =
		TEXT("/Game/Drone/AI/StateTrees/ST_NPC_FriendlyBaseRoutine.ST_NPC_FriendlyBaseRoutine");
}

ADroneNPCAIController::ADroneNPCAIController()
{
	bAttachToPawn = true;

	StateTreeAIComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));
	// 역할별 Tree를 Controller가 명시적으로 선택한다. 자동 시작을 켜면 Friendly까지
	// Hostile Tree를 공유하거나, Asset 지정 전 빈 Tree를 시작할 수 있으므로 끈다.
	StateTreeAIComponent->SetStartLogicAutomatically(false);

	ReservationComponent = CreateDefaultSubobject<UDroneSmartObjectReservationComponent>(TEXT("ReservationComponent"));

	DronePerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("DronePerceptionComponent"));
	SetPerceptionComponent(*DronePerceptionComponent);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	// 첫 감지 Spike용 편집 가능 시험값이다. 최종 탐지 거리·각도·난이도 규칙이 아니다.
	SightConfig->SightRadius = 4000.0f;
	SightConfig->LoseSightRadius = 4500.0f;
	SightConfig->PeripheralVisionAngleDegrees = 70.0f;
	SightConfig->SetMaxAge(3.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	DronePerceptionComponent->ConfigureSense(*SightConfig);
	DronePerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
	DronePerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
		this,
		&ADroneNPCAIController::HandleTargetPerceptionUpdated);
}

void ADroneNPCAIController::BeginPlay()
{
	Super::BeginPlay();

	// UWorldSubsystem::OnWorldBeginPlay 뒤라 Smart Object Runtime 조회가 안전하다.
	// 레벨에 미리 배치된 Controller는 OnPossess에서 Asset만 지정하고 여기서 실행한다.
	TryStartAssignedStateTree();
}

void ADroneNPCAIController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// StateTree의 MG 진입 Task는 한 번만 호출될 수 있다. 이미 시선 보간을 위해
	// 동작하는 Controller Tick에서 실제 점유자 한 명의 포탑 조준만 계속 갱신한다.
	if (ResponseState == EDroneNPCAIResponseState::UseMGTurret)
	{
		UpdateMGTurretOperation();
	}
	else
	{
		UpdatePersonalWeaponFacing(DeltaSeconds);
	}
	// 몸 정렬을 먼저 적용한 뒤 Pawn 로컬 기준 시선각을 계산한다.
	UpdateDroneGaze(DeltaSeconds);
}

void ADroneNPCAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ClearDroneGameplayFocus();
	CancelPendingDroneLost();
	DetectedDrone.Reset();
	ResponseState = EDroneNPCAIResponseState::Patrol;
	bHasLastKnownDroneLocation = false;
	LastKnownDroneLocation = FVector::ZeroVector;
	DroneDetectionCount = 0;
	DroneLostCount = 0;
	DroneSearchStartCount = 0;
	CompletedDroneSearchCount = 0;
	DroneDestroyedResponseCount = 0;
	MGTurretClaimCount = 0;
	MGTurretArrivalCount = 0;
	MGTurretUseCount = 0;
	ActiveMGTurretStation.Reset();
	CoverClaimCount = 0;
	CoverUseCount = 0;
	SmoothedDroneLookRotation = FRotator::ZeroRotator;
	DroneLookAlpha = 0.0f;

	if (const UDroneNPCProfileComponent* Profile = GetPossessedProfile())
	{
		ReservationComponent->SetUserTags(Profile->BuildSmartObjectUserTags());
		if (UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent())
		{
			WeaponComponent->ConfigureWeapon(Profile->GetProfile().WeaponType);
		}
	}
	else if (UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent())
	{
		WeaponComponent->ConfigureWeapon(EDroneNPCWeaponType::Unarmed);
	}
	ConfigureDefaultPatrolActivities();

	// 역할별 Asset을 명시적으로 분리해 Friendly가 적 전투 분기를 공유하지 않게 한다.
	if (IsHostileNPC())
	{
		if (UStateTree* HostilePatrolStateTree = LoadObject<UStateTree>(nullptr, HostilePatrolStateTreePath))
		{
			StateTreeAIComponent->SetStateTree(HostilePatrolStateTree);
			// Runtime Spawn처럼 Controller BeginPlay가 이미 끝난 경우에는 지금 시작한다.
			// 레벨 로딩 중 Possess라면 BeginPlay가 Smart Object 초기화 뒤 시작한다.
			if (HasActorBegunPlay())
			{
				TryStartAssignedStateTree();
			}
		}
	}
	else if (IsFriendlyNPC())
	{
		if (UStateTree* FriendlyStateTree = LoadObject<UStateTree>(nullptr, FriendlyBaseRoutineStateTreePath))
		{
			StateTreeAIComponent->SetStateTree(FriendlyStateTree);
			if (HasActorBegunPlay())
			{
				TryStartAssignedStateTree();
			}
		}
	}
}

void ADroneNPCAIController::TryStartAssignedStateTree()
{
	if (ResponseState != EDroneNPCAIResponseState::Dead
		&& (IsHostileNPC() || IsFriendlyNPC())
		&& StateTreeAIComponent
		&& !StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->StartLogic();
	}
}

void ADroneNPCAIController::OnUnPossess()
{
	ClearDroneGameplayFocus();
	CancelPendingDroneLost();
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	ResponseState = EDroneNPCAIResponseState::Patrol;
	bHasLastKnownDroneLocation = false;
	if (StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->StopLogic(TEXT("NPC UnPossessed"));
	}
	Super::OnUnPossess();
}

bool ADroneNPCAIController::UsesRifle() const
{
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	return Profile && Profile->GetProfile().WeaponType == EDroneNPCWeaponType::Rifle;
}

bool ADroneNPCAIController::UsesShotgun() const
{
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	return Profile && Profile->GetProfile().WeaponType == EDroneNPCWeaponType::Shotgun;
}

bool ADroneNPCAIController::CanUseMGTurret() const
{
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	return Profile
		&& Profile->IsHostile()
		&& Profile->GetProfile().bCanUseMGTurret;
}

bool ADroneNPCAIController::IsHostileNPC() const
{
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	return Profile && Profile->IsHostile();
}

bool ADroneNPCAIController::IsFriendlyNPC() const
{
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	return Profile && Profile->IsFriendly();
}

UDroneNPCWeaponComponent* ADroneNPCAIController::GetPossessedWeaponComponent() const
{
	return GetPawn() ? GetPawn()->FindComponentByClass<UDroneNPCWeaponComponent>() : nullptr;
}

bool ADroneNPCAIController::CanFirePersonalWeapon() const
{
	AActor* TargetActor = GetDetectedDrone();
	const UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent();
	return ResponseState != EDroneNPCAIResponseState::Dead
		&& IsHostileNPC()
		&& TargetActor
		&& WeaponComponent
		&& WeaponComponent->CanFire(TargetActor, TargetActor->GetActorLocation());
}

bool ADroneNPCAIController::StartPersonalWeaponFire()
{
	AActor* TargetActor = GetDetectedDrone();
	UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent();
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| !IsHostileNPC()
		|| !TargetActor
		|| !WeaponComponent)
	{
		return false;
	}

	// AI는 빈 탄창이면 공용 Reload 경계를 명시적으로 호출한다. 현재 Greybox Reload는
	// 즉시 완료되며 시간·Animation·예비 탄약은 후속 카드에서 이 지점에 연결한다.
	if (!WeaponComponent->HasMagazineAmmo() && !WeaponComponent->Reload())
	{
		return false;
	}
	return WeaponComponent->StartFire(TargetActor, TargetActor->GetActorLocation());
}

void ADroneNPCAIController::StopPersonalWeaponFire()
{
	if (UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent())
	{
		WeaponComponent->StopFire();
	}
}

bool ADroneNPCAIController::ReloadPersonalWeapon()
{
	UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent();
	return ResponseState != EDroneNPCAIResponseState::Dead
		&& IsHostileNPC()
		&& WeaponComponent
		&& WeaponComponent->Reload();
}

void ADroneNPCAIController::EnterDroneDetectedResponse()
{
	if (!IsHostileNPC() || ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return;
	}

	StopMGTurretOperation();
	ResponseState = EDroneNPCAIResponseState::DroneDetected;
	StopMovement();
	ReservationComponent->ReleaseReservation();
}

bool ADroneNPCAIController::BeginDroneSearch(const float AcceptanceRadius)
{
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| !IsHostileNPC() || HasDetectedDrone() || !bHasLastKnownDroneLocation || !GetPawn())
	{
		return false;
	}

	ResponseState = EDroneNPCAIResponseState::Search;
	++DroneSearchStartCount;
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	MoveToLocation(
		LastKnownDroneLocation,
		FMath::Max(10.0f, AcceptanceRadius),
		true,
		true,
		true,
		true,
		nullptr,
		false);
	// 마지막 위치가 NavMesh 밖이어도 Search 상태에서 제자리 탐색 시간을 보낸다.
	return true;
}

void ADroneNPCAIController::CompleteDroneSearch()
{
	if (ResponseState != EDroneNPCAIResponseState::Search)
	{
		return;
	}

	StopMovement();
	ClearDroneGameplayFocus();
	++CompletedDroneSearchCount;
	ResponseState = EDroneNPCAIResponseState::Patrol;
	ConfigureDefaultPatrolActivities();
}

void ADroneNPCAIController::HandlePossessedPawnDeath()
{
	if (ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return;
	}

	// Slot을 먼저 Free로 돌려놓은 뒤 대기 중인 다른 MG 가능 NPC에게 재시도 Event를 보낸다.
	CancelPendingDroneLost();
	ClearDroneGameplayFocus();
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	bHasLastKnownDroneLocation = false;
	ResponseState = EDroneNPCAIResponseState::Dead;

	if (StateTreeAIComponent && StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->StopLogic(TEXT("NPC Health reached zero"));
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<ADroneNPCAIController> It(World); It; ++It)
	{
		ADroneNPCAIController* OtherController = *It;
		const EDroneNPCAIResponseState OtherState = OtherController
			? OtherController->GetResponseState()
			: EDroneNPCAIResponseState::Dead;
		const bool bCanRetryFromCurrentState = OtherState == EDroneNPCAIResponseState::DroneDetected
			|| OtherState == EDroneNPCAIResponseState::MoveToCover
			|| OtherState == EDroneNPCAIResponseState::UseCover;
		if (!OtherController
			|| OtherController == this
			|| !bCanRetryFromCurrentState
			|| !OtherController->HasDetectedDrone()
			|| !OtherController->CanUseMGTurret())
		{
			continue;
		}

		OtherController->StopPersonalWeaponFire();
		if (UStateTreeAIComponent* OtherStateTree = OtherController->GetStateTreeAIComponent())
		{
			OtherStateTree->SendStateTreeEvent(DroneAITags::Event_DroneDetected);
		}
	}
}

void ADroneNPCAIController::HandleDetectedDroneDestroyed(AActor* DestroyedDrone)
{
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| !DestroyedDrone
		|| DetectedDrone.Get() != DestroyedDrone)
	{
		return;
	}

	// DroneLost와 달리 파괴된 표적의 마지막 위치를 Search하지 않는다. 전투 자원을
	// 즉시 정리한 뒤 기존 DroneLost 전환을 사용해 Search 실패 -> Patrol로 복귀시킨다.
	CancelPendingDroneLost();
	ClearDroneGameplayFocus();
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	bHasLastKnownDroneLocation = false;
	LastKnownDroneLocation = FVector::ZeroVector;
	ResponseState = EDroneNPCAIResponseState::Patrol;
	ConfigureDefaultPatrolActivities();
	++DroneDestroyedResponseCount;

	if (StateTreeAIComponent && StateTreeAIComponent->IsRunning())
	{
		// 마지막 위치 Flag가 false라 Search Task는 즉시 실패하고 ClaimPatrol로 전환된다.
		// 별도 Destroyed Event를 추가하지 않아 기존 StateTree Asset과도 호환된다.
		StateTreeAIComponent->SendStateTreeEvent(DroneAITags::Event_DroneLost);
	}
	else
	{
		TryStartAssignedStateTree();
	}
}

void ADroneNPCAIController::ConfigureDefaultPatrolActivities()
{
	FGameplayTagContainer Activities;
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	if (!Profile)
	{
		ReservationComponent->SetRequiredActivityTags(Activities);
		return;
	}

	if (Profile->IsHostile())
	{
		Activities.AddTag(DroneAITags::Activity_EnemyPatrol);
		Activities.AddTag(DroneAITags::Activity_Guard);
	}
	else if (Profile->IsFriendly())
	{
		Activities.AddTag(DroneAITags::Activity_FriendlyBasePatrol);
		Activities.AddTag(DroneAITags::Activity_Ambient);
	}
	else
	{
		Activities.AddTag(DroneAITags::Activity_Ambient);
	}

	ReservationComponent->SetRequiredActivityTags(Activities);
}

bool ADroneNPCAIController::AlignPawnToReservedSlot()
{
	APawn* ControlledPawn = GetPawn();
	FTransform SlotTransform;
	if (!ControlledPawn
		|| !ReservationComponent
		|| !ReservationComponent->GetReservedSlotTransform(SlotTransform))
	{
		return false;
	}

	// 지면 NPC의 Slot 방향은 Yaw만 사용한다. Definition 또는 Station에 Pitch/Roll이
	// 들어가도 Character가 기울어지지 않게 하고, ControlRotation도 함께 맞춰 다음
	// 프레임에 Controller가 도착 방향을 즉시 덮어쓰지 않도록 한다.
	const FRotator SlotFacing(0.0f, SlotTransform.Rotator().Yaw, 0.0f);
	SetControlRotation(SlotFacing);
	ControlledPawn->SetActorRotation(SlotFacing);
	return true;
}

bool ADroneNPCAIController::PrepareMGTurretSearch()
{
	if (ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return false;
	}

	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	if (!Profile || !Profile->IsHostile() || !Profile->GetProfile().bCanUseMGTurret)
	{
		return false;
	}

	FGameplayTagContainer Activities;
	Activities.AddTag(DroneAITags::Activity_MGTurret);
	ReservationComponent->SetRequiredActivityTags(Activities);
	return true;
}

bool ADroneNPCAIController::ClaimAvailableMGTurret(FTransform& OutSlotTransform)
{
	OutSlotTransform = FTransform::Identity;
	if (!HasDetectedDrone() || !GetPawn() || !PrepareMGTurretSearch())
	{
		return false;
	}

	// MG 이동을 선택한 NPC는 개인 무기 Timer를 먼저 정리한다. Claim 실패 시
	// StateTree가 DroneDetected 개인 무기 상태로 즉시 대체한다.
	StopPersonalWeaponFire();
	StopMovement();
	ReservationComponent->ReleaseReservation();
	if (!ReservationComponent->ClaimNearestAvailableSlot(GetPawn()->GetActorLocation(), OutSlotTransform))
	{
		ConfigureDefaultPatrolActivities();
		return false;
	}
	if (!GetReservedMGTurretOperatorTransform(OutSlotTransform))
	{
		ReservationComponent->ReleaseReservation();
		ConfigureDefaultPatrolActivities();
		return false;
	}

	ResponseState = EDroneNPCAIResponseState::MoveToMGTurret;
	++MGTurretClaimCount;
	return true;
}

bool ADroneNPCAIController::CompleteMGTurretMove()
{
	if (ResponseState != EDroneNPCAIResponseState::MoveToMGTurret
		|| !HasDetectedDrone()
		|| !ReservationComponent->HasValidReservation()
		|| !AlignPawnToMGTurretOperator())
	{
		return false;
	}

	StopMovement();
	ResponseState = EDroneNPCAIResponseState::HoldMGTurret;
	++MGTurretArrivalCount;
	return true;
}

bool ADroneNPCAIController::BeginMGTurretOperation()
{
	if (ResponseState != EDroneNPCAIResponseState::HoldMGTurret
		|| !HasDetectedDrone()
		|| !GetPawn()
		|| !ReservationComponent->HasValidReservation())
	{
		return false;
	}
	if (!AlignPawnToMGTurretOperator())
	{
		return false;
	}

	ADroneSmartObjectStation* Station = Cast<ADroneSmartObjectStation>(
		ReservationComponent->GetReservedSmartObjectActor());
	if (!Station || !ReservationComponent->MarkReservationOccupied())
	{
		return false;
	}

	if (!Station->BeginMGTurretUse(GetPawn(), GetDetectedDrone()))
	{
		ReservationComponent->ReleaseReservation();
		return false;
	}

	ActiveMGTurretStation = Station;
	ResponseState = EDroneNPCAIResponseState::UseMGTurret;
	++MGTurretUseCount;
	return true;
}

bool ADroneNPCAIController::UpdateMGTurretOperation()
{
	ADroneSmartObjectStation* Station = ActiveMGTurretStation.Get();
	if (ResponseState != EDroneNPCAIResponseState::UseMGTurret
		|| !HasDetectedDrone()
		|| !GetPawn()
		|| !ReservationComponent->IsReservationOccupied()
		|| !Station)
	{
		return false;
	}

	// 포탑 몸체를 먼저 갱신해야 그 자식 Operator Anchor의 위치·방향도 같은
	// 프레임의 Yaw를 가진다. 그 뒤 사수를 Anchor에 붙여 한 프레임 지연을 없앤다.
	return Station->UpdateMGTurretUse(GetPawn(), GetDetectedDrone())
		&& AlignPawnToMGTurretOperator();
}

void ADroneNPCAIController::StopMGTurretOperation()
{
	if (ADroneSmartObjectStation* Station = ActiveMGTurretStation.Get())
	{
		Station->EndMGTurretUse(GetPawn());
	}
	ActiveMGTurretStation.Reset();
}

void ADroneNPCAIController::AbortMGTurretResponse()
{
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	if (ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return;
	}
	ConfigureDefaultPatrolActivities();
	ResponseState = HasDetectedDrone()
		? EDroneNPCAIResponseState::DroneDetected
		: EDroneNPCAIResponseState::Patrol;
}

bool ADroneNPCAIController::ClaimAvailableCover(FTransform& OutSlotTransform)
{
	OutSlotTransform = FTransform::Identity;
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| !IsHostileNPC()
		|| !HasDetectedDrone()
		|| !GetPawn())
	{
		return false;
	}

	StopMovement();
	ReservationComponent->ReleaseReservation();
	FGameplayTagContainer Activities;
	Activities.AddTag(DroneAITags::Activity_Cover);
	ReservationComponent->SetRequiredActivityTags(Activities);
	if (!ReservationComponent->ClaimNearestAvailableSlot(GetPawn()->GetActorLocation(), OutSlotTransform))
	{
		ConfigureDefaultPatrolActivities();
		ResponseState = EDroneNPCAIResponseState::DroneDetected;
		StartPersonalWeaponFire();
		return false;
	}

	StopPersonalWeaponFire();
	ResponseState = EDroneNPCAIResponseState::MoveToCover;
	++CoverClaimCount;
	return true;
}

bool ADroneNPCAIController::CompleteCoverMove()
{
	if (ResponseState != EDroneNPCAIResponseState::MoveToCover
		|| !HasDetectedDrone()
		|| !ReservationComponent->HasValidReservation()
		|| !AlignPawnToReservedSlot()
		|| !ReservationComponent->MarkReservationOccupied())
	{
		return false;
	}

	StopMovement();
	ResponseState = EDroneNPCAIResponseState::UseCover;
	++CoverUseCount;
	return StartPersonalWeaponFire();
}

bool ADroneNPCAIController::UpdateCoverResponse()
{
	if (ResponseState != EDroneNPCAIResponseState::UseCover
		|| !HasDetectedDrone()
		|| !ReservationComponent->IsReservationOccupied())
	{
		return false;
	}

	UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent();
	return WeaponComponent
		&& (WeaponComponent->IsFiring() || StartPersonalWeaponFire());
}

void ADroneNPCAIController::AbortCoverResponse()
{
	StopMovement();
	ReservationComponent->ReleaseReservation();
	if (ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return;
	}

	ConfigureDefaultPatrolActivities();
	ResponseState = HasDetectedDrone()
		? EDroneNPCAIResponseState::DroneDetected
		: EDroneNPCAIResponseState::Patrol;
	if (HasDetectedDrone())
	{
		StartPersonalWeaponFire();
	}
}

bool ADroneNPCAIController::ClaimNextEnemyPatrolSlot(FTransform& OutSlotTransform)
{
	OutSlotTransform = FTransform::Identity;
	if (!IsHostileNPC() || !GetPawn() || HasDetectedDrone())
	{
		return false;
	}

	FGameplayTagContainer Activities;
	Activities.AddTag(DroneAITags::Activity_EnemyPatrol);
	ReservationComponent->SetRequiredActivityTags(Activities);

	if (bHasCompletedPatrolSlot)
	{
		return ReservationComponent->ClaimNearestAvailableSlotAvoiding(
			GetPawn()->GetActorLocation(),
			LastCompletedPatrolSlotLocation,
			PatrolRepeatAvoidanceRadius,
			OutSlotTransform);
	}
	return ReservationComponent->ClaimNearestAvailableSlot(GetPawn()->GetActorLocation(), OutSlotTransform);
}

void ADroneNPCAIController::CompleteCurrentPatrolSlot()
{
	FTransform SlotTransform;
	if (ReservationComponent->GetReservedSlotTransform(SlotTransform))
	{
		LastCompletedPatrolSlotLocation = SlotTransform.GetLocation();
		bHasCompletedPatrolSlot = true;
		++CompletedPatrolCycles;

		const bool bAlreadyVisited = VisitedPatrolSlotLocations.ContainsByPredicate(
			[this](const FVector& Location)
			{
				return Location.Equals(LastCompletedPatrolSlotLocation, 10.0f);
			});
		if (!bAlreadyVisited)
		{
			VisitedPatrolSlotLocations.Add(LastCompletedPatrolSlotLocation);
		}
	}
	ReservationComponent->ReleaseReservation();
}

bool ADroneNPCAIController::ClaimNextFriendlyActivitySlot(FTransform& OutSlotTransform)
{
	OutSlotTransform = FTransform::Identity;
	if (!IsFriendlyNPC() || !GetPawn())
	{
		return false;
	}

	const FGameplayTag PreferredActivity = bPreferAmbientActivity
		? DroneAITags::Activity_Ambient
		: DroneAITags::Activity_FriendlyBasePatrol;
	const FGameplayTag FallbackActivity = bPreferAmbientActivity
		? DroneAITags::Activity_FriendlyBasePatrol
		: DroneAITags::Activity_Ambient;

	auto TryClaimActivity = [this, &OutSlotTransform](const FGameplayTag Activity)
	{
		FGameplayTagContainer Activities;
		Activities.AddTag(Activity);
		ReservationComponent->SetRequiredActivityTags(Activities);

		const bool bClaimed = bHasCompletedFriendlySlot
			? ReservationComponent->ClaimNearestAvailableSlotAvoiding(
				GetPawn()->GetActorLocation(),
				LastCompletedFriendlySlotLocation,
				PatrolRepeatAvoidanceRadius,
				OutSlotTransform)
			: ReservationComponent->ClaimNearestAvailableSlot(GetPawn()->GetActorLocation(), OutSlotTransform);
		if (bClaimed)
		{
			CurrentFriendlyActivity = Activity;
		}
		return bClaimed;
	};

	// 선호 종류의 모든 Slot이 사용 중이면 다른 아군 활동으로 넘어가 전체 루틴 정지를 피한다.
	return TryClaimActivity(PreferredActivity) || TryClaimActivity(FallbackActivity);
}

void ADroneNPCAIController::CompleteCurrentFriendlyActivitySlot()
{
	FTransform SlotTransform;
	if (ReservationComponent->GetReservedSlotTransform(SlotTransform))
	{
		LastCompletedFriendlySlotLocation = SlotTransform.GetLocation();
		bHasCompletedFriendlySlot = true;
		++CompletedFriendlyRoutineCycles;

		const bool bAlreadyVisited = VisitedFriendlySlotLocations.ContainsByPredicate(
			[this](const FVector& Location)
			{
				return Location.Equals(LastCompletedFriendlySlotLocation, 10.0f);
			});
		if (!bAlreadyVisited)
		{
			VisitedFriendlySlotLocations.Add(LastCompletedFriendlySlotLocation);
		}

		if (CurrentFriendlyActivity.IsValid())
		{
			VisitedFriendlyActivities.AddTag(CurrentFriendlyActivity);
			bPreferAmbientActivity = CurrentFriendlyActivity == DroneAITags::Activity_FriendlyBasePatrol;
		}
	}

	CurrentFriendlyActivity = FGameplayTag();
	ReservationComponent->ReleaseReservation();
}

bool ADroneNPCAIController::HasVisitedFriendlyActivity(const FGameplayTag ActivityTag) const
{
	return ActivityTag.IsValid() && VisitedFriendlyActivities.HasTagExact(ActivityTag);
}

void ADroneNPCAIController::HandleTargetPerceptionUpdated(AActor* Actor, const FAIStimulus Stimulus)
{
	if (ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return;
	}

	// 현재 첫 감지 대상은 프로젝트 소유 Drone Prototype만 허용한다.
	if (!Actor || !Actor->IsA<ADronePrototypePawn>())
	{
		return;
	}

	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	if (!Profile || !Profile->IsHostile())
	{
		// Friendly/Neutral NPC는 드론을 보더라도 전투 상태로 전환하지 않는다.
		return;
	}

	// 사망한 Drone의 뒤늦은 Sight 자극은 새 감지로 등록하지 않는다.
	if (const UDroneHealthComponent* TargetHealth = Actor->FindComponentByClass<UDroneHealthComponent>();
		TargetHealth && TargetHealth->IsDead())
	{
		HandleDetectedDroneDestroyed(Actor);
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		// 순간 가림 뒤 같은 Drone을 다시 본 경우 Lost Event를 만들지 않는다.
		CancelPendingDroneLost();
		const bool bWasAlreadyDetected = DetectedDrone.Get() == Actor;
		DetectedDrone = Actor;
		LastKnownDroneLocation = Actor->GetActorLocation();
		bHasLastKnownDroneLocation = true;
		if (!bWasAlreadyDetected)
		{
			// 순찰·대기 Slot을 붙잡은 채 전투로 넘어가지 않도록 첫 감지에서만 해제한다.
			// 같은 Target의 반복 자극이 MG 이동·Claim을 취소하지 않게 한다.
			EnterDroneDetectedResponse();
			++DroneDetectionCount;
			if (StateTreeAIComponent->IsRunning())
			{
				StateTreeAIComponent->SendStateTreeEvent(DroneAITags::Event_DroneDetected);
			}
			OnDronePerceptionChanged.Broadcast(Actor, true);
		}
	}
	else if (DetectedDrone.Get() == Actor)
	{
		QueueDroneLostConfirmation(Actor);
	}
}

void ADroneNPCAIController::QueueDroneLostConfirmation(AActor* Actor)
{
	if (!Actor || DetectedDrone.Get() != Actor || ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return;
	}

	LastKnownDroneLocation = Actor->GetActorLocation();
	bHasLastKnownDroneLocation = true;

	// 같은 실패 자극이 여러 번 와도 Timer 하나만 유지한다.
	if (PendingLostDrone.Get() == Actor && GetWorldTimerManager().IsTimerActive(DroneLostGraceTimerHandle))
	{
		return;
	}

	CancelPendingDroneLost();
	PendingLostDrone = Actor;
	if (DroneSightLossGracePeriod <= 0.0f)
	{
		ConfirmPendingDroneLost();
		return;
	}

	GetWorldTimerManager().SetTimer(
		DroneLostGraceTimerHandle,
		this,
		&ADroneNPCAIController::ConfirmPendingDroneLost,
		DroneSightLossGracePeriod,
		false);
}

void ADroneNPCAIController::CancelPendingDroneLost()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DroneLostGraceTimerHandle);
	}
	PendingLostDrone.Reset();
}

void ADroneNPCAIController::ConfirmPendingDroneLost()
{
	AActor* LostActor = PendingLostDrone.Get();
	if (!LostActor || DetectedDrone.Get() != LostActor || ResponseState == EDroneNPCAIResponseState::Dead)
	{
		CancelPendingDroneLost();
		return;
	}

	// 성공 Sight Callback은 이 Timer보다 먼저/나중 어느 순서로 오더라도
	// HandleTargetPerceptionUpdated에서 Timer를 취소하므로 여기서는 보류된 대상만 확정한다.
	CancelPendingDroneLost();
	LastKnownDroneLocation = LostActor->GetActorLocation();
	bHasLastKnownDroneLocation = true;
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	++DroneLostCount;
	if (StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->SendStateTreeEvent(DroneAITags::Event_DroneLost);
	}
	OnDronePerceptionChanged.Broadcast(LostActor, false);
}

bool ADroneNPCAIController::HasActiveDroneLookTarget() const
{
	return ResponseState != EDroneNPCAIResponseState::Dead
		&& IsHostileNPC()
		&& (DetectedDrone.IsValid()
			|| (ResponseState == EDroneNPCAIResponseState::Search && bHasLastKnownDroneLocation));
}

bool ADroneNPCAIController::GetReservedMGTurretOperatorTransform(FTransform& OutOperatorTransform) const
{
	OutOperatorTransform = FTransform::Identity;
	if (!ReservationComponent || !ReservationComponent->HasValidReservation())
	{
		return false;
	}

	const ADroneMGTurretStation* Turret = Cast<ADroneMGTurretStation>(
		ReservationComponent->GetReservedSmartObjectActor());
	return Turret
		? (OutOperatorTransform = Turret->GetMGTurretOperatorTransform(), true)
		: false;
}

bool ADroneNPCAIController::AlignPawnToMGTurretOperator()
{
	APawn* ControlledPawn = GetPawn();
	FTransform OperatorTransform;
	if (!ControlledPawn || !GetReservedMGTurretOperatorTransform(OperatorTransform))
	{
		return false;
	}

	// OperatorAnchor는 발이 놓일 지점이다. Character Actor 원점은 Capsule 중앙이므로
	// 절반 높이만큼 올려 정확히 포탑 뒤 바닥에 선다.
	FVector PawnLocation = OperatorTransform.GetLocation();
	if (const ACharacter* CharacterPawn = Cast<ACharacter>(ControlledPawn))
	{
		if (const UCapsuleComponent* Capsule = CharacterPawn->GetCapsuleComponent())
		{
			PawnLocation += FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	else
	{
		PawnLocation.Z = ControlledPawn->GetActorLocation().Z;
	}

	const FRotator OperatorFacing(0.0f, OperatorTransform.Rotator().Yaw, 0.0f);
	ControlledPawn->SetActorLocationAndRotation(
		PawnLocation,
		OperatorFacing,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetControlRotation(OperatorFacing);
	return true;
}

void ADroneNPCAIController::UpdatePersonalWeaponFacing(const float DeltaSeconds)
{
	const bool bUsesPersonalWeaponResponse = ResponseState == EDroneNPCAIResponseState::DroneDetected
		|| ResponseState == EDroneNPCAIResponseState::UseCover;
	APawn* ControlledPawn = GetPawn();
	AActor* CurrentDrone = DetectedDrone.Get();
	if (!bFaceDroneDuringPersonalWeaponResponse
		|| !bUsesPersonalWeaponResponse
		|| !ControlledPawn
		|| !CurrentDrone)
	{
		return;
	}

	FVector ToDrone = CurrentDrone->GetActorLocation() - ControlledPawn->GetActorLocation();
	ToDrone.Z = 0.0f;
	if (ToDrone.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentFacing = ControlledPawn->GetActorRotation();
	const float DesiredYaw = ToDrone.Rotation().Yaw;
	const float NewYaw = FMath::FixedTurn(
		CurrentFacing.Yaw,
		DesiredYaw,
		FMath::Max(0.0f, PersonalWeaponFacingTurnSpeedDegreesPerSecond) * DeltaSeconds);
	const FRotator NewFacing(0.0f, NewYaw, 0.0f);
	SetControlRotation(NewFacing);
	ControlledPawn->SetActorRotation(NewFacing, ETeleportType::None);
}

void ADroneNPCAIController::UpdateDroneGaze(const float DeltaSeconds)
{
	APawn* ControlledPawn = GetPawn();
	FVector LookTargetLocation = FVector::ZeroVector;
	bool bHasTarget = false;

	if (ResponseState != EDroneNPCAIResponseState::Dead && IsHostileNPC() && ControlledPawn)
	{
		if (AActor* CurrentDrone = DetectedDrone.Get())
		{
			LookTargetLocation = CurrentDrone->GetActorLocation();
			LastKnownDroneLocation = LookTargetLocation;
			bHasLastKnownDroneLocation = true;
			bHasTarget = true;
		}
		else if (ResponseState == EDroneNPCAIResponseState::Search && bHasLastKnownDroneLocation)
		{
			LookTargetLocation = LastKnownDroneLocation;
			bHasTarget = true;
		}
	}

	FRotator DesiredLookRotation = FRotator::ZeroRotator;
	if (bHasTarget)
	{
		const FVector LookDirection = LookTargetLocation - ControlledPawn->GetPawnViewLocation();
		if (!LookDirection.IsNearlyZero())
		{
			DesiredLookRotation = LookDirection.Rotation() - ControlledPawn->GetActorRotation();
			DesiredLookRotation.Normalize();
			DesiredLookRotation.Yaw = FMath::Clamp(
				DesiredLookRotation.Yaw,
				-MaxDroneLookYawDegrees,
				MaxDroneLookYawDegrees);
			DesiredLookRotation.Pitch = FMath::Clamp(
				DesiredLookRotation.Pitch,
				-MaxDroneLookPitchDownDegrees,
				MaxDroneLookPitchUpDegrees);
			DesiredLookRotation.Roll = 0.0f;
		}
	}

	const float InterpolationSpeed = bHasTarget
		? DroneLookTrackingInterpolationSpeed
		: DroneLookReturnInterpolationSpeed;
	SmoothedDroneLookRotation.Pitch = FMath::FInterpTo(
		SmoothedDroneLookRotation.Pitch,
		DesiredLookRotation.Pitch,
		DeltaSeconds,
		InterpolationSpeed);
	SmoothedDroneLookRotation.Yaw = FMath::FInterpTo(
		SmoothedDroneLookRotation.Yaw,
		DesiredLookRotation.Yaw,
		DeltaSeconds,
		InterpolationSpeed);
	SmoothedDroneLookRotation.Roll = 0.0f;
	DroneLookAlpha = FMath::FInterpTo(
		DroneLookAlpha,
		bHasTarget ? 1.0f : 0.0f,
		DeltaSeconds,
		InterpolationSpeed);

	if (!bHasTarget && DroneLookAlpha <= KINDA_SMALL_NUMBER)
	{
		DroneLookAlpha = 0.0f;
		if (SmoothedDroneLookRotation.IsNearlyZero(0.01f))
		{
			SmoothedDroneLookRotation = FRotator::ZeroRotator;
		}
	}
}

void ADroneNPCAIController::ClearDroneGameplayFocus()
{
	ClearFocus(EAIFocusPriority::Gameplay);
}

UDroneNPCProfileComponent* ADroneNPCAIController::GetPossessedProfile() const
{
	return GetPawn() ? GetPawn()->FindComponentByClass<UDroneNPCProfileComponent>() : nullptr;
}
