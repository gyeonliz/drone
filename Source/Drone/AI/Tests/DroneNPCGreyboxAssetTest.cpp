#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/DroneAITags.h"
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
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Perception/AIPerceptionComponent.h"
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
		case EPhase::WaitForSearch:
			return UpdateSearch(Now, Hostiles, Friendlies);
		case EPhase::WaitForReturn:
			return UpdateReturn(Now, Hostiles, Friendlies);
		default:
			return true;
		}
	}

private:
	enum class EPhase : uint8
	{
		WaitForBaseline,
		WaitForDetected,
		WaitForSearch,
		WaitForReturn
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
			bHostilesDetected &= Controller->HasDetectedDrone()
				&& Controller->GetResponseState() == EDroneNPCAIResponseState::DroneDetected
				&& Controller->GetDroneDetectionCount() == 1
				&& !Controller->GetReservationComponent()->HasValidReservation();
		}
		bool bFriendliesUnaffected = true;
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			bFriendliesUnaffected &= !Controller->HasDetectedDrone()
				&& Controller->GetResponseState() == EDroneNPCAIResponseState::Patrol
				&& Controller->GetDroneDetectionCount() == 0;
		}
		if (!bHostilesDetected || !bFriendliesUnaffected)
		{
			return FinishWithTimeout(Now, 5.0, TEXT("Hostile-only DroneDetected response did not become stable"));
		}

		bool bCommonWeaponPathReady = true;
		for (ADroneNPCAIController* Controller : Hostiles)
		{
			UDroneNPCWeaponComponent* WeaponComponent = Controller->GetPossessedWeaponComponent();
			bCommonWeaponPathReady &= Controller->CanFirePersonalWeapon()
				&& Controller->StartPersonalWeaponFire()
				&& WeaponComponent
				&& WeaponComponent->IsFiring()
				&& WeaponComponent->GetCurrentTarget() == Drone
				&& WeaponComponent->GetCurrentAimPoint().Equals(Drone->GetActorLocation());
		}
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			const UDroneNPCWeaponComponent* WeaponComponent = Controller->GetPossessedWeaponComponent();
			bCommonWeaponPathReady &= !Controller->CanFirePersonalWeapon()
				&& WeaponComponent
				&& !WeaponComponent->IsFiring();
		}
		if (!bCommonWeaponPathReady)
		{
			Test->AddError(TEXT("Rifle and Shotgun did not share the detected Target/Aim Point Weapon path"));
			return true;
		}

		for (ADroneNPCAIController* Controller : Hostiles)
		{
			BroadcastSight(Controller, Drone, false);
		}
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
			const UDroneNPCWeaponComponent* WeaponComponent = Controller->GetPossessedWeaponComponent();
			bAllSearching &= !Controller->HasDetectedDrone()
				&& Controller->GetResponseState() == EDroneNPCAIResponseState::Search
				&& Controller->GetDroneLostCount() == 1
				&& Controller->GetDroneSearchStartCount() == 1
				&& Controller->HasLastKnownDroneLocation()
				&& WeaponComponent
				&& !WeaponComponent->IsFiring()
				&& WeaponComponent->GetCurrentTarget() == nullptr;
		}
		bool bFriendliesUnaffected = true;
		for (const ADroneNPCAIController* Controller : Friendlies)
		{
			bFriendliesUnaffected &= Controller->GetResponseState() == EDroneNPCAIResponseState::Patrol
				&& Controller->GetDroneLostCount() == 0;
		}
		if (!bAllSearching || !bFriendliesUnaffected)
		{
			return FinishWithTimeout(Now, 5.0, TEXT("DroneLost did not transition both Hostiles into Search"));
		}

		AdvanceTo(EPhase::WaitForReturn, Now);
		return false;
	}

	bool UpdateReturn(
		const double Now,
		const TArray<ADroneNPCAIController*>& Hostiles,
		const TArray<ADroneNPCAIController*>& Friendlies)
	{
		bool bHostilesReturned = true;
		for (const ADroneNPCAIController* Controller : Hostiles)
		{
			const bool bPatrolWorkResumed = Controller->GetReservationComponent()->HasValidReservation()
				|| Controller->GetCompletedPatrolCycles() > BaselineHostileCycles.FindRef(Controller);
			bHostilesReturned &= Controller->GetResponseState() == EDroneNPCAIResponseState::Patrol
				&& Controller->GetCompletedDroneSearchCount() == 1
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

		Test->TestTrue(TEXT("Both Hostiles return to Patrol after one Search"), bHostilesReturned);
		Test->TestTrue(TEXT("Both Friendlies continue BaseRoutine during Hostile perception response"), bFriendliesContinued);
		return true;
	}

	FAutomationTestBase* Test;
	EPhase Phase = EPhase::WaitForBaseline;
	double StartedAt = 0.0;
	double PhaseStartedAt = 0.0;
	TMap<TWeakObjectPtr<ADroneNPCAIController>, int32> BaselineHostileCycles;
	TMap<TWeakObjectPtr<ADroneNPCAIController>, int32> BaselineFriendlyCycles;
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
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, GameModeClassPath);
	TestNotNull(TEXT("Manny Greybox Mesh loads"), MannyMesh);
	TestNotNull(TEXT("Unarmed Anim Blueprint Class loads"), UnarmedAnimClass);
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
		TestTrue(*FString::Printf(TEXT("BP_NPC_%s uses shared Unarmed Anim BP"), Expectation.Name), CDO->GetMesh()->GetAnimClass() == UnarmedAnimClass);
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
		if (Actor->IsA<ADroneSmartObjectStation>())
		{
			++StationCount;
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
	TestEqual(TEXT("NPC Greybox map has ten Smart Object Stations"), StationCount, 10);
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
