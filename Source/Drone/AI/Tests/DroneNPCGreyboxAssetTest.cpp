#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/DroneAITags.h"
#include "AI/Animation/DroneNPCAnimInstance.h"
#include "AI/DroneMGTurretStation.h"
#include "AI/DroneNPCAIController.h"
#include "AI/DroneNPCCharacter.h"
#include "AI/DroneNPCNavigationFloor.h"
#include "AI/DroneAIStateTreeAuthoringLibrary.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/DroneNPCSpawnPoint.h"
#include "AI/DroneSmartObjectReservationComponent.h"
#include "AI/DroneSmartObjectStation.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Prototype/DronePrototypePawn.h"
#include "Animation/AnimInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/BoxComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Health/DroneHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "StateTree.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace DroneNPCGreybox
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/Lvl_NPCSmartObjectGreybox");
constexpr const TCHAR* MapObjectPath =
	TEXT("/Game/Drone/Maps/Lvl_NPCSmartObjectGreybox.Lvl_NPCSmartObjectGreybox");
constexpr const TCHAR* GameModeClassPath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
constexpr const TCHAR* MannyMeshPath =
	TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
constexpr const TCHAR* UnarmedAnimClassPath =
	TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C");
constexpr const TCHAR* ArmedAnimClassPath =
	TEXT("/Game/Drone/AI/Animation/ABP_NPC_Rifle_Greybox.ABP_NPC_Rifle_Greybox_C");
constexpr const TCHAR* ArmedLocomotionBlendSpacePath =
	TEXT("/Game/Drone/AI/Animation/BS_NPC_Rifle_Locomotion.BS_NPC_Rifle_Locomotion");
constexpr const TCHAR* HostilePatrolStateTreePackage =
	TEXT("/Game/Drone/AI/StateTrees/ST_NPC_HostilePatrol");
constexpr const TCHAR* HostilePatrolStateTreeObjectPath =
	TEXT("/Game/Drone/AI/StateTrees/ST_NPC_HostilePatrol.ST_NPC_HostilePatrol");
constexpr const TCHAR* FriendlyBaseRoutineStateTreePackage =
	TEXT("/Game/Drone/AI/StateTrees/ST_NPC_FriendlyBaseRoutine");
constexpr const TCHAR* FriendlyBaseRoutineStateTreeObjectPath =
	TEXT("/Game/Drone/AI/StateTrees/ST_NPC_FriendlyBaseRoutine.ST_NPC_FriendlyBaseRoutine");

struct FNPCExpectation
{
	const TCHAR* Name;
	EDroneNPCFaction Faction;
	EDroneNPCWeaponType Weapon;
	bool bCanUseMG;
	int32 PlacedCount;
};

const FNPCExpectation NPCExpectations[] =
{
	{TEXT("Hostile_Rifle"), EDroneNPCFaction::Hostile, EDroneNPCWeaponType::Rifle, true, 1},
	{TEXT("Hostile_Shotgun"), EDroneNPCFaction::Hostile, EDroneNPCWeaponType::Shotgun, false, 1},
	{TEXT("Friendly_Base"), EDroneNPCFaction::Friendly, EDroneNPCWeaponType::Unarmed, false, 2},
};

UWorld* FindPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			return Context.World();
		}
	}
	return nullptr;
}

FRequestPlaySessionParams MakePlayParams()
{
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(1);
	Settings->bLaunchSeparateServer = false;
	Settings->AddToRoot();

	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.EditorPlaySettings = Settings;
	Params.bAllowOnlineSubsystem = false;
	return Params;
}

bool HasExpectedActivityTags(const ADroneNPCAIController* Controller, const FDroneNPCProfile& Profile)
{
	const UDroneSmartObjectReservationComponent* Reservation = Controller
		? Controller->GetReservationComponent()
		: nullptr;
	if (!Reservation)
	{
		return false;
	}

	const FGameplayTagContainer& Activities = Reservation->GetRequiredActivityTags();
	if (Profile.Faction == EDroneNPCFaction::Hostile)
	{
		return Activities.HasTagExact(DroneAITags::Activity_EnemyPatrol)
			&& !Activities.HasTagExact(DroneAITags::Activity_FriendlyBasePatrol);
	}
	if (Profile.Faction == EDroneNPCFaction::Friendly)
	{
		// StateTree 실행 전에는 두 후보가 모두 설정되고, 실행 중에는 현재 검색 종류 하나만 남는다.
		return (Activities.HasTagExact(DroneAITags::Activity_FriendlyBasePatrol)
				|| Activities.HasTagExact(DroneAITags::Activity_Ambient))
			&& !Activities.HasTagExact(DroneAITags::Activity_EnemyPatrol);
	}
	return false;
}

class FValidateNPCGreyboxPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateNPCGreyboxPIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		if (!PIEWorld || !PIEWorld->HasBegunPlay())
		{
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(TEXT("NPC Greybox PIE World did not begin play within 20 seconds"));
				return true;
			}
			return false;
		}

		TArray<ADroneNPCCharacter*> NPCs;
		for (TActorIterator<ADroneNPCCharacter> It(PIEWorld); It; ++It)
		{
			NPCs.Add(*It);
		}
		if (NPCs.Num() != 4 && Now - StartedAt <= 20.0)
		{
			return false;
		}

		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(PIEWorld);
		ARecastNavMesh* RecastNavMesh = Navigation
			? Cast<ARecastNavMesh>(Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
			: nullptr;
		bool bNavigationReady = RecastNavMesh && RecastNavMesh->GetNumActiveTiles() > 0;
		if (bNavigationReady)
		{
			for (const ADroneNPCCharacter* NPC : NPCs)
			{
				FNavLocation Projected;
				bNavigationReady &= NPC && Navigation->ProjectPointToNavigation(
					NPC->GetActorLocation(),
					Projected,
					FVector(100.0f, 100.0f, 300.0f));
			}
		}
		const bool bShouldKeepWaitingForNavigation = !bNavigationReady
			&& Now - StartedAt <= 20.0
			&& (!Navigation || Navigation->IsNavigationBuildInProgress() || Now - StartedAt <= 5.0);
		if (bShouldKeepWaitingForNavigation)
		{
			return false;
		}

		if (!bNavigationReady && Navigation)
		{
			Test->AddInfo(FString::Printf(
				TEXT("Navigation diagnostics: Locked=%s InProgress=%s Remaining=%d Running=%d Tiles=%d Bounds=%s"),
				Navigation->IsNavigationBuildingLocked() ? TEXT("true") : TEXT("false"),
				Navigation->IsNavigationBuildInProgress() ? TEXT("true") : TEXT("false"),
				Navigation->GetNumRemainingBuildTasks(),
				Navigation->GetNumRunningBuildTasks(),
				RecastNavMesh ? RecastNavMesh->GetNumActiveTiles() : -1,
				*Navigation->GetNavigableWorldBounds().ToString()));
			for (TActorIterator<AStaticMeshActor> It(PIEWorld); It; ++It)
			{
				const AStaticMeshActor* StaticMeshActor = *It;
				const UStaticMeshComponent* StaticMeshComponent = StaticMeshActor
					? StaticMeshActor->GetStaticMeshComponent()
					: nullptr;
				if (StaticMeshActor && StaticMeshComponent)
				{
					Test->AddInfo(FString::Printf(
						TEXT("Static navigation geometry: Actor=%s Mesh=%s Collision=%d NavRelevant=%s Mobility=%d Bounds=%s"),
						*StaticMeshActor->GetName(),
						*GetNameSafe(StaticMeshComponent->GetStaticMesh()),
						static_cast<int32>(StaticMeshComponent->GetCollisionEnabled()),
						StaticMeshComponent->CanEverAffectNavigation() ? TEXT("true") : TEXT("false"),
						static_cast<int32>(StaticMeshComponent->Mobility),
						*StaticMeshComponent->Bounds.GetBox().ToString()));
				}
			}
			for (TActorIterator<ADroneNPCNavigationFloor> It(PIEWorld); It; ++It)
			{
				const ADroneNPCNavigationFloor* NavigationFloor = *It;
				const UBoxComponent* NavigationCollision = NavigationFloor
					? NavigationFloor->GetNavigationCollision()
					: nullptr;
				if (NavigationFloor && NavigationCollision)
				{
					Test->AddInfo(FString::Printf(
						TEXT("Native navigation floor: Collision=%d Query=%s NavRelevant=%s Registered=%s Bounds=%s"),
						static_cast<int32>(NavigationCollision->GetCollisionEnabled()),
						NavigationCollision->IsQueryCollisionEnabled() ? TEXT("true") : TEXT("false"),
						NavigationCollision->IsNavigationRelevant() ? TEXT("true") : TEXT("false"),
						NavigationCollision->IsRegistered() ? TEXT("true") : TEXT("false"),
						*NavigationCollision->Bounds.GetBox().ToString()));
				}
			}
		}

		Test->TestEqual(TEXT("NPC Greybox PIE contains four NPCs"), NPCs.Num(), 4);
		Test->TestNotNull(TEXT("NPC Greybox PIE has a Navigation System"), Navigation);
		Test->TestNotNull(TEXT("NPC Greybox PIE registers its RecastNavMesh"), RecastNavMesh);
		Test->TestTrue(
			TEXT("NPC Greybox PIE RecastNavMesh has active tiles"),
			RecastNavMesh && RecastNavMesh->GetNumActiveTiles() > 0);
		Test->TestTrue(TEXT("Every NPC start position projects onto NavMesh"), bNavigationReady);

		int32 HostileRifleCount = 0;
		int32 HostileShotgunCount = 0;
		int32 FriendlyCount = 0;
		UClass* ArmedAnimClass = LoadClass<UAnimInstance>(nullptr, ArmedAnimClassPath);
		UClass* UnarmedAnimClass = LoadClass<UAnimInstance>(nullptr, UnarmedAnimClassPath);
		Test->TestNotNull(TEXT("PIE loads the project-owned armed Anim BP"), ArmedAnimClass);
		Test->TestNotNull(TEXT("PIE loads the shared Unarmed Anim BP"), UnarmedAnimClass);
		for (ADroneNPCCharacter* NPC : NPCs)
		{
			Test->TestNotNull(TEXT("Placed NPC exists in PIE"), NPC);
			if (!NPC)
			{
				continue;
			}

			ADroneNPCAIController* Controller = Cast<ADroneNPCAIController>(NPC->GetController());
			Test->TestNotNull(TEXT("Placed NPC is possessed by Drone NPC AI Controller"), Controller);
			const UDroneNPCProfileComponent* ProfileComponent = NPC->GetNPCProfileComponent();
			const UDroneNPCWeaponComponent* WeaponComponent = NPC->GetNPCWeaponComponent();
			Test->TestNotNull(TEXT("Placed NPC keeps its Profile Component"), ProfileComponent);
			Test->TestNotNull(TEXT("Placed NPC keeps its common Weapon Component"), WeaponComponent);
			if (!Controller || !ProfileComponent || !WeaponComponent)
			{
				continue;
			}

			const FDroneNPCProfile& Profile = ProfileComponent->GetProfile();
			const UClass* ExpectedAnimClass = Profile.WeaponType == EDroneNPCWeaponType::Unarmed
				? UnarmedAnimClass
				: ArmedAnimClass;
			Test->TestTrue(
				TEXT("Placed PIE NPC uses the role-appropriate Anim BP"),
				NPC->GetMesh() && NPC->GetMesh()->GetAnimClass() == ExpectedAnimClass);
			Test->TestTrue(TEXT("Controller exposes the possessed common Weapon Component"), Controller->GetPossessedWeaponComponent() == WeaponComponent);
			Test->TestTrue(TEXT("Possession configures Weapon Component from NPC Profile"), WeaponComponent->GetWeaponType() == Profile.WeaponType);
			Test->TestTrue(TEXT("Controller configures role-specific Activity Tags"), HasExpectedActivityTags(Controller, Profile));
			const FGameplayTagContainer& UserTags = Controller->GetReservationComponent()->GetUserTags();

			if (Profile.Faction == EDroneNPCFaction::Hostile && Profile.WeaponType == EDroneNPCWeaponType::Rifle)
			{
				++HostileRifleCount;
				Test->TestTrue(TEXT("Rifle Controller reports Rifle role"), Controller->UsesRifle());
				Test->TestFalse(TEXT("Rifle Controller does not report Shotgun role"), Controller->UsesShotgun());
				Test->TestTrue(TEXT("Eligible Rifle NPC has MG operator tag"), UserTags.HasTagExact(DroneAITags::Role_MGTurretOperator));
			}
			else if (Profile.Faction == EDroneNPCFaction::Hostile && Profile.WeaponType == EDroneNPCWeaponType::Shotgun)
			{
				++HostileShotgunCount;
				Test->TestTrue(TEXT("Shotgun Controller reports Shotgun role"), Controller->UsesShotgun());
				Test->TestFalse(TEXT("Shotgun Controller does not report Rifle role"), Controller->UsesRifle());
				Test->TestFalse(TEXT("Shotgun NPC has no MG operator tag"), UserTags.HasTagExact(DroneAITags::Role_MGTurretOperator));
			}
			else if (Profile.Faction == EDroneNPCFaction::Friendly)
			{
				++FriendlyCount;
				Test->TestFalse(TEXT("Friendly NPC reports no Rifle role"), Controller->UsesRifle());
				Test->TestFalse(TEXT("Friendly NPC reports no Shotgun role"), Controller->UsesShotgun());
			}
		}

		Test->TestEqual(TEXT("PIE has one Hostile Rifle NPC"), HostileRifleCount, 1);
		Test->TestEqual(TEXT("PIE has one Hostile Shotgun NPC"), HostileShotgunCount, 1);
		Test->TestEqual(TEXT("PIE has two Friendly Base NPCs"), FriendlyCount, 2);
		return true;
	}

private:
	FAutomationTestBase* Test;
	double StartedAt = 0.0;
};

class FValidateNPCBaseRoutinesPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateNPCBaseRoutinesPIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		if (!PIEWorld || !PIEWorld->HasBegunPlay())
		{
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(TEXT("NPC Base Routines PIE World did not begin play within 20 seconds"));
				return true;
			}
			return false;
		}

		TArray<ADroneNPCCharacter*> Hostiles;
		TArray<ADroneNPCCharacter*> Friendlies;
		for (TActorIterator<ADroneNPCCharacter> It(PIEWorld); It; ++It)
		{
			ADroneNPCCharacter* NPC = *It;
			const UDroneNPCProfileComponent* Profile = NPC ? NPC->GetNPCProfileComponent() : nullptr;
			if (Profile && Profile->GetProfile().Faction == EDroneNPCFaction::Hostile)
			{
				Hostiles.Add(NPC);
			}
			else if (Profile && Profile->GetProfile().Faction == EDroneNPCFaction::Friendly)
			{
				Friendlies.Add(NPC);
			}
		}

		if (Hostiles.Num() != 2 || Friendlies.Num() != 2)
		{
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(FString::Printf(
					TEXT("NPC Base Routines PIE expected 2 Hostile and 2 Friendly NPCs, found %d and %d"),
					Hostiles.Num(),
					Friendlies.Num()));
				return true;
			}
			return false;
		}

		bool bAllHostilesCompleted = true;
		for (ADroneNPCCharacter* Hostile : Hostiles)
		{
			ADroneNPCAIController* Controller = Cast<ADroneNPCAIController>(Hostile->GetController());
			bAllHostilesCompleted &= Controller
				&& Controller->GetStateTreeAIComponent()
				&& Controller->GetStateTreeAIComponent()->IsRunning()
				&& Controller->GetCompletedPatrolCycles() >= 2
				&& Controller->GetVisitedPatrolSlotCount() >= 2;
		}
		bool bAllFriendliesCompleted = true;
		for (ADroneNPCCharacter* Friendly : Friendlies)
		{
			ADroneNPCAIController* Controller = Cast<ADroneNPCAIController>(Friendly->GetController());
			bAllFriendliesCompleted &= Controller
				&& Controller->GetStateTreeAIComponent()
				&& Controller->GetStateTreeAIComponent()->IsRunning()
				&& Controller->GetCompletedFriendlyRoutineCycles() >= 2
				&& Controller->GetVisitedFriendlySlotCount() >= 2
				&& Controller->HasVisitedFriendlyActivity(DroneAITags::Activity_FriendlyBasePatrol)
				&& Controller->HasVisitedFriendlyActivity(DroneAITags::Activity_Ambient);
		}

		if ((!bAllHostilesCompleted || !bAllFriendliesCompleted) && Now - StartedAt <= 35.0)
		{
			return false;
		}

		Test->TestTrue(TEXT("Both Hostile NPCs complete two patrol cycles and visit two distinct slots"), bAllHostilesCompleted);
		for (ADroneNPCCharacter* Hostile : Hostiles)
		{
			ADroneNPCAIController* Controller = Cast<ADroneNPCAIController>(Hostile->GetController());
			Test->TestNotNull(TEXT("Hostile Patrol NPC keeps Drone AI Controller"), Controller);
			if (Controller)
			{
				Test->TestTrue(TEXT("Hostile Patrol StateTree is running"), Controller->GetStateTreeAIComponent()->IsRunning());
				Test->TestTrue(TEXT("Hostile completes at least two patrol cycles"), Controller->GetCompletedPatrolCycles() >= 2);
				Test->TestTrue(TEXT("Hostile visits at least two distinct patrol slots"), Controller->GetVisitedPatrolSlotCount() >= 2);
			}
		}

		Test->TestTrue(
			TEXT("Both Friendly NPCs complete two cycles across Base Patrol and Ambient slots"),
			bAllFriendliesCompleted);
		for (ADroneNPCCharacter* Friendly : Friendlies)
		{
			ADroneNPCAIController* Controller = Cast<ADroneNPCAIController>(Friendly->GetController());
			Test->TestNotNull(TEXT("Friendly Base NPC keeps Drone AI Controller"), Controller);
			if (Controller)
			{
				Test->TestTrue(TEXT("Friendly Base Routine StateTree is running"), Controller->GetStateTreeAIComponent()->IsRunning());
				Test->TestTrue(TEXT("Friendly completes at least two activity cycles"), Controller->GetCompletedFriendlyRoutineCycles() >= 2);
				Test->TestTrue(TEXT("Friendly visits at least two distinct activity slots"), Controller->GetVisitedFriendlySlotCount() >= 2);
				Test->TestTrue(
					TEXT("Friendly visits a Friendly Base Patrol slot"),
					Controller->HasVisitedFriendlyActivity(DroneAITags::Activity_FriendlyBasePatrol));
				Test->TestTrue(
					TEXT("Friendly visits an Ambient slot"),
					Controller->HasVisitedFriendlyActivity(DroneAITags::Activity_Ambient));
			}
		}
		return true;
	}

private:
	FAutomationTestBase* Test;
	double StartedAt = 0.0;
};

class FValidateNPCPerceptionSearchPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateNPCPerceptionSearchPIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
			PhaseStartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		if (!PIEWorld || !PIEWorld->HasBegunPlay())
		{
			return FinishWithTimeout(Now, 20.0, TEXT("NPC Perception PIE World did not begin play"));
		}

		ADronePrototypePawn* Drone = nullptr;
		for (TActorIterator<ADronePrototypePawn> It(PIEWorld); It; ++It)
		{
			Drone = *It;
			break;
		}

		TArray<ADroneNPCAIController*> Hostiles;
		TArray<ADroneNPCAIController*> Friendlies;
		for (TActorIterator<ADroneNPCCharacter> It(PIEWorld); It; ++It)
		{
			ADroneNPCCharacter* NPC = *It;
			ADroneNPCAIController* Controller = NPC
				? Cast<ADroneNPCAIController>(NPC->GetController())
				: nullptr;
			const UDroneNPCProfileComponent* Profile = NPC ? NPC->GetNPCProfileComponent() : nullptr;
			if (!Controller || !Profile)
			{
				continue;
			}
			if (Profile->GetProfile().Faction == EDroneNPCFaction::Hostile)
			{
				Hostiles.Add(Controller);
			}
			else if (Profile->GetProfile().Faction == EDroneNPCFaction::Friendly)
			{
				Friendlies.Add(Controller);
			}
		}

		if (!Drone || Hostiles.Num() != 2 || Friendlies.Num() != 2)
		{
			return FinishWithTimeout(Now, 20.0, TEXT("NPC Perception PIE requires one Drone, two Hostiles, and two Friendlies"));
		}

		switch (Phase)
		{
		case EPhase::WaitForBaseline:
			return UpdateBaseline(Now, Drone, Hostiles, Friendlies);
		case EPhase::WaitForDetected:
			return UpdateDetected(Now, Drone, Hostiles, Friendlies);
		case EPhase::WaitForMGTurret:
			return UpdateMGTurret(Now, Drone, Hostiles, Friendlies);
		case EPhase::WaitForMGTurretReassignment:
			return UpdateMGTurretReassignment(Now, Drone, Hostiles, Friendlies);
		case EPhase::WaitForSearch:
			return UpdateSearch(Now, Hostiles, Friendlies);
		case EPhase::WaitForReturn:
			return UpdateReturn(Now, Drone, Hostiles, Friendlies);
		case EPhase::WaitForSecondEngagement:
			return UpdateSecondEngagement(Now, Drone, Hostiles, Friendlies);
		case EPhase::WaitForDroneDestroyed:
			return UpdateDroneDestroyed(Now, Drone, Hostiles, Friendlies);
		default:
			return true;
		}
	}

private:
	enum class EPhase : uint8
	{
		WaitForBaseline,
		WaitForDetected,
		WaitForMGTurret,
		WaitForMGTurretReassignment,
		WaitForSearch,
		WaitForReturn,
		WaitForSecondEngagement,
		WaitForDroneDestroyed
	};

	bool FinishWithTimeout(const double Now, const double Timeout, const TCHAR* Message)
	{
		if (Now - PhaseStartedAt <= Timeout)
		{
			return false;
		}
		Test->AddError(Message);
		return true;
	}

	void AdvanceTo(const EPhase NewPhase, const double Now)
	{
		Phase = NewPhase;
		PhaseStartedAt = Now;
	}

	static void BroadcastSight(ADroneNPCAIController* Controller, ADronePrototypePawn* Drone, const bool bSensed)
	{
		if (!Controller || !Drone || !Controller->GetDronePerceptionComponent())
		{
			return;
		}

		const FVector ReceiverLocation = Controller->GetPawn()
			? Controller->GetPawn()->GetActorLocation()
			: FVector::ZeroVector;
		FAIStimulus Stimulus(
			*GetDefault<UAISense_Sight>(),
			1.0f,
			Drone->GetActorLocation(),
			ReceiverLocation,
			FAIStimulus::SensingSucceeded);
		if (!bSensed)
		{
			Stimulus.MarkNoLongerSensed();
		}
		Controller->GetDronePerceptionComponent()->OnTargetPerceptionUpdated.Broadcast(Drone, Stimulus);
	}

	bool UpdateBaseline(
		const double Now,
		ADronePrototypePawn* Drone,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		const bool bHostileReady = Hostiles.ContainsByPredicate(
			[](const ADroneNPCAIController* Controller)
			{
				return !Controller || Controller->GetCompletedPatrolCycles() < 1;
			}) == false;
		const bool bFriendlyReady = Friendlies.ContainsByPredicate(
			[](const ADroneNPCAIController* Controller)
			{
				return !Controller || Controller->GetCompletedFriendlyRoutineCycles() < 1;
			}) == false;
		if (!bHostileReady || !bFriendlyReady)
		{
			return FinishWithTimeout(Now, 30.0, TEXT("NPC routines did not establish a patrol baseline before perception test"));
		}

		const UDroneHealthComponent* DroneHealth = Drone->GetHealthComponent();
		bool bDefaultHealthReady = DroneHealth
			&& FMath::IsNearlyEqual(DroneHealth->GetMaxHealth(), 100.0f)
			&& FMath::IsNearlyEqual(DroneHealth->GetCurrentHealth(), 100.0f)
			&& !DroneHealth->IsDead();
		for (ADroneNPCAIController* Controller : Hostiles)
		{
			ADroneNPCCharacter* NPC = Cast<ADroneNPCCharacter>(Controller->GetPawn());
			const UDroneHealthComponent* NPCHealth = NPC ? NPC->GetHealthComponent() : nullptr;
			bDefaultHealthReady &= NPCHealth
				&& FMath::IsNearlyEqual(NPCHealth->GetMaxHealth(), 100.0f)
				&& FMath::IsNearlyEqual(NPCHealth->GetCurrentHealth(), 100.0f)
				&& !NPCHealth->IsDead();

			// 한 Slot 경합과 사망 후 재점유를 같은 Map에서 검증하기 위해 Shotgun도
			// 이 PIE 실행에서만 MG 사용 가능 Profile로 바꾼다. 원본 Asset은 변경하지 않는다.
			if (NPC && Controller->UsesShotgun())
			{
				UDroneNPCProfileComponent* ProfileComponent = NPC->GetNPCProfileComponent();
				FDroneNPCProfile RuntimeProfile = ProfileComponent->GetProfile();
				RuntimeProfile.bCanUseMGTurret = true;
				ProfileComponent->SetProfile(RuntimeProfile);
				Controller->GetReservationComponent()->SetUserTags(ProfileComponent->BuildSmartObjectUserTags());
			}
		}
		if (!bDefaultHealthReady)
		{
			Test->AddError(TEXT("Drone and NPC default Health must start alive at 100/100"));
			return true;
		}

		for (ADroneNPCAIController* Controller : Hostiles)
		{
			BaselineHostileCycles.Add(Controller, Controller->GetCompletedPatrolCycles());
			BroadcastSight(Controller, Drone, true);
		}
		for (ADroneNPCAIController* Controller : Friendlies)
		{
			BaselineFriendlyCycles.Add(Controller, Controller->GetCompletedFriendlyRoutineCycles());
			BroadcastSight(Controller, Drone, true);
		}
		AdvanceTo(EPhase::WaitForDetected, Now);
		return false;
	}

	bool UpdateDetected(
		const double Now,
		ADronePrototypePawn* Drone,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		bool bHostilesDetected = true;
		for (const ADroneNPCAIController* Controller : Hostiles)
		{
			const EDroneNPCAIResponseState ResponseState = Controller->GetResponseState();
			const bool bExpectedResponse = Controller->CanUseMGTurret()
				? ResponseState == EDroneNPCAIResponseState::DroneDetected
					|| ResponseState == EDroneNPCAIResponseState::MoveToMGTurret
					|| ResponseState == EDroneNPCAIResponseState::HoldMGTurret
					|| ResponseState == EDroneNPCAIResponseState::UseMGTurret
					|| ResponseState == EDroneNPCAIResponseState::MoveToCover
					|| ResponseState == EDroneNPCAIResponseState::UseCover
				: ResponseState == EDroneNPCAIResponseState::DroneDetected;
			bHostilesDetected &= Controller->HasDetectedDrone()
				&& bExpectedResponse
				&& Controller->GetDroneDetectionCount() == 1
				// AI Focus는 몸/Smart Object Slot 회전과 경쟁하므로 사용하지 않는다.
				// 시각 표현용 Drone gaze는 Controller의 별도 상태만 검증한다.
				&& Controller->GetFocusActor() == nullptr
				&& Controller->HasActiveDroneLookTarget()
				&& Controller->GetDroneLookAlpha() > 0.0f
				&& FMath::Abs(Controller->GetSmoothedDroneLookRotation().Yaw)
					<= Controller->GetMaxDroneLookYawDegrees() + KINDA_SMALL_NUMBER
				&& Controller->GetSmoothedDroneLookRotation().Pitch
					<= Controller->GetMaxDroneLookPitchUpDegrees() + KINDA_SMALL_NUMBER
				&& Controller->GetSmoothedDroneLookRotation().Pitch
					>= -Controller->GetMaxDroneLookPitchDownDegrees() - KINDA_SMALL_NUMBER;
		}
		bool bFriendliesUnaffected = true;
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			bFriendliesUnaffected &= !Controller->HasDetectedDrone()
				&& Controller->GetResponseState() == EDroneNPCAIResponseState::Patrol
				&& Controller->GetDroneDetectionCount() == 0
				&& Controller->GetFocusActor() == nullptr
				&& !Controller->HasActiveDroneLookTarget()
				&& FMath::IsNearlyZero(Controller->GetDroneLookAlpha());
		}
		if (!bHostilesDetected || !bFriendliesUnaffected)
		{
			return FinishWithTimeout(Now, 5.0, TEXT("Hostile-only DroneDetected response did not become stable"));
		}

		AdvanceTo(EPhase::WaitForMGTurret, Now);
		return false;
	}

	bool UpdateMGTurret(
		const double Now,
		ADronePrototypePawn* Drone,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		int32 MGTurretOperatorCount = 0;
		int32 MGTurretEligibleCount = 0;
		int32 MGTurretReservationCount = 0;
		bool bMGTurretOperatorReady = true;
		bool bPersonalWeaponFallbackReady = true;
		for (ADroneNPCAIController* Controller : Hostiles)
		{
			UDroneNPCWeaponComponent* WeaponComponent = Controller->GetPossessedWeaponComponent();
			MGTurretEligibleCount += Controller->CanUseMGTurret() ? 1 : 0;
			const EDroneNPCAIResponseState ResponseState = Controller->GetResponseState();
			const APawn* ControlledPawn = Controller->GetPawn();
			FVector ToDrone = ControlledPawn
				? Drone->GetActorLocation() - ControlledPawn->GetActorLocation()
				: FVector::ZeroVector;
			ToDrone.Z = 0.0f;
			const bool bFacesDrone = ControlledPawn
				&& !ToDrone.IsNearlyZero()
				&& FMath::IsNearlyZero(
					FMath::FindDeltaAngleDegrees(
						ControlledPawn->GetActorRotation().Yaw,
						ToDrone.Rotation().Yaw),
					5.0f);
			if (Controller->GetReservationComponent()->HasValidReservation()
				&& (ResponseState == EDroneNPCAIResponseState::MoveToMGTurret
					|| ResponseState == EDroneNPCAIResponseState::HoldMGTurret
					|| ResponseState == EDroneNPCAIResponseState::UseMGTurret))
			{
				++MGTurretReservationCount;
			}

			if (Controller->GetResponseState() == EDroneNPCAIResponseState::UseMGTurret)
			{
				++MGTurretOperatorCount;
				ADroneSmartObjectStation* Station = Controller->GetActiveMGTurretStation();
				const ADroneMGTurretStation* Turret = Cast<ADroneMGTurretStation>(Station);
				FTransform OperatorTransform;
				const bool bHasOperatorTransform = Controller->GetReservedMGTurretOperatorTransform(OperatorTransform);
				const bool bAtOperatorAnchor = ControlledPawn
					&& bHasOperatorTransform
					&& FVector::DistSquared2D(ControlledPawn->GetActorLocation(), OperatorTransform.GetLocation())
						<= FMath::Square(2.0f);
				const bool bFacesOperatorDirection = ControlledPawn
					&& bHasOperatorTransform
					&& FMath::IsNearlyZero(
						FMath::FindDeltaAngleDegrees(
							ControlledPawn->GetActorRotation().Yaw,
							OperatorTransform.Rotator().Yaw),
						1.0f);
				bMGTurretOperatorReady &= Controller->GetMGTurretClaimCount() == 1
					&& Controller->GetMGTurretArrivalCount() == 1
					&& Controller->GetMGTurretUseCount() == 1
					&& Controller->GetReservationComponent()->IsReservationOccupied()
					&& Turret
					&& Turret->GetMGTurretOperatorAnchor()
					&& bAtOperatorAnchor
					&& bFacesOperatorDirection
					&& Station
					&& Station->IsMGTurretInUse()
					&& Station->GetMGTurretUser() == Controller->GetPawn()
					&& Station->GetMGTurretTarget() == Drone
					&& Station->GetMGTurretAimPoint().Equals(Drone->GetActorLocation(), 1.0f)
					&& Station->GetMGTurretOccupationCount() == 1
					&& FMath::IsNearlyEqual(Station->GetMGTurretDamage(), 8.0f)
					&& Station->UsesMGTurretProjectileBallistics()
					&& FMath::IsNearlyEqual(Station->GetMGTurretProjectileSpeed(), 5500.0f)
					&& Station->GetMGTurretProjectileSpawnCount() > 0
					&& WeaponComponent
					&& !WeaponComponent->IsFiring();
				if (Station)
				{
					UsedMGTurretStation = Station;
					InitialMGTurretController = Controller;
				}
				continue;
			}

			if (WeaponComponent)
			{
				// 수동 Sight Broadcast는 실제 Sight 반경을 적용하지 않는다. Shotgun의
				// 사거리 자체는 전용 테스트에 맡기고 여기서는 MG 실패 시 개인 무기
				// Fallback과 공용 Target/Aim Point 전달만 분리 검증한다.
				WeaponComponent->ConfigureRifleGreybox(100000.0f, 0.5f);
				WeaponComponent->ConfigureShotgunGreybox(100000.0f, 1.0f, 8, 6.0f);
			}
			bPersonalWeaponFallbackReady &= Controller->GetResponseState() == EDroneNPCAIResponseState::UseCover
				&& Controller->GetCoverClaimCount() == 1
				&& Controller->GetCoverUseCount() == 1
				&& Controller->GetReservationComponent()->IsReservationOccupied()
				&& bFacesDrone
				&& Controller->CanFirePersonalWeapon()
				&& WeaponComponent
				&& WeaponComponent->IsFiring()
				&& WeaponComponent->GetCurrentTarget() == Drone
				&& WeaponComponent->GetCurrentAimPoint().Equals(Drone->GetActorLocation());
			if (bPersonalWeaponFallbackReady)
			{
				InitialCoverController = Controller;
			}
		}
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			const UDroneNPCWeaponComponent* WeaponComponent = Controller->GetPossessedWeaponComponent();
			bPersonalWeaponFallbackReady &= !Controller->CanFirePersonalWeapon()
				&& WeaponComponent
				&& !WeaponComponent->IsFiring();
		}
		if (MGTurretEligibleCount != 2
			|| MGTurretOperatorCount != 1
			|| MGTurretReservationCount != 1
			|| !bMGTurretOperatorReady
			|| !bPersonalWeaponFallbackReady)
		{
			if (Now - PhaseStartedAt > 12.0)
			{
				const ADroneSmartObjectStation* DebugStation = UsedMGTurretStation.Get();
				Test->AddInfo(FString::Printf(
					TEXT("MG debug: eligible=%d operators=%d reservations=%d operatorReady=%d fallbackReady=%d station=%s shots=%d yaw=%.2f pitch=%.2f error=%.2f target=%s"),
					MGTurretEligibleCount,
					MGTurretOperatorCount,
					MGTurretReservationCount,
					bMGTurretOperatorReady ? 1 : 0,
					bPersonalWeaponFallbackReady ? 1 : 0,
					*GetNameSafe(DebugStation),
					DebugStation ? DebugStation->GetMGTurretProjectileSpawnCount() : -1,
					DebugStation ? DebugStation->GetMGTurretCurrentYawDegrees() : 0.0f,
					DebugStation ? DebugStation->GetMGTurretCurrentPitchDegrees() : 0.0f,
					DebugStation ? DebugStation->GetMGTurretAlignmentErrorDegrees() : 0.0f,
					*GetNameSafe(DebugStation ? DebugStation->GetMGTurretTarget() : nullptr)));
			}
			return FinishWithTimeout(Now, 12.0, TEXT("MG Occupy/Aim/Fire or personal Weapon fallback did not become stable"));
		}

		ADroneNPCAIController* Operator = InitialMGTurretController.Get();
		ADroneSmartObjectStation* Station = UsedMGTurretStation.Get();
		ADroneNPCCharacter* OperatorPawn = Operator
			? Cast<ADroneNPCCharacter>(Operator->GetPawn())
			: nullptr;
		if (!OperatorPawn || !Station || !OperatorPawn->GetHealthComponent())
		{
			Test->AddError(TEXT("Initial MG operator does not expose the required Health/Station contract"));
			return true;
		}

		Test->TestEqual(TEXT("Rifle greybox damage is 10"),
			Operator->GetPossessedWeaponComponent()->GetRifleDamage(), 10.0f);
		Test->TestEqual(TEXT("Shotgun pellet greybox damage is 8"),
			Operator->GetPossessedWeaponComponent()->GetShotgunDamagePerPellet(), 8.0f);
		Drone->GetHealthComponent()->ResetHealth();
		MGTurretShotsBeforeReassignment = Station->GetMGTurretProjectileSpawnCount();
		UGameplayStatics::ApplyDamage(OperatorPawn, 100.0f, nullptr, Drone, nullptr);
		AdvanceTo(EPhase::WaitForMGTurretReassignment, Now);
		return false;
	}

	bool UpdateMGTurretReassignment(
		const double Now,
		ADronePrototypePawn* Drone,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		ADroneNPCAIController* DeadController = InitialMGTurretController.Get();
		ADroneSmartObjectStation* Station = UsedMGTurretStation.Get();
		ADroneNPCAIController* Survivor = nullptr;
		for (ADroneNPCAIController* Controller : Hostiles)
		{
			if (Controller && Controller != DeadController)
			{
				Survivor = Controller;
				break;
			}
		}
		ADroneNPCCharacter* DeadPawn = DeadController
			? Cast<ADroneNPCCharacter>(DeadController->GetPawn())
			: nullptr;
		const UDroneHealthComponent* DeadHealth = DeadPawn ? DeadPawn->GetHealthComponent() : nullptr;

		const bool bDeadCleanedUp = DeadController
			&& DeadPawn
			&& DeadHealth
			&& DeadHealth->IsDead()
			&& FMath::IsNearlyZero(DeadHealth->GetCurrentHealth())
			&& DeadHealth->GetDeathEventCount() == 1
			&& DeadController->GetResponseState() == EDroneNPCAIResponseState::Dead
			&& !DeadController->GetReservationComponent()->HasValidReservation()
			&& DeadController->GetActiveMGTurretStation() == nullptr
			&& !DeadPawn->GetActorEnableCollision();
		const bool bSurvivorReassigned = Survivor
			&& Station
			&& Survivor == InitialCoverController.Get()
			&& Survivor->GetResponseState() == EDroneNPCAIResponseState::UseMGTurret
			&& Survivor->GetReservationComponent()->IsReservationOccupied()
			&& Survivor->GetActiveMGTurretStation() == Station
			&& Survivor->GetMGTurretUseCount() == 1
			&& Survivor->GetCoverClaimCount() == 1
			&& Survivor->GetCoverUseCount() == 1
			&& Station->IsMGTurretInUse()
			&& Station->GetMGTurretUser() == Survivor->GetPawn()
			&& Station->GetMGTurretTarget() == Drone
			&& Station->GetMGTurretOccupationCount() == 2
			&& Station->GetMGTurretReleaseCount() >= 1
			&& Station->GetMGTurretProjectileSpawnCount() > MGTurretShotsBeforeReassignment;
		if (!bDeadCleanedUp || !bSurvivorReassigned)
		{
			return FinishWithTimeout(Now, 12.0, TEXT("Dead MG operator was not cleaned up or another eligible Hostile did not reoccupy the Slot"));
		}

		SurvivingMGTurretController = Survivor;
		// 순간적인 Sight 실패 뒤 같은 프레임에 재감지되면 DroneLost가 확정되지 않아야 한다.
		// MG 이동/점유 상태가 시야 깜빡임 때문에 Search와 왕복하는 회귀를 막는다.
		const int32 LostCountBeforeTransientSightFailure = Survivor->GetDroneLostCount();
		BroadcastSight(Survivor, Drone, false);
		if (!Survivor->IsDroneSightLossPending()
			|| !Survivor->HasDetectedDrone()
			|| Survivor->GetFocusActor() != nullptr
			|| !Survivor->HasActiveDroneLookTarget())
		{
			Test->AddError(TEXT("Transient Sight failure was not held during the DroneLost grace period"));
			return true;
		}
		BroadcastSight(Survivor, Drone, true);
		if (Survivor->IsDroneSightLossPending()
			|| Survivor->GetDroneLostCount() != LostCountBeforeTransientSightFailure
			|| !Survivor->HasDetectedDrone()
			|| Survivor->GetFocusActor() != nullptr)
		{
			Test->AddError(TEXT("Drone reacquisition did not cancel the pending DroneLost transition"));
			return true;
		}

		BroadcastSight(Survivor, Drone, false);
		for (ADroneNPCAIController* Controller : Friendlies)
		{
			BroadcastSight(Controller, Drone, false);
		}
		// 실제 Sight 갱신이 수동 Lost 자극 직후 같은 Pawn을 다시 감지해
		// Search를 중단하지 않도록 테스트 Pawn을 LoseSight 범위 밖으로 격리한다.
		Drone->SetActorLocation(
			FVector(100000.0f, 100000.0f, 100000.0f),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		AdvanceTo(EPhase::WaitForSearch, Now);
		return false;
	}

	bool UpdateSearch(
		const double Now,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		bool bAllSearching = true;
		for (const ADroneNPCAIController* Controller : Hostiles)
		{
			if (Controller == InitialMGTurretController.Get())
			{
				bAllSearching &= Controller->GetResponseState() == EDroneNPCAIResponseState::Dead
					&& !Controller->HasDetectedDrone()
					&& !Controller->GetReservationComponent()->HasValidReservation()
					&& Controller->GetActiveMGTurretStation() == nullptr;
				continue;
			}

			const UDroneNPCWeaponComponent* WeaponComponent = Controller->GetPossessedWeaponComponent();
			bAllSearching &= !Controller->HasDetectedDrone()
				&& Controller->GetResponseState() == EDroneNPCAIResponseState::Search
				&& Controller->GetDroneLostCount() == 1
				&& Controller->GetDroneSearchStartCount() == 1
				&& Controller->HasLastKnownDroneLocation()
				&& !Controller->GetReservationComponent()->HasValidReservation()
				&& Controller->GetActiveMGTurretStation() == nullptr
				&& Controller->GetFocusActor() == nullptr
				&& Controller->HasActiveDroneLookTarget()
				&& WeaponComponent
				&& !WeaponComponent->IsFiring()
				&& WeaponComponent->GetCurrentTarget() == nullptr;
		}
		bool bFriendliesUnaffected = true;
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			bFriendliesUnaffected &= Controller->GetResponseState() == EDroneNPCAIResponseState::Patrol
				&& Controller->GetDroneLostCount() == 0
				&& Controller->GetFocusActor() == nullptr
				&& !Controller->HasActiveDroneLookTarget();
		}
		const bool bMGTurretReleased = UsedMGTurretStation.IsValid()
			&& !UsedMGTurretStation->IsMGTurretInUse()
			&& UsedMGTurretStation->GetMGTurretReleaseCount() >= 2;
		if (!bAllSearching || !bFriendliesUnaffected || !bMGTurretReleased)
		{
			return FinishWithTimeout(Now, 5.0, TEXT("DroneLost did not release reassigned MG or transition the surviving Hostile into Search"));
		}

		AdvanceTo(EPhase::WaitForReturn, Now);
		return false;
	}

	bool UpdateReturn(
		const double Now,
		ADronePrototypePawn* Drone,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		bool bHostilesReturned = true;
		for (const ADroneNPCAIController* Controller : Hostiles)
		{
			if (Controller == InitialMGTurretController.Get())
			{
				bHostilesReturned &= Controller->GetResponseState() == EDroneNPCAIResponseState::Dead
					&& Controller->GetCompletedDroneSearchCount() == 0;
				continue;
			}

			const bool bPatrolWorkResumed = Controller->GetReservationComponent()->HasValidReservation()
				|| Controller->GetCompletedPatrolCycles() > BaselineHostileCycles.FindRef(Controller);
			bHostilesReturned &= Controller->GetResponseState() == EDroneNPCAIResponseState::Patrol
				&& Controller->GetCompletedDroneSearchCount() == 1
				&& Controller->GetFocusActor() == nullptr
				&& !Controller->HasActiveDroneLookTarget()
				&& bPatrolWorkResumed;
		}
		bool bFriendliesContinued = true;
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			bFriendliesContinued &= Controller->GetCompletedFriendlyRoutineCycles()
				> BaselineFriendlyCycles.FindRef(Controller);
		}
		if ((!bHostilesReturned || !bFriendliesContinued) && Now - PhaseStartedAt <= 20.0)
		{
			return false;
		}
		if (!bHostilesReturned)
		{
			for (int32 Index = 0; Index < Hostiles.Num(); ++Index)
			{
				const ADroneNPCAIController* Controller = Hostiles[Index];
				Test->AddError(FString::Printf(
					TEXT("Hostile[%d] return state=%d detected=%d detect/lost/search/complete=%d/%d/%d/%d reservation=%d patrol=%d baseline=%d"),
					Index,
					static_cast<int32>(Controller->GetResponseState()),
					Controller->HasDetectedDrone() ? 1 : 0,
					Controller->GetDroneDetectionCount(),
					Controller->GetDroneLostCount(),
					Controller->GetDroneSearchStartCount(),
					Controller->GetCompletedDroneSearchCount(),
					Controller->GetReservationComponent()->HasValidReservation() ? 1 : 0,
					Controller->GetCompletedPatrolCycles(),
					BaselineHostileCycles.FindRef(Controller)));
			}
		}

		Test->TestTrue(TEXT("Dead Hostile stays dead and surviving Hostile returns to Patrol after Search"), bHostilesReturned);
		Test->TestTrue(TEXT("Both Friendlies continue BaseRoutine during Hostile perception response"), bFriendliesContinued);
		if (!bHostilesReturned || !bFriendliesContinued)
		{
			return true;
		}

		ADroneNPCAIController* Survivor = SurvivingMGTurretController.Get();
		if (!Drone || !Drone->GetHealthComponent() || !Survivor || !Survivor->GetPawn())
		{
			Test->AddError(TEXT("Second engagement requires the surviving Hostile and Drone Health"));
			return true;
		}

		// Search 복귀 뒤 같은 Drone을 다시 근처에 투입해 파괴 시 전투 종료까지 검증한다.
		Drone->GetHealthComponent()->ResetHealth();
		// 실제 Sight의 비동기 Lost 갱신이 수동 테스트 자극을 덮어쓰지 않도록 이 단계부터
		// 자동 Source 등록을 끈다. 파괴 시 본 코드가 다시 Unregister해도 안전해야 한다.
		Drone->GetPerceptionStimuliSource()->UnregisterFromPerceptionSystem();
		Drone->SetActorLocation(
			Survivor->GetPawn()->GetActorLocation() + FVector(600.0f, 0.0f, 300.0f),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		BroadcastSight(Survivor, Drone, true);
		AdvanceTo(EPhase::WaitForSecondEngagement, Now);
		return false;
	}

	bool UpdateSecondEngagement(
		const double Now,
		ADronePrototypePawn* Drone,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		ADroneNPCAIController* Survivor = SurvivingMGTurretController.Get();
		ADroneSmartObjectStation* Station = UsedMGTurretStation.Get();
		const bool bReengaged = Survivor
			&& Station
			&& Survivor->HasDetectedDrone()
			&& Survivor->GetDroneDetectionCount() == 2
			&& Survivor->GetResponseState() == EDroneNPCAIResponseState::UseMGTurret
			&& Survivor->GetActiveMGTurretStation() == Station
			&& Survivor->GetReservationComponent()->IsReservationOccupied()
			&& Station->IsMGTurretInUse()
			&& Station->GetMGTurretUser() == Survivor->GetPawn()
			&& Station->GetMGTurretTarget() == Drone;
		if (!bReengaged)
		{
			if (Now - PhaseStartedAt <= 12.0)
			{
				return false;
			}
			Test->AddError(FString::Printf(
				TEXT("Second engagement failed: state=%d detected=%d detect/lost=%d/%d reservation=%d occupied=%d activeMG=%s stationUse=%d stationUser=%s stationTarget=%s"),
				Survivor ? static_cast<int32>(Survivor->GetResponseState()) : -1,
				Survivor && Survivor->HasDetectedDrone() ? 1 : 0,
				Survivor ? Survivor->GetDroneDetectionCount() : -1,
				Survivor ? Survivor->GetDroneLostCount() : -1,
				Survivor && Survivor->GetReservationComponent()->HasValidReservation() ? 1 : 0,
				Survivor && Survivor->GetReservationComponent()->IsReservationOccupied() ? 1 : 0,
				*GetNameSafe(Survivor ? Survivor->GetActiveMGTurretStation() : nullptr),
				Station && Station->IsMGTurretInUse() ? 1 : 0,
				*GetNameSafe(Station ? Station->GetMGTurretUser() : nullptr),
				*GetNameSafe(Station ? Station->GetMGTurretTarget() : nullptr)));
			return true;
		}

		bool bOthersUnchanged = true;
		for (const ADroneNPCAIController* Controller : Hostiles)
		{
			if (Controller != Survivor)
			{
				bOthersUnchanged &= Controller->GetResponseState() == EDroneNPCAIResponseState::Dead
					&& Controller->GetDroneDestroyedResponseCount() == 0;
			}
		}
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			bOthersUnchanged &= !Controller->HasDetectedDrone()
				&& Controller->GetDroneDestroyedResponseCount() == 0;
		}
		if (!bOthersUnchanged)
		{
			Test->AddError(TEXT("Dead Hostile or Friendly changed during the second engagement"));
			return true;
		}

		UGameplayStatics::ApplyDamage(Drone, 100.0f, Survivor, Survivor->GetPawn(), nullptr);
		AdvanceTo(EPhase::WaitForDroneDestroyed, Now);
		return false;
	}

	bool UpdateDroneDestroyed(
		const double Now,
		ADronePrototypePawn* Drone,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		ADroneNPCAIController* Survivor = SurvivingMGTurretController.Get();
		ADroneSmartObjectStation* Station = UsedMGTurretStation.Get();
		const UDroneHealthComponent* DroneHealth = Drone ? Drone->GetHealthComponent() : nullptr;
		const UDroneNPCWeaponComponent* SurvivorWeapon = Survivor
			? Survivor->GetPossessedWeaponComponent()
			: nullptr;
		const bool bDroneDestroyedOnce = Drone
			&& DroneHealth
			&& DroneHealth->IsDead()
			&& FMath::IsNearlyZero(DroneHealth->GetCurrentHealth())
			&& DroneHealth->GetDeathEventCount() == 1
			&& Drone->GetDroneDestroyedEventCount() == 1
			&& !Drone->GetActorEnableCollision()
			&& Drone->GetPrototypeMovementComponent()
			&& !Drone->GetPrototypeMovementComponent()->IsActive();
		const bool bCombatStopped = Survivor
			&& Survivor->GetDroneDestroyedResponseCount() == 1
			&& !Survivor->HasDetectedDrone()
			&& !Survivor->HasLastKnownDroneLocation()
			&& Survivor->GetFocusActor() == nullptr
			&& !Survivor->HasActiveDroneLookTarget()
			&& Survivor->GetResponseState() == EDroneNPCAIResponseState::Patrol
			&& Survivor->GetActiveMGTurretStation() == nullptr
			&& SurvivorWeapon
			&& !SurvivorWeapon->IsFiring()
			&& SurvivorWeapon->GetCurrentTarget() == nullptr
			&& Station
			&& !Station->IsMGTurretInUse()
			&& Station->GetMGTurretUser() == nullptr
			&& Station->GetMGTurretTarget() == nullptr;

		bool bOthersUnaffected = true;
		for (const ADroneNPCAIController* Controller : Hostiles)
		{
			if (Controller != Survivor)
			{
				bOthersUnaffected &= Controller->GetResponseState() == EDroneNPCAIResponseState::Dead
					&& Controller->GetDroneDestroyedResponseCount() == 0;
			}
		}
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			bOthersUnaffected &= Controller->GetDroneDestroyedResponseCount() == 0
				&& !Controller->HasDetectedDrone();
		}

		if ((!bDroneDestroyedOnce || !bCombatStopped || !bOthersUnaffected)
			&& Now - PhaseStartedAt <= 5.0)
		{
			return false;
		}
		if (!bCombatStopped)
		{
			Test->AddError(FString::Printf(
				TEXT("Destroyed cleanup failed: responseCount=%d state=%d detected=%d lastKnown=%d reservation=%d occupied=%d activeMG=%s weaponFiring=%d weaponTarget=%s stationUse=%d stationUser=%s stationTarget=%s"),
				Survivor ? Survivor->GetDroneDestroyedResponseCount() : -1,
				Survivor ? static_cast<int32>(Survivor->GetResponseState()) : -1,
				Survivor && Survivor->HasDetectedDrone() ? 1 : 0,
				Survivor && Survivor->HasLastKnownDroneLocation() ? 1 : 0,
				Survivor && Survivor->GetReservationComponent()->HasValidReservation() ? 1 : 0,
				Survivor && Survivor->GetReservationComponent()->IsReservationOccupied() ? 1 : 0,
				*GetNameSafe(Survivor ? Survivor->GetActiveMGTurretStation() : nullptr),
				SurvivorWeapon && SurvivorWeapon->IsFiring() ? 1 : 0,
				*GetNameSafe(SurvivorWeapon ? SurvivorWeapon->GetCurrentTarget() : nullptr),
				Station && Station->IsMGTurretInUse() ? 1 : 0,
				*GetNameSafe(Station ? Station->GetMGTurretUser() : nullptr),
				*GetNameSafe(Station ? Station->GetMGTurretTarget() : nullptr)));
		}

		Test->TestTrue(TEXT("Drone reaches zero Health and publishes its destruction Event once"), bDroneDestroyedOnce);
		Test->TestTrue(TEXT("Drone destruction releases MG and stops every surviving Hostile combat resource"), bCombatStopped);
		Test->TestTrue(TEXT("Dead Hostile and Friendlies ignore the Drone destruction response"), bOthersUnaffected);
		if (!bDroneDestroyedOnce || !bCombatStopped || !bOthersUnaffected)
		{
			return true;
		}

		// 파괴 뒤 들어오는 중복 Damage가 사망/임무 신호를 다시 발생시키지 않아야 한다.
		UGameplayStatics::ApplyDamage(Drone, 10.0f, Survivor, Survivor->GetPawn(), nullptr);
		Test->TestEqual(TEXT("Repeated damage does not rebroadcast Drone Health death"), DroneHealth->GetDeathEventCount(), 1);
		Test->TestEqual(TEXT("Repeated damage does not rebroadcast Drone destruction"), Drone->GetDroneDestroyedEventCount(), 1);
		return true;
	}

	FAutomationTestBase* Test;
	EPhase Phase = EPhase::WaitForBaseline;
	double StartedAt = 0.0;
	double PhaseStartedAt = 0.0;
	TMap<TWeakObjectPtr<ADroneNPCAIController>, int32> BaselineHostileCycles;
	TMap<TWeakObjectPtr<ADroneNPCAIController>, int32> BaselineFriendlyCycles;
	TWeakObjectPtr<ADroneSmartObjectStation> UsedMGTurretStation;
	TWeakObjectPtr<ADroneNPCAIController> InitialMGTurretController;
	TWeakObjectPtr<ADroneNPCAIController> InitialCoverController;
	TWeakObjectPtr<ADroneNPCAIController> SurvivingMGTurretController;
	int32 MGTurretShotsBeforeReassignment = 0;
};
} // namespace DroneNPCGreybox

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCGreyboxAssetTest,
	"Drone.AI.NPCGreyboxAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCGreyboxAssetTest::RunTest(const FString& Parameters)
{
	using namespace DroneNPCGreybox;

	USkeletalMesh* MannyMesh = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	UClass* UnarmedAnimClass = LoadClass<UAnimInstance>(nullptr, UnarmedAnimClassPath);
	UClass* ArmedAnimClass = LoadClass<UAnimInstance>(nullptr, ArmedAnimClassPath);
	UBlendSpace* ArmedLocomotionBlendSpace = LoadObject<UBlendSpace>(nullptr, ArmedLocomotionBlendSpacePath);
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, GameModeClassPath);
	TestNotNull(TEXT("Manny Greybox Mesh loads"), MannyMesh);
	TestNotNull(TEXT("Unarmed Anim Blueprint Class loads"), UnarmedAnimClass);
	TestNotNull(TEXT("Project-owned armed Anim Blueprint Class loads"), ArmedAnimClass);
	TestTrue(
		TEXT("Project-owned armed Anim Blueprint uses the Drone NPC AnimInstance parent"),
		ArmedAnimClass && ArmedAnimClass->IsChildOf(UDroneNPCAnimInstance::StaticClass()));
	TestNotNull(TEXT("Project-owned Rifle locomotion BlendSpace loads"), ArmedLocomotionBlendSpace);
	TestNotNull(TEXT("Prototype GameMode Class loads"), GameModeClass);

	TMap<UClass*, int32> ExpectedPlacedCounts;
	for (const FNPCExpectation& Expectation : NPCExpectations)
	{
		const FString BlueprintClassPath = FString::Printf(
			TEXT("/Game/Drone/AI/Blueprints/BP_NPC_%s.BP_NPC_%s_C"),
			Expectation.Name,
			Expectation.Name);
		UClass* NPCClass = LoadClass<ADroneNPCCharacter>(nullptr, *BlueprintClassPath);
		TestNotNull(*FString::Printf(TEXT("BP_NPC_%s Class loads"), Expectation.Name), NPCClass);
		if (!NPCClass)
		{
			continue;
		}

		TestTrue(
			*FString::Printf(TEXT("BP_NPC_%s uses project Character parent"), Expectation.Name),
			NPCClass->IsChildOf(ADroneNPCCharacter::StaticClass()));
		const ADroneNPCCharacter* CDO = Cast<ADroneNPCCharacter>(NPCClass->GetDefaultObject());
		TestNotNull(*FString::Printf(TEXT("BP_NPC_%s CDO exists"), Expectation.Name), CDO);
		if (!CDO)
		{
			continue;
		}

		const FDroneNPCProfile& Profile = CDO->GetNPCProfileComponent()->GetProfile();
		TestTrue(*FString::Printf(TEXT("BP_NPC_%s Faction matches"), Expectation.Name), Profile.Faction == Expectation.Faction);
		TestTrue(*FString::Printf(TEXT("BP_NPC_%s Weapon matches"), Expectation.Name), Profile.WeaponType == Expectation.Weapon);
		TestTrue(*FString::Printf(TEXT("BP_NPC_%s MG permission matches"), Expectation.Name), Profile.bCanUseMGTurret == Expectation.bCanUseMG);
		TestTrue(*FString::Printf(TEXT("BP_NPC_%s uses project AI Controller"), Expectation.Name), CDO->AIControllerClass == ADroneNPCAIController::StaticClass());
		TestTrue(*FString::Printf(TEXT("BP_NPC_%s auto-possesses AI"), Expectation.Name), CDO->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned);
		TestTrue(*FString::Printf(TEXT("BP_NPC_%s uses shared Manny Greybox Mesh"), Expectation.Name), CDO->GetMesh()->GetSkeletalMeshAsset() == MannyMesh);
		const UClass* ExpectedAnimClass = Expectation.Weapon == EDroneNPCWeaponType::Unarmed
			? UnarmedAnimClass
			: ArmedAnimClass;
		TestTrue(
			*FString::Printf(TEXT("BP_NPC_%s uses the role-appropriate Anim BP"), Expectation.Name),
			CDO->GetMesh()->GetAnimClass() == ExpectedAnimClass);
		TestEqual(*FString::Printf(TEXT("BP_NPC_%s Greybox Mesh has no collision"), Expectation.Name), CDO->GetMesh()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		ExpectedPlacedCounts.Add(NPCClass, Expectation.PlacedCount);
	}

	UClass* SpawnPointClass = LoadClass<ADroneNPCSpawnPoint>(
		nullptr,
		TEXT("/Game/Drone/AI/Blueprints/BP_NPCSpawnPoint.BP_NPCSpawnPoint_C"));
	TestNotNull(TEXT("BP_NPCSpawnPoint Class loads"), SpawnPointClass);
	if (SpawnPointClass)
	{
		TestTrue(TEXT("BP_NPCSpawnPoint uses project Spawn Point parent"), SpawnPointClass->IsChildOf(ADroneNPCSpawnPoint::StaticClass()));
	}

	UWorld* World = LoadObject<UWorld>(nullptr, MapObjectPath);
	TestNotNull(TEXT("NPC Smart Object Greybox map loads from central Maps folder"), World);
	if (!World)
	{
		return false;
	}

	TestTrue(TEXT("NPC Greybox map keeps Prototype GameMode"), World->GetWorldSettings()->DefaultGameMode.Get() == GameModeClass);
	int32 PlayerStartCount = 0;
	int32 NavBoundsCount = 0;
	int32 NavigationFloorCount = 0;
	int32 RecastNavMeshCount = 0;
	int32 DynamicRecastNavMeshCount = 0;
	int32 StationCount = 0;
	int32 CoverStationCount = 0;
	TMap<UClass*, int32> ActualPlacedCounts;
	FBox NavigationBounds(EForceInit::ForceInit);
	TArray<const AActor*> NavigationUsers;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		PlayerStartCount += Actor->IsA<APlayerStart>() ? 1 : 0;
		if (Actor->GetClass()->GetPathName() == TEXT("/Script/NavigationSystem.NavMeshBoundsVolume"))
		{
			++NavBoundsCount;
			NavigationBounds += Actor->GetComponentsBoundingBox(true);
		}
		if (const ADroneNPCNavigationFloor* NavigationFloor = Cast<ADroneNPCNavigationFloor>(Actor))
		{
			++NavigationFloorCount;
			const UBoxComponent* NavigationCollision = NavigationFloor->GetNavigationCollision();
			TestNotNull(TEXT("NPC Greybox Navigation Floor owns Box collision"), NavigationCollision);
			if (NavigationCollision)
			{
				TestEqual(
					TEXT("NPC Greybox Navigation Floor blocks query and physics"),
					NavigationCollision->GetCollisionEnabled(),
					ECollisionEnabled::QueryAndPhysics);
				TestTrue(
					TEXT("NPC Greybox Navigation Floor affects navigation"),
					NavigationCollision->CanEverAffectNavigation());
			}
		}
		if (ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(Actor))
		{
			++RecastNavMeshCount;
			DynamicRecastNavMeshCount +=
				RecastNavMesh->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic ? 1 : 0;
		}
		if (const ADroneSmartObjectStation* Station = Cast<ADroneSmartObjectStation>(Actor))
		{
			++StationCount;
			CoverStationCount += Station->GetActivity() == EDroneSmartObjectActivity::Cover ? 1 : 0;
			NavigationUsers.Add(Actor);
		}
		if (Actor->IsA<ADroneNPCCharacter>())
		{
			ActualPlacedCounts.FindOrAdd(Actor->GetClass())++;
			NavigationUsers.Add(Actor);
		}
	}

	TestEqual(TEXT("NPC Greybox map has one PlayerStart"), PlayerStartCount, 1);
	TestEqual(TEXT("NPC Greybox map has one NavMeshBoundsVolume"), NavBoundsCount, 1);
	TestEqual(TEXT("NPC Greybox map has one native Navigation Floor"), NavigationFloorCount, 1);
	TestEqual(TEXT("NPC Greybox map saves one RecastNavMesh"), RecastNavMeshCount, 1);
	TestEqual(TEXT("NPC Greybox map uses runtime Dynamic Recast generation"), DynamicRecastNavMeshCount, 1);
	TestEqual(TEXT("NPC Greybox map has twelve Smart Object Stations"), StationCount, 12);
	TestEqual(TEXT("NPC Greybox map has two Cover Stations"), CoverStationCount, 2);
	for (const TPair<UClass*, int32>& Expected : ExpectedPlacedCounts)
	{
		TestEqual(
			*FString::Printf(TEXT("NPC Greybox map places expected count for %s"), *GetNameSafe(Expected.Key)),
			ActualPlacedCounts.FindRef(Expected.Key),
			Expected.Value);
	}
	for (const AActor* Actor : NavigationUsers)
	{
		TestTrue(
			*FString::Printf(TEXT("%s is inside the authored NavMesh bounds"), *GetNameSafe(Actor)),
			NavigationBounds.IsInsideOrOn(Actor->GetActorLocation()));
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneHostilePatrolStateTreeAssetTest,
	"Drone.AI.HostilePatrolStateTreeAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneHostilePatrolStateTreeAssetTest::RunTest(const FString& Parameters)
{
	using namespace DroneNPCGreybox;
	UStateTree* StateTree = LoadObject<UStateTree>(nullptr, HostilePatrolStateTreeObjectPath);
	TestNotNull(TEXT("Hostile Patrol StateTree loads"), StateTree);
	TestTrue(TEXT("Hostile Patrol StateTree is compiled and ready"), StateTree && StateTree->IsReadyToRun());
	TestTrue(
		TEXT("Hostile Patrol StateTree keeps the authored Claim-Move-Wait-Release contract"),
		UDroneAIStateTreeAuthoringLibrary::ValidateHostilePatrolStateTree(HostilePatrolStateTreePackage));
	TestTrue(
		TEXT("Hostile StateTree keeps DroneDetected-Search-Patrol event transitions"),
		UDroneAIStateTreeAuthoringLibrary::ValidateHostilePerceptionStateTree(HostilePatrolStateTreePackage));
	TestTrue(
		TEXT("Hostile StateTree keeps MG priority, Cover, and personal Weapon fallback transitions"),
		UDroneAIStateTreeAuthoringLibrary::ValidateHostileCoverStateTree(HostilePatrolStateTreePackage));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFriendlyBaseRoutineStateTreeAssetTest,
	"Drone.AI.FriendlyBaseRoutineStateTreeAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneFriendlyBaseRoutineStateTreeAssetTest::RunTest(const FString& Parameters)
{
	using namespace DroneNPCGreybox;
	UStateTree* StateTree = LoadObject<UStateTree>(nullptr, FriendlyBaseRoutineStateTreeObjectPath);
	TestNotNull(TEXT("Friendly Base Routine StateTree loads"), StateTree);
	TestTrue(TEXT("Friendly Base Routine StateTree is compiled and ready"), StateTree && StateTree->IsReadyToRun());
	TestTrue(
		TEXT("Friendly Base Routine StateTree keeps the authored Claim-Move-Wait-Release contract"),
		UDroneAIStateTreeAuthoringLibrary::ValidateFriendlyBaseRoutineStateTree(FriendlyBaseRoutineStateTreePackage));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCGreyboxPIETest,
	"Drone.AI.NPCGreyboxPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCGreyboxPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneNPCGreybox;
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains,
		2);

	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("NPC Greybox PIE test requires an idle Editor"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapPackage));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateNPCGreyboxPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCBaseRoutinesPIETest,
	"Drone.AI.NPCBaseRoutinesPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCBaseRoutinesPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneNPCGreybox;
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains,
		2);

	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("NPC Base Routines PIE test requires an idle Editor"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapPackage));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateNPCBaseRoutinesPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCPerceptionSearchPIETest,
	"Drone.AI.NPCPerceptionSearchPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCPerceptionSearchPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneNPCGreybox;
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains,
		2);

	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("NPC Perception Search PIE test requires an idle Editor"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapPackage));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateNPCPerceptionSearchPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
