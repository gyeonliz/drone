#include "AI/DroneNPCAIController.h"

#include "AI/DroneAITags.h"
#include "AI/DroneNPCEngagementPolicy.h"
#include "AI/DroneMGTurretStation.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/DroneSmartObjectReservationComponent.h"
#include "AI/DroneSmartObjectStation.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Health/DroneHealthComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
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
	RefreshDroneSightTuning();
	DronePerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
	DronePerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
		this,
		&ADroneNPCAIController::HandleTargetPerceptionUpdated);
}

void ADroneNPCAIController::BeginPlay()
{
	Super::BeginPlay();
	// Native Constructor 뒤 적용된 파생 Controller BP Class Defaults를 실제 Sense에 반영한다.
	RefreshDroneSightTuning();

	// UWorldSubsystem::OnWorldBeginPlay 뒤라 Smart Object Runtime 조회가 안전하다.
	// 레벨에 미리 배치된 Controller는 OnPossess에서 Asset만 지정하고 여기서 실행한다.
	TryStartAssignedStateTree();
}

void ADroneNPCAIController::RefreshDroneSightTuning()
{
	if (!SightConfig || !DronePerceptionComponent)
	{
		return;
	}

	SightConfig->SightRadius = FMath::Max(1.0f, DroneSightRadius);
	SightConfig->LoseSightRadius = FMath::Max(SightConfig->SightRadius, DroneLoseSightRadius);
	SightConfig->PeripheralVisionAngleDegrees = FMath::Clamp(DronePeripheralVisionAngleDegrees, 0.0f, 180.0f);
	SightConfig->SetMaxAge(FMath::Max(0.0f, DroneSightStimulusMaxAgeSeconds));
	SightConfig->DetectionByAffiliation.bDetectEnemies = bDroneSightDetectEnemies;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = bDroneSightDetectFriendlies;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = bDroneSightDetectNeutrals;
	DronePerceptionComponent->ConfigureSense(*SightConfig);
	if (HasActorBegunPlay())
	{
		DronePerceptionComponent->RequestStimuliListenerUpdate();
	}
}

void ADroneNPCAIController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePersonalWeaponDisengageCooldown(DeltaSeconds);
	UpdateMGTurretReassignmentRetry(DeltaSeconds);
	// StateTree의 MG 진입 Task는 한 번만 호출될 수 있다. 이미 시선 보간을 위해
	// 동작하는 Controller Tick에서 실제 점유자 한 명의 포탑 조준만 계속 갱신한다.
	if (ResponseState == EDroneNPCAIResponseState::UseMGTurret)
	{
		UpdateMGTurretOperation();
	}
	else
	{
		if (ResponseState == EDroneNPCAIResponseState::DroneDetected
			|| ResponseState == EDroneNPCAIResponseState::PursueDrone)
		{
			UpdatePersonalWeaponEngagement(DeltaSeconds);
		}
		UpdatePursuitFacing(DeltaSeconds);
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
	SetResponseState(EDroneNPCAIResponseState::Patrol, true);
	bHasLastKnownDroneLocation = false;
	LastKnownDroneLocation = FVector::ZeroVector;
	DroneDetectionCount = 0;
	DroneLostCount = 0;
	DroneSearchStartCount = 0;
	CompletedDroneSearchCount = 0;
	DroneDestroyedResponseCount = 0;
	ResetPersonalWeaponEngagement();
	PersonalWeaponPursuitStartCount = 0;
	PersonalWeaponPursuitMoveRequestCount = 0;
	PersonalWeaponDisengageCount = 0;
	MGTurretClaimCount = 0;
	MGTurretArrivalCount = 0;
	MGTurretUseCount = 0;
	ActiveMGTurretStation.Reset();
	ClearMGTurretReassignmentRetry();
	CoverClaimCount = 0;
	CoverUseCount = 0;
	SmoothedDroneLookRotation = FRotator::ZeroRotator;
	DroneLookAlpha = 0.0f;
	bPersonalWeaponFacingTurnActive = false;

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
	ClearMGTurretReassignmentRetry();
	ResetPersonalWeaponEngagement();
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	SetResponseState(EDroneNPCAIResponseState::Patrol, true);
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

float ADroneNPCAIController::GetResponseStateElapsedSeconds() const
{
	const UWorld* World = GetWorld();
	return World
		? FMath::Max(0.0f, World->GetTimeSeconds() - ResponseStateEnteredWorldTimeSeconds)
		: 0.0f;
}

bool ADroneNPCAIController::HasSatisfiedMinimumResponseStateDuration() const
{
	// World가 없는 CDO/종료 구간은 StateTree 실행 대상이 아니므로 전환을 막지 않는다.
	return !GetWorld()
		|| ResponseState == EDroneNPCAIResponseState::Dead
		|| MinimumResponseStateDurationSeconds <= 0.0f
		|| GetResponseStateElapsedSeconds() >= MinimumResponseStateDurationSeconds;
}

bool ADroneNPCAIController::MaintainCurrentResponseStateAction()
{
	switch (ResponseState)
	{
	case EDroneNPCAIResponseState::Patrol:
		return !HasDetectedDrone();

	case EDroneNPCAIResponseState::DroneDetected:
	case EDroneNPCAIResponseState::PursueDrone:
		// Controller Tick owns personal-weapon time, fire and movement updates. MG retry
		// maintenance can run earlier in that same Tick; do not count its DeltaSeconds twice.
		return IsHostileNPC() && GetPawn() && HasDetectedDrone();

	case EDroneNPCAIResponseState::MoveToMGTurret:
	case EDroneNPCAIResponseState::HoldMGTurret:
		return HasDetectedDrone()
			&& ReservationComponent
			&& ReservationComponent->HasValidReservation();

	case EDroneNPCAIResponseState::UseMGTurret:
		return UpdateMGTurretOperation();

	case EDroneNPCAIResponseState::MoveToCover:
		return HasDetectedDrone()
			&& ReservationComponent
			&& ReservationComponent->HasValidReservation();

	case EDroneNPCAIResponseState::UseCover:
		return UpdateCoverResponse();

	case EDroneNPCAIResponseState::Search:
		return !HasDetectedDrone() && bHasLastKnownDroneLocation;

	case EDroneNPCAIResponseState::Dead:
	default:
		return false;
	}
}

void ADroneNPCAIController::SetResponseState(
	const EDroneNPCAIResponseState NewState,
	const bool bRestartDuration)
{
	if (!bRestartDuration && ResponseState == NewState)
	{
		return;
	}

	ResponseState = NewState;
	ResponseStateEnteredWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
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

float ADroneNPCAIController::GetPersonalWeaponRange() const
{
	const UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent();
	if (!WeaponComponent)
	{
		return 0.0f;
	}
	if (UsesShotgun())
	{
		return WeaponComponent->GetShotgunRange();
	}
	if (UsesRifle())
	{
		return WeaponComponent->GetRifleRange();
	}
	return 0.0f;
}

bool ADroneNPCAIController::IsPersonalWeaponPursuitMoveActive() const
{
	const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
	return MoveStatus == EPathFollowingStatus::Moving
		|| MoveStatus == EPathFollowingStatus::Paused;
}

bool ADroneNPCAIController::UpdatePersonalWeaponEngagement(const float DeltaSeconds)
{
	APawn* ControlledPawn = GetPawn();
	AActor* TargetActor = GetDetectedDrone();
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| !IsHostileNPC()
		|| !ControlledPawn
		|| !TargetActor)
	{
		return false;
	}

	if (!bHasPersonalWeaponCombatOrigin)
	{
		PersonalWeaponCombatOrigin = ControlledPawn->GetActorLocation();
		bHasPersonalWeaponCombatOrigin = true;
	}

	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const float WeaponRange = GetPersonalWeaponRange();
	const EDroneNPCEngagementDecision Decision = FDroneNPCEngagementPolicy::Evaluate(
		bHasPersonalWeaponCombatOrigin,
		PersonalWeaponCombatOrigin,
		PawnLocation,
		TargetLocation,
		WeaponRange,
		PersonalWeaponCombatLeashRadius);

	if (Decision == EDroneNPCEngagementDecision::Disengage)
	{
		PersonalWeaponOutOfRangeElapsedSeconds = 0.0f;
		DisengagePersonalWeaponTarget();
		return false;
	}

	if (Decision == EDroneNPCEngagementDecision::Fire)
	{
		// 실제 사거리 안에 들어오는 즉시 추적을 끝낸다. 공간 Hysteresis 때문에
		// 사거리 안에서도 계속 달려드는 동작을 만들지 않는다.
		PersonalWeaponOutOfRangeElapsedSeconds = 0.0f;
		StopMovement();
		// PathFollowing 정지와 CharacterMovement 감속은 서로 다른 단계다. 직전
		// Pursue 속도가 남은 채 조준 Yaw를 적용하면 잠깐 뒤로 걷거나 옆으로
		// 미끄러지며 쏘는 모습이 생기므로, 실제 사격 상태에서는 수평 속도도 즉시 제거한다.
		if (ACharacter* CharacterPawn = Cast<ACharacter>(ControlledPawn))
		{
			if (UCharacterMovementComponent* Movement = CharacterPawn->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}
		SetResponseState(EDroneNPCAIResponseState::DroneDetected);
		PersonalWeaponPursuitNoProgressSeconds = 0.0f;
		PersonalWeaponPursuitRepathRemainingSeconds = 0.0f;
		LastPersonalWeaponPursuitDistance = 0.0f;
		PersonalWeaponPursuitNavigationDestination = FVector::ZeroVector;
		bHasPersonalWeaponPursuitNavigationDestination = false;
		if (const UDroneNPCWeaponComponent* WeaponComponent = GetPossessedWeaponComponent();
			WeaponComponent && WeaponComponent->IsFiring())
		{
			return true;
		}
		return StartPersonalWeaponFire();
	}

	StopPersonalWeaponFire();
	if (ResponseState != EDroneNPCAIResponseState::PursueDrone)
	{
		// 경계에서 한두 프레임만 사거리 밖으로 튀는 값은 상태 전환으로 만들지 않는다.
		// 실제로 계속 밖에 있을 때만 Pursue에 진입하며, 안쪽 복귀는 위에서 즉시 처리한다.
		PersonalWeaponOutOfRangeElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
		if (PersonalWeaponOutOfRangeElapsedSeconds
			< FMath::Max(0.0f, PersonalWeaponOutOfRangeConfirmationSeconds))
		{
			StopMovement();
			return true;
		}
	}
	const float CurrentDistance = FVector::Dist(PawnLocation, TargetLocation);
	if (ResponseState != EDroneNPCAIResponseState::PursueDrone)
	{
		SetResponseState(EDroneNPCAIResponseState::PursueDrone);
		PersonalWeaponOutOfRangeElapsedSeconds = 0.0f;
		PersonalWeaponPursuitNoProgressSeconds = 0.0f;
		PersonalWeaponPursuitRepathRemainingSeconds = 0.0f;
		LastPersonalWeaponPursuitDistance = CurrentDistance;
		PersonalWeaponPursuitNavigationDestination = FVector::ZeroVector;
		bHasPersonalWeaponPursuitNavigationDestination = false;
		++PersonalWeaponPursuitStartCount;
	}
	else if (CurrentDistance + FMath::Max(0.0f, PersonalWeaponPursuitProgressTolerance)
		< LastPersonalWeaponPursuitDistance)
	{
		PersonalWeaponPursuitNoProgressSeconds = 0.0f;
		LastPersonalWeaponPursuitDistance = CurrentDistance;
	}
	else
	{
		PersonalWeaponPursuitNoProgressSeconds += FMath::Max(0.0f, DeltaSeconds);
	}

	if (PersonalWeaponPursuitNoProgressSeconds
		>= FMath::Max(0.1f, PersonalWeaponPursuitNoProgressTimeoutSeconds))
	{
		DisengagePersonalWeaponTarget();
		return false;
	}

	PersonalWeaponPursuitRepathRemainingSeconds -= FMath::Max(0.0f, DeltaSeconds);
	if (PersonalWeaponPursuitRepathRemainingSeconds <= 0.0f)
	{
		const float AcceptanceRadius = FMath::Max(
			100.0f,
			WeaponRange * FMath::Clamp(PersonalWeaponPursuitRangeRatio, 0.1f, 0.95f));
		FNavLocation ProjectedTarget;
		const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		const bool bProjectedToNavigation = NavigationSystem
			&& NavigationSystem->ProjectPointToNavigation(
				TargetLocation,
				ProjectedTarget,
				PersonalWeaponPursuitNavigationProjectionExtent.GetAbs());
		EPathFollowingRequestResult::Type MoveResult = EPathFollowingRequestResult::Failed;
		bool bRequestedMove = false;
		if (bProjectedToNavigation)
		{
			const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
			const bool bMoveInProgress = MoveStatus == EPathFollowingStatus::Moving
				|| MoveStatus == EPathFollowingStatus::Paused;
			bRequestedMove = FDroneNPCEngagementPolicy::ShouldRequestPursuitMove(
				bHasPersonalWeaponPursuitNavigationDestination,
				PersonalWeaponPursuitNavigationDestination,
				ProjectedTarget.Location,
				PersonalWeaponPursuitRepathDistance,
				bMoveInProgress);
			if (bRequestedMove)
			{
				MoveResult = MoveToLocation(
					ProjectedTarget.Location,
					AcceptanceRadius,
					true,
					true,
					true,
					false,
					nullptr,
					true);
				++PersonalWeaponPursuitMoveRequestCount;
				if (MoveResult != EPathFollowingRequestResult::Failed)
				{
					PersonalWeaponPursuitNavigationDestination = ProjectedTarget.Location;
					bHasPersonalWeaponPursuitNavigationDestination = true;
				}
			}
		}
		PersonalWeaponPursuitRepathRemainingSeconds = FMath::Max(
			0.05f,
			PersonalWeaponPursuitRepathIntervalSeconds);
		if (!bProjectedToNavigation || (bRequestedMove && MoveResult == EPathFollowingRequestResult::Failed))
		{
			bHasPersonalWeaponPursuitNavigationDestination = false;
			PersonalWeaponPursuitNoProgressSeconds += FMath::Max(
				0.05f,
				PersonalWeaponPursuitRepathIntervalSeconds);
		}
	}
	return true;
}

void ADroneNPCAIController::ResetPersonalWeaponEngagement(const bool bClearIgnoredDrone)
{
	bHasPersonalWeaponCombatOrigin = false;
	PersonalWeaponCombatOrigin = FVector::ZeroVector;
	PersonalWeaponPursuitNoProgressSeconds = 0.0f;
	PersonalWeaponPursuitRepathRemainingSeconds = 0.0f;
	PersonalWeaponOutOfRangeElapsedSeconds = 0.0f;
	LastPersonalWeaponPursuitDistance = 0.0f;
	PersonalWeaponPursuitNavigationDestination = FVector::ZeroVector;
	bHasPersonalWeaponPursuitNavigationDestination = false;
	if (bClearIgnoredDrone)
	{
		IgnoredDisengagedDrone.Reset();
		IgnoredDisengageCombatOrigin = FVector::ZeroVector;
		bHasIgnoredDisengageCombatOrigin = false;
		PersonalWeaponDisengageCooldownRemainingSeconds = 0.0f;
	}
}

void ADroneNPCAIController::DisengagePersonalWeaponTarget()
{
	AActor* AbandonedTarget = DetectedDrone.Get();
	if (!AbandonedTarget || ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return;
	}
	IgnoredDisengagedDrone = AbandonedTarget;
	IgnoredDisengageCombatOrigin = bHasPersonalWeaponCombatOrigin
		? PersonalWeaponCombatOrigin
		: (GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector);
	bHasIgnoredDisengageCombatOrigin = true;
	PersonalWeaponDisengageCooldownRemainingSeconds = FMath::Max(
		0.0f,
		PersonalWeaponDisengageCooldownSeconds);
	CancelPendingDroneLost();
	ClearMGTurretReassignmentRetry();
	ClearDroneGameplayFocus();
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	bHasLastKnownDroneLocation = false;
	LastKnownDroneLocation = FVector::ZeroVector;
	ResetPersonalWeaponEngagement(false);
	SetResponseState(EDroneNPCAIResponseState::Patrol);
	ConfigureDefaultPatrolActivities();
	++PersonalWeaponDisengageCount;

	if (DronePerceptionComponent)
	{
		DronePerceptionComponent->ForgetActor(AbandonedTarget);
	}
	if (StateTreeAIComponent && StateTreeAIComponent->IsRunning())
	{
		// Pursuit는 Detected State 안에서 Controller가 수행한다. 리시 포기 후 Lost/Search
		// 전환을 우회하면 방금 끝난 MoveTo Task가 남을 수 있다. 다만 StateTree Task
		// 실행 중 동기 Restart하면 StartTree 재진입 오류가 나므로 다음 Tick에 정리한다.
		GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ADroneNPCAIController::RestartStateTreeAfterPersonalWeaponDisengage);
	}
	else
	{
		TryStartAssignedStateTree();
	}
	OnDronePerceptionChanged.Broadcast(AbandonedTarget, false);
}

void ADroneNPCAIController::RestartStateTreeAfterPersonalWeaponDisengage()
{
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| HasDetectedDrone()
		|| !GetPawn()
		|| !StateTreeAIComponent)
	{
		return;
	}

	if (StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->RestartLogic();
	}
	else
	{
		TryStartAssignedStateTree();
	}
}

void ADroneNPCAIController::UpdatePersonalWeaponDisengageCooldown(const float DeltaSeconds)
{
	if (!IgnoredDisengagedDrone.IsValid())
	{
		IgnoredDisengageCombatOrigin = FVector::ZeroVector;
		bHasIgnoredDisengageCombatOrigin = false;
		PersonalWeaponDisengageCooldownRemainingSeconds = 0.0f;
		return;
	}

	PersonalWeaponDisengageCooldownRemainingSeconds -= FMath::Max(0.0f, DeltaSeconds);
	if (PersonalWeaponDisengageCooldownRemainingSeconds > 0.0f)
	{
		return;
	}
	if (bHasIgnoredDisengageCombatOrigin)
	{
		const float ReturnRadius = FMath::Max(100.0f, PersonalWeaponCombatLeashRadius)
			* FMath::Clamp(PersonalWeaponDisengageReturnRadiusRatio, 0.1f, 1.0f);
		if (FVector::DistSquared2D(
				IgnoredDisengageCombatOrigin,
				IgnoredDisengagedDrone->GetActorLocation()) > FMath::Square(ReturnRadius))
		{
			// 표적이 리시 안으로 실제 복귀할 때까지 순찰을 유지한다.
			return;
		}
	}

	IgnoredDisengagedDrone.Reset();
	IgnoredDisengageCombatOrigin = FVector::ZeroVector;
	bHasIgnoredDisengageCombatOrigin = false;
	PersonalWeaponDisengageCooldownRemainingSeconds = 0.0f;
	if (DronePerceptionComponent)
	{
		DronePerceptionComponent->RequestStimuliListenerUpdate();
	}
}

void ADroneNPCAIController::EnterDroneDetectedResponse()
{
	if (!IsHostileNPC() || ResponseState == EDroneNPCAIResponseState::Dead)
	{
		return;
	}

	StopMGTurretOperation();
	if (!bHasPersonalWeaponCombatOrigin)
	{
		if (const APawn* ControlledPawn = GetPawn())
		{
			PersonalWeaponCombatOrigin = ControlledPawn->GetActorLocation();
			bHasPersonalWeaponCombatOrigin = true;
		}
	}
	SetResponseState(EDroneNPCAIResponseState::DroneDetected);
	StopMovement();
	if (ACharacter* CharacterPawn = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* Movement = CharacterPawn->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}
	ReservationComponent->ReleaseReservation();
}

bool ADroneNPCAIController::BeginDroneSearch(const float AcceptanceRadius)
{
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| !IsHostileNPC() || HasDetectedDrone() || !bHasLastKnownDroneLocation || !GetPawn())
	{
		return false;
	}

	SetResponseState(EDroneNPCAIResponseState::Search);
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
	if (ACharacter* CharacterPawn = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* Movement = CharacterPawn->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}
	ClearDroneGameplayFocus();
	++CompletedDroneSearchCount;
	SetResponseState(EDroneNPCAIResponseState::Patrol);
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
	ClearMGTurretReassignmentRetry();
	ClearDroneGameplayFocus();
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	ResetPersonalWeaponEngagement();
	bHasLastKnownDroneLocation = false;
	SetResponseState(EDroneNPCAIResponseState::Dead);

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
			|| OtherState == EDroneNPCAIResponseState::PursueDrone
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

		OtherController->BeginMGTurretReassignmentRetry();
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
	ClearMGTurretReassignmentRetry();
	ClearDroneGameplayFocus();
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	ResetPersonalWeaponEngagement();
	bHasLastKnownDroneLocation = false;
	LastKnownDroneLocation = FVector::ZeroVector;
	SetResponseState(EDroneNPCAIResponseState::Patrol);
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

	SetResponseState(EDroneNPCAIResponseState::MoveToMGTurret);
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
	SetResponseState(EDroneNPCAIResponseState::HoldMGTurret);
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
	SetResponseState(EDroneNPCAIResponseState::UseMGTurret);
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
	SetResponseState(HasDetectedDrone()
		? EDroneNPCAIResponseState::DroneDetected
		: EDroneNPCAIResponseState::Patrol);
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
		SetResponseState(EDroneNPCAIResponseState::DroneDetected);
		StartPersonalWeaponFire();
		return false;
	}

	StopPersonalWeaponFire();
	SetResponseState(EDroneNPCAIResponseState::MoveToCover);
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
	SetResponseState(EDroneNPCAIResponseState::UseCover);
	++CoverUseCount;
	// Slot 점유 전환 자체가 성공했다면 UseCover 진입은 유지한다. 첫 사격 시작이 같은
	// 프레임에 실패해도 UseCover Task가 최소 유지시간 동안 탄약·표적 조건을 재확인한다.
	StartPersonalWeaponFire();
	return true;
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
	SetResponseState(HasDetectedDrone()
		? EDroneNPCAIResponseState::DroneDetected
		: EDroneNPCAIResponseState::Patrol);
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

	// 리시 초과나 경로 정체로 방금 포기한 같은 표적을 즉시 다시 감지하면
	// Patrol과 Pursuit가 매 프레임 왕복한다. Cooldown 동안 그 자극만 무시한다.
	if (IgnoredDisengagedDrone.Get() == Actor)
	{
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
	ClearMGTurretReassignmentRetry();
	LastKnownDroneLocation = LostActor->GetActorLocation();
	bHasLastKnownDroneLocation = true;
	StopPersonalWeaponFire();
	StopMovement();
	StopMGTurretOperation();
	ReservationComponent->ReleaseReservation();
	DetectedDrone.Reset();
	ResetPersonalWeaponEngagement();
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
		bPersonalWeaponFacingTurnActive = false;
		return;
	}
	// 개인화기 몸 Yaw는 정지 사격/엄폐에서만 담당한다. 실제 이동 중에는
	// PursuitFacing 또는 CharacterMovement가 유일한 몸 방향 소유자여야 한다.
	if (const ACharacter* CharacterPawn = Cast<ACharacter>(ControlledPawn))
	{
		if (const UCharacterMovementComponent* Movement = CharacterPawn->GetCharacterMovement();
			Movement && Movement->Velocity.SizeSquared2D() > FMath::Square(20.0f))
		{
			bPersonalWeaponFacingTurnActive = false;
			return;
		}
	}

	FVector ToDrone = CurrentDrone->GetActorLocation() - ControlledPawn->GetActorLocation();
	ToDrone.Z = 0.0f;
	if (ToDrone.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentFacing = ControlledPawn->GetActorRotation();
	const float DesiredYaw = ToDrone.Rotation().Yaw;
	const float YawError = FMath::FindDeltaAngleDegrees(CurrentFacing.Yaw, DesiredYaw);
	const float StopThresholdDegrees = GetPersonalWeaponFacingDeadZoneDegrees();
	const float StartThresholdDegrees = StopThresholdDegrees + GetPersonalWeaponFacingHysteresisDegrees();
	const float AbsoluteYawError = FMath::Abs(YawError);
	if (bPersonalWeaponFacingTurnActive)
	{
		bPersonalWeaponFacingTurnActive = AbsoluteYawError > StopThresholdDegrees;
	}
	else
	{
		bPersonalWeaponFacingTurnActive = AbsoluteYawError > StartThresholdDegrees;
	}
	if (!bPersonalWeaponFacingTurnActive)
	{
		// 시작/정지 문턱을 분리해 경계값을 넘나드는 표적에서도 몸 회전이 깜빡이지 않게 한다.
		return;
	}
	const float NewYaw = FMath::FixedTurn(
		CurrentFacing.Yaw,
		DesiredYaw,
		GetPersonalWeaponFacingTurnSpeedDegreesPerSecond() * DeltaSeconds);
	const FRotator NewFacing(0.0f, NewYaw, 0.0f);
	SetControlRotation(NewFacing);
	ControlledPawn->SetActorRotation(NewFacing, ETeleportType::None);
}

void ADroneNPCAIController::UpdatePursuitFacing(const float DeltaSeconds)
{
	ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn());
	UCharacterMovementComponent* Movement = ControlledCharacter
		? ControlledCharacter->GetCharacterMovement()
		: nullptr;
	if (!ControlledCharacter || !Movement)
	{
		return;
	}

	const bool bUsesVelocityFacing = ResponseState == EDroneNPCAIResponseState::PursueDrone;
	const bool bUsesPersonalWeaponFacing = ResponseState == EDroneNPCAIResponseState::DroneDetected
		|| ResponseState == EDroneNPCAIResponseState::UseCover;
	Movement->bUseControllerDesiredRotation = false;
	// 개인화기 정지 사격/엄폐는 Controller가 몸을 조준 방향으로 직접 돌린다.
	// 이때 CharacterMovement가 이동 방향으로 다시 Yaw를 쓰면 두 회전 소유자가
	// 서로 덮어써 도리도리·옆걸음 포즈가 생길 수 있다.
	Movement->bOrientRotationToMovement = !bUsesVelocityFacing && !bUsesPersonalWeaponFacing;
	if (!bUsesVelocityFacing)
	{
		return;
	}

	const FVector MoveDirection = ControlledCharacter->GetVelocity().GetSafeNormal2D();
	if (MoveDirection.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentFacing = ControlledCharacter->GetActorRotation();
	const float DesiredYaw = MoveDirection.Rotation().Yaw;
	const float NewYaw = FMath::FixedTurn(
		CurrentFacing.Yaw,
		DesiredYaw,
		FMath::Max(1.0f, PursuitFacingTurnSpeedDegreesPerSecond) * FMath::Max(0.0f, DeltaSeconds));
	const FRotator NewFacing(0.0f, NewYaw, 0.0f);
	SetControlRotation(NewFacing);
	ControlledCharacter->SetActorRotation(NewFacing, ETeleportType::None);
}

void ADroneNPCAIController::UpdateDroneGaze(const float DeltaSeconds)
{
	APawn* ControlledPawn = GetPawn();
	FVector LookTargetLocation = FVector::ZeroVector;
	bool bHasTarget = false;
	const bool bCanUpdateCombatGaze = ResponseState == EDroneNPCAIResponseState::DroneDetected
		|| ResponseState == EDroneNPCAIResponseState::PursueDrone
		|| ResponseState == EDroneNPCAIResponseState::MoveToMGTurret
		|| ResponseState == EDroneNPCAIResponseState::HoldMGTurret
		|| ResponseState == EDroneNPCAIResponseState::UseMGTurret
		|| ResponseState == EDroneNPCAIResponseState::MoveToCover
		|| ResponseState == EDroneNPCAIResponseState::UseCover
		|| ResponseState == EDroneNPCAIResponseState::Search;

	if (bCanUpdateCombatGaze && IsHostileNPC() && ControlledPawn)
	{
		if (AActor* CurrentDrone = DetectedDrone.Get())
		{
			const FVector DroneLocation = CurrentDrone->GetActorLocation();
			LastKnownDroneLocation = DroneLocation;
			bHasLastKnownDroneLocation = true;
			if (ResponseState == EDroneNPCAIResponseState::PursueDrone)
			{
				// 추적 중에는 장애물을 피해가는 Nav 이동 방향과 Drone 직선 방향이 다를 수 있다.
				// 몸은 CharacterMovement가 이동 벡터를 따라 돌리고, Bone gaze도 같은 벡터를
				// 보게 해 몸/고개가 서로 다른 방향을 선택하는 도리도리 현상을 막는다.
				FVector PursuitLookDirection = ControlledPawn->GetVelocity().GetSafeNormal2D();
				if (PursuitLookDirection.IsNearlyZero())
				{
					PursuitLookDirection = ControlledPawn->GetActorForwardVector().GetSafeNormal2D();
				}
				LookTargetLocation = ControlledPawn->GetPawnViewLocation()
					+ PursuitLookDirection * 1000.0f;
			}
			else
			{
				LookTargetLocation = DroneLocation;
			}
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
			// 몸은 Hysteresis로 큰 Yaw만 담당하고, 작은 잔여 오차는 보간된 Bone Gaze가
			// 계속 담당한다. 데드존 경계에서 시선을 0도로 Snap하지 않는다.
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

float ADroneNPCAIController::GetPersonalWeaponFacingDeadZoneDegrees() const
{
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	return Profile
		? FMath::Max(0.0f, Profile->GetProfile().PersonalWeaponFacingDeadZoneDegrees)
		: 3.0f;
}

float ADroneNPCAIController::GetPersonalWeaponFacingHysteresisDegrees() const
{
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	return Profile
		? FMath::Max(0.0f, Profile->GetProfile().PersonalWeaponFacingHysteresisDegrees)
		: 3.0f;
}

float ADroneNPCAIController::GetPersonalWeaponFacingTurnSpeedDegreesPerSecond() const
{
	const UDroneNPCProfileComponent* Profile = GetPossessedProfile();
	return Profile
		? FMath::Max(0.0f, Profile->GetProfile().PersonalWeaponFacingTurnSpeedDegreesPerSecond)
		: 180.0f;
}

void ADroneNPCAIController::ClearDroneGameplayFocus()
{
	ClearFocus(EAIFocusPriority::Gameplay);
}

void ADroneNPCAIController::BeginMGTurretReassignmentRetry()
{
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| !CanUseMGTurret()
		|| !HasDetectedDrone())
	{
		return;
	}

	bMGTurretReassignmentRetryPending = true;
	MGTurretReassignmentRetryElapsedSeconds = 0.0f;
	MGTurretReassignmentRetryRemainingSeconds = 0.0f;
	// 현재 Cover/개인화기 행동은 최소 유지시간 동안 계속한다. 실제 재점유 Event를
	// 보내는 프레임에만 사격을 멈춰, 재시도 대기 중 행동이 매 Tick 꺼졌다 켜지지 않게 한다.
}

void ADroneNPCAIController::UpdateMGTurretReassignmentRetry(const float DeltaSeconds)
{
	if (!bMGTurretReassignmentRetryPending)
	{
		return;
	}

	MGTurretReassignmentRetryElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
	if (ResponseState == EDroneNPCAIResponseState::UseMGTurret)
	{
		ClearMGTurretReassignmentRetry();
		return;
	}
	if (ResponseState == EDroneNPCAIResponseState::Dead
		|| !CanUseMGTurret()
		|| !HasDetectedDrone()
		|| MGTurretReassignmentRetryElapsedSeconds > FMath::Max(0.1f, MGTurretReassignmentRetryWindowSeconds))
	{
		ClearMGTurretReassignmentRetry();
		return;
	}

	MGTurretReassignmentRetryRemainingSeconds -= FMath::Max(0.0f, DeltaSeconds);
	if (MGTurretReassignmentRetryRemainingSeconds > 0.0f)
	{
		return;
	}

	const bool bCanInterruptForRetry = ResponseState == EDroneNPCAIResponseState::DroneDetected
		|| ResponseState == EDroneNPCAIResponseState::PursueDrone
		|| ResponseState == EDroneNPCAIResponseState::MoveToCover
		|| ResponseState == EDroneNPCAIResponseState::UseCover;
	if (!bCanInterruptForRetry || !StateTreeAIComponent || !StateTreeAIComponent->IsRunning())
	{
		return;
	}

	if (!HasSatisfiedMinimumResponseStateDuration())
	{
		MaintainCurrentResponseStateAction();
		return;
	}

	// Event 처리와 Claim/Move 실패는 같은 프레임에 끝날 수 있다. 다음 Tick부터 간격을
	// 두고 다시 Event를 보내 Slot 해제 직후 한 번의 경합으로 재점유가 영구 중단되지 않게 한다.
	StopPersonalWeaponFire();
	StateTreeAIComponent->SendStateTreeEvent(DroneAITags::Event_DroneDetected);
	MGTurretReassignmentRetryRemainingSeconds = FMath::Max(0.1f, MGTurretReassignmentRetryIntervalSeconds);
}

void ADroneNPCAIController::ClearMGTurretReassignmentRetry()
{
	bMGTurretReassignmentRetryPending = false;
	MGTurretReassignmentRetryElapsedSeconds = 0.0f;
	MGTurretReassignmentRetryRemainingSeconds = 0.0f;
}

UDroneNPCProfileComponent* ADroneNPCAIController::GetPossessedProfile() const
{
	return GetPawn() ? GetPawn()->FindComponentByClass<UDroneNPCProfileComponent>() : nullptr;
}
