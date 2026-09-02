#include "AI/DroneNPCAIController.h"

#include "AI/DroneAITags.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/DroneSmartObjectReservationComponent.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Prototype/DronePrototypePawn.h"
#include "StateTree.h"

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

void ADroneNPCAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	DetectedDrone.Reset();
	ResponseState = EDroneNPCAIResponseState::Patrol;
	bHasLastKnownDroneLocation = false;
	LastKnownDroneLocation = FVector::ZeroVector;
	DroneDetectionCount = 0;
	DroneLostCount = 0;
	DroneSearchStartCount = 0;
	CompletedDroneSearchCount = 0;
	MGTurretClaimCount = 0;
	MGTurretArrivalCount = 0;

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
	if ((IsHostileNPC() || IsFriendlyNPC())
		&& StateTreeAIComponent
		&& !StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->StartLogic();
	}
}

void ADroneNPCAIController::OnUnPossess()
{
	StopPersonalWeaponFire();
	StopMovement();
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
	return IsHostileNPC()
		&& TargetActor
		&& WeaponComponent
		&& WeaponComponent->CanFire(TargetActor, TargetActor->GetActorLocation());
}

bool ADroneNPCAIController::StartPersonalWeaponFire()
{
	AActor* TargetActor = GetDetectedDrone();
	UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent();
	return IsHostileNPC()
		&& TargetActor
		&& WeaponComponent
		&& WeaponComponent->StartFire(TargetActor, TargetActor->GetActorLocation());
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
	return IsHostileNPC() && WeaponComponent && WeaponComponent->Reload();
}

void ADroneNPCAIController::EnterDroneDetectedResponse()
{
	if (!IsHostileNPC())
	{
		return;
	}

	ResponseState = EDroneNPCAIResponseState::DroneDetected;
	StopMovement();
	ReservationComponent->ReleaseReservation();
}

bool ADroneNPCAIController::BeginDroneSearch(const float AcceptanceRadius)
{
	if (!IsHostileNPC() || HasDetectedDrone() || !bHasLastKnownDroneLocation || !GetPawn())
	{
		return false;
	}

	ResponseState = EDroneNPCAIResponseState::Search;
	++DroneSearchStartCount;
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
	++CompletedDroneSearchCount;
	ResponseState = EDroneNPCAIResponseState::Patrol;
	ConfigureDefaultPatrolActivities();
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

bool ADroneNPCAIController::PrepareMGTurretSearch()
{
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

	ResponseState = EDroneNPCAIResponseState::MoveToMGTurret;
	++MGTurretClaimCount;
	return true;
}

bool ADroneNPCAIController::CompleteMGTurretMove()
{
	if (ResponseState != EDroneNPCAIResponseState::MoveToMGTurret
		|| !HasDetectedDrone()
		|| !ReservationComponent->HasValidReservation())
	{
		return false;
	}

	StopMovement();
	ResponseState = EDroneNPCAIResponseState::HoldMGTurret;
	++MGTurretArrivalCount;
	return true;
}

void ADroneNPCAIController::AbortMGTurretResponse()
{
	StopMovement();
	ReservationComponent->ReleaseReservation();
	ConfigureDefaultPatrolActivities();
	ResponseState = HasDetectedDrone()
		? EDroneNPCAIResponseState::DroneDetected
		: EDroneNPCAIResponseState::Patrol;
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

	if (Stimulus.WasSuccessfullySensed())
	{
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
		LastKnownDroneLocation = Actor->GetActorLocation();
		bHasLastKnownDroneLocation = true;
		StopPersonalWeaponFire();
		StopMovement();
		ReservationComponent->ReleaseReservation();
		DetectedDrone.Reset();
		++DroneLostCount;
		if (StateTreeAIComponent->IsRunning())
		{
			StateTreeAIComponent->SendStateTreeEvent(DroneAITags::Event_DroneLost);
		}
		OnDronePerceptionChanged.Broadcast(Actor, false);
	}
}

UDroneNPCProfileComponent* ADroneNPCAIController::GetPossessedProfile() const
{
	return GetPawn() ? GetPawn()->FindComponentByClass<UDroneNPCProfileComponent>() : nullptr;
}
