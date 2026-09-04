#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/DroneAITags.h"
#include "AI/DroneAutomaticTurret.h"
#include "AI/DroneNPCAIController.h"
#include "AI/DroneNPCCharacter.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/DroneNPCSpawnPoint.h"
#include "AI/DroneSmartObjectReservationComponent.h"
#include "AI/DroneMGTurretStation.h"
#include "AI/DroneSmartObjectStation.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Prototype/DronePrototypePawn.h"
#include "SmartObjectComponent.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectUserComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSmartObjectFoundationTest,
	"Drone.AI.SmartObjectFoundationDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneSmartObjectFoundationTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Drone NPC Character is concrete"), ADroneNPCCharacter::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("Drone NPC AI Controller is concrete"), ADroneNPCAIController::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("NPC Spawn Point is concrete"), ADroneNPCSpawnPoint::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("Smart Object Station is concrete"), ADroneSmartObjectStation::StaticClass()->HasAnyClassFlags(CLASS_Abstract));

	const ADroneNPCCharacter* NPCDefaults = GetDefault<ADroneNPCCharacter>();
	TestNotNull(TEXT("NPC Character CDO exists"), NPCDefaults);
	if (NPCDefaults)
	{
		TestTrue(TEXT("NPC uses project AI Controller"), NPCDefaults->AIControllerClass == ADroneNPCAIController::StaticClass());
		TestTrue(TEXT("NPC auto-possesses when placed or spawned"), NPCDefaults->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned);
		TestNotNull(TEXT("NPC owns a Profile Component"), NPCDefaults->GetNPCProfileComponent());
		TestNotNull(TEXT("NPC owns a Smart Object User Component"), NPCDefaults->GetSmartObjectUserComponent());
		TestNotNull(TEXT("NPC owns the common Weapon Component"), NPCDefaults->GetNPCWeaponComponent());
	}

	const ADroneNPCAIController* ControllerDefaults = GetDefault<ADroneNPCAIController>();
	TestNotNull(TEXT("NPC AI Controller CDO exists"), ControllerDefaults);
	if (ControllerDefaults)
	{
		TestNotNull(TEXT("Controller owns StateTree AI"), ControllerDefaults->GetStateTreeAIComponent());
		TestNotNull(TEXT("Controller owns AI Perception"), ControllerDefaults->GetDronePerceptionComponent());
		TestNotNull(TEXT("Controller owns reservation lifecycle"), ControllerDefaults->GetReservationComponent());
		TestFalse(TEXT("Controller starts without a fake detected Drone"), ControllerDefaults->HasDetectedDrone());
	}

	const ADroneSmartObjectStation* StationDefaults = GetDefault<ADroneSmartObjectStation>();
	TestNotNull(TEXT("Smart Object Station CDO exists"), StationDefaults);
	if (StationDefaults)
	{
		TestFalse(TEXT("Station avoids per-frame Actor Tick"), StationDefaults->PrimaryActorTick.bCanEverTick);
		TestNotNull(TEXT("Station owns Smart Object Component"), StationDefaults->GetSmartObjectComponent());
		TestNull(TEXT("Generic Station has no MG base mount"), StationDefaults->GetMGTurretBaseMount());
		TestNull(TEXT("Generic Station has no MG Yaw pivot"), StationDefaults->GetMGTurretYawPivot());
		TestNull(TEXT("Generic Station has no MG Pitch pivot"), StationDefaults->GetMGTurretAimPivot());
		TestNull(TEXT("Generic Station has no MG Muzzle"), StationDefaults->GetMGTurretMuzzle());
		TestNotNull(TEXT("Station owns slot facing preview"), StationDefaults->GetSlotFacingPreview());
		if (StationDefaults->GetSlotFacingPreview())
		{
			TestTrue(
				TEXT("Slot facing preview follows the Smart Object Component transform"),
				StationDefaults->GetSlotFacingPreview()->GetAttachParent() == StationDefaults->GetSmartObjectComponent());
		}
		TestTrue(TEXT("Default station activity tag is valid"), StationDefaults->GetExpectedActivityTag().IsValid());
		TestFalse(TEXT("Native station does not hide a missing Definition"), StationDefaults->HasSmartObjectDefinition());
	}

	const ADroneMGTurretStation* TurretDefaults = GetDefault<ADroneMGTurretStation>();
	TestNotNull(TEXT("Dedicated MG Turret CDO exists"), TurretDefaults);
	if (TurretDefaults)
	{
		TestTrue(TEXT("Dedicated MG Turret uses the MG activity"), TurretDefaults->GetActivity() == EDroneSmartObjectActivity::MGTurret);
		TestFalse(TEXT("Dedicated MG Turret also avoids per-frame Actor Tick"), TurretDefaults->PrimaryActorTick.bCanEverTick);
		TestNotNull(TEXT("MG Turret owns fixed base mount"), TurretDefaults->GetMGTurretBaseMount());
		TestNotNull(TEXT("MG Turret owns separate Yaw pivot"), TurretDefaults->GetMGTurretYawPivot());
		TestNotNull(TEXT("MG Turret owns separate Pitch pivot"), TurretDefaults->GetMGTurretAimPivot());
		TestNotNull(TEXT("MG Turret owns barrel-relative Muzzle"), TurretDefaults->GetMGTurretMuzzle());
		TestNotNull(TEXT("MG Turret owns exactly assigned base mesh"), TurretDefaults->GetMGTurretBaseMesh());
		TestNotNull(TEXT("MG Turret owns exactly assigned body mesh"), TurretDefaults->GetMGTurretBodyMesh());
		TestNotNull(TEXT("MG Turret owns exactly assigned barrel mesh"), TurretDefaults->GetMGTurretBarrelMesh());
		TestNotNull(TEXT("MG Turret owns a Blueprint-adjustable operator anchor"), TurretDefaults->GetMGTurretOperatorAnchor());
		TestEqual(TEXT("MG operator default rear distance"), TurretDefaults->GetMGTurretOperatorDistance(), 120.0f);
		if (TurretDefaults->GetMGTurretBaseMount()
			&& TurretDefaults->GetMGTurretYawPivot()
			&& TurretDefaults->GetMGTurretAimPivot()
			&& TurretDefaults->GetMGTurretMuzzle()
			&& TurretDefaults->GetMGTurretBaseMesh()
			&& TurretDefaults->GetMGTurretBodyMesh()
			&& TurretDefaults->GetMGTurretBarrelMesh()
			&& TurretDefaults->GetMGTurretOperatorAnchor())
		{
			TestTrue(TEXT("MG Yaw pivot follows fixed base"), TurretDefaults->GetMGTurretYawPivot()->GetAttachParent() == TurretDefaults->GetMGTurretBaseMount());
			TestTrue(TEXT("MG Pitch pivot follows Yaw pivot"), TurretDefaults->GetMGTurretAimPivot()->GetAttachParent() == TurretDefaults->GetMGTurretYawPivot());
			TestTrue(TEXT("MG Muzzle follows Pitch pivot"), TurretDefaults->GetMGTurretMuzzle()->GetAttachParent() == TurretDefaults->GetMGTurretAimPivot());
			TestTrue(TEXT("MG base mesh stays fixed"), TurretDefaults->GetMGTurretBaseMesh()->GetAttachParent() == TurretDefaults->GetMGTurretBaseMount());
			TestTrue(TEXT("MG body mesh follows Yaw only"), TurretDefaults->GetMGTurretBodyMesh()->GetAttachParent() == TurretDefaults->GetMGTurretYawPivot());
			TestTrue(TEXT("MG barrel mesh follows Pitch"), TurretDefaults->GetMGTurretBarrelMesh()->GetAttachParent() == TurretDefaults->GetMGTurretAimPivot());
			TestTrue(TEXT("MG operator anchor follows the Yaw body"), TurretDefaults->GetMGTurretOperatorAnchor()->GetAttachParent() == TurretDefaults->GetMGTurretYawPivot());
			TestTrue(TEXT("MG operator defaults behind the Yaw body at ground height"), TurretDefaults->GetMGTurretOperatorAnchor()->GetRelativeLocation().Equals(FVector(-120.0f, 0.0f, -55.0f), 0.01f));
			TestTrue(TEXT("MG operator inherits body Yaw without a separate rotation"), TurretDefaults->GetMGTurretOperatorAnchor()->GetRelativeRotation().IsNearlyZero(0.01f));
			const UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
			TestNotNull(TEXT("Temporary Cylinder mesh loads"), CylinderMesh);
			TestTrue(TEXT("MG base uses temporary Cylinder"), TurretDefaults->GetMGTurretBaseMesh()->GetStaticMesh() == CylinderMesh);
			TestTrue(TEXT("MG body uses temporary Cylinder"), TurretDefaults->GetMGTurretBodyMesh()->GetStaticMesh() == CylinderMesh);
			TestTrue(TEXT("MG barrel uses temporary Cylinder"), TurretDefaults->GetMGTurretBarrelMesh()->GetStaticMesh() == CylinderMesh);
		}
	}

	const ADroneEmplacedAutomaticTurret* EmplacedDefaults = GetDefault<ADroneEmplacedAutomaticTurret>();
	const ADroneVehicleAutomaticTurret* VehicleDefaults = GetDefault<ADroneVehicleAutomaticTurret>();
	TestNotNull(TEXT("Emplaced automatic turret CDO exists"), EmplacedDefaults);
	TestNotNull(TEXT("Vehicle automatic turret CDO exists"), VehicleDefaults);
	if (EmplacedDefaults)
	{
		TestTrue(TEXT("Emplaced automatic turret ticks for autonomous tracking"), EmplacedDefaults->PrimaryActorTick.bCanEverTick);
		TestTrue(TEXT("Emplaced automatic turret exposes its mount type"),
			EmplacedDefaults->GetMountType() == EDroneAutomaticTurretMountType::Emplaced);
		TestTrue(TEXT("Emplaced automatic turret is enabled by default"), EmplacedDefaults->IsAutomaticTurretEnabled());
		TestTrue(TEXT("Emplaced automatic turret detection uses hysteresis"),
			EmplacedDefaults->GetLoseTargetRange() >= EmplacedDefaults->GetDetectionRange());
		TestTrue(TEXT("Emplaced automatic turret requires line of sight"), EmplacedDefaults->RequiresTargetLineOfSight());
		TestFalse(TEXT("Emplaced automatic turret is not a claimable Smart Object"), EmplacedDefaults->HasSmartObjectDefinition());
	}
	if (VehicleDefaults)
	{
		TestTrue(TEXT("Vehicle automatic turret exposes its mount type"),
			VehicleDefaults->GetMountType() == EDroneAutomaticTurretMountType::VehicleMounted);
		TestTrue(TEXT("Vehicle automatic turret is enabled by default"), VehicleDefaults->IsAutomaticTurretEnabled());
		TestTrue(TEXT("Vehicle automatic turret has a longer detection range than emplaced default"),
			!EmplacedDefaults || VehicleDefaults->GetDetectionRange() > EmplacedDefaults->GetDetectionRange());
		TestFalse(TEXT("Vehicle automatic turret is not a claimable Smart Object"), VehicleDefaults->HasSmartObjectDefinition());
	}

	const ADroneNPCSpawnPoint* SpawnDefaults = GetDefault<ADroneNPCSpawnPoint>();
	TestNotNull(TEXT("NPC Spawn Point CDO exists"), SpawnDefaults);
	if (SpawnDefaults)
	{
		TestFalse(TEXT("Spawn Point avoids per-frame Actor Tick"), SpawnDefaults->PrimaryActorTick.bCanEverTick);
		TestFalse(TEXT("Spawn Point does not mutate a Map by default"), SpawnDefaults->ShouldSpawnOnBeginPlay());
		TestEqual(TEXT("Spawn Point defaults to one NPC"), SpawnDefaults->GetConfiguredSpawnCount(), 1);
		TestTrue(TEXT("Single default NPC spawns at the marker"), SpawnDefaults->GetSpawnTransformForIndex(0).Equals(SpawnDefaults->GetActorTransform()));
	}

	UDroneNPCProfileComponent* Profile = NewObject<UDroneNPCProfileComponent>();
	TestNotNull(TEXT("Transient NPC Profile can be created"), Profile);
	if (Profile)
	{
		FDroneNPCProfile HostileRifleProfile;
		HostileRifleProfile.Faction = EDroneNPCFaction::Hostile;
		HostileRifleProfile.WeaponType = EDroneNPCWeaponType::Rifle;
		HostileRifleProfile.bCanUseMGTurret = true;
		Profile->SetProfile(HostileRifleProfile);

		const FGameplayTagContainer UserTags = Profile->BuildSmartObjectUserTags();
		TestTrue(TEXT("Hostile Profile emits Hostile tag"), UserTags.HasTagExact(DroneAITags::Faction_Hostile));
		TestTrue(TEXT("Rifle Profile emits Rifle tag"), UserTags.HasTagExact(DroneAITags::Weapon_Rifle));
		TestTrue(TEXT("Eligible Hostile emits MG operator tag"), UserTags.HasTagExact(DroneAITags::Role_MGTurretOperator));
		TestFalse(TEXT("Hostile Profile never emits Friendly tag"), UserTags.HasTagExact(DroneAITags::Faction_Friendly));
	}

	const ADronePrototypePawn* DroneDefaults = GetDefault<ADronePrototypePawn>();
	TestNotNull(TEXT("Prototype Drone CDO exists"), DroneDefaults);
	if (DroneDefaults)
	{
		TestNotNull(TEXT("Prototype Drone is an explicit perception source"), DroneDefaults->GetPerceptionStimuliSource());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSmartObjectStationAssetsTest,
	"Drone.AI.SmartObjectStationAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneSmartObjectStationAssetsTest::RunTest(const FString& Parameters)
{
	struct FStationAssetExpectation
	{
		const TCHAR* Name;
		EDroneSmartObjectActivity Activity;
		FGameplayTag ActivityTag;
		bool bIsMGTurret;
	};

	const FStationAssetExpectation Expectations[] =
	{
		{TEXT("EnemyPatrol"), EDroneSmartObjectActivity::EnemyPatrol, DroneAITags::Activity_EnemyPatrol, false},
		{TEXT("FriendlyBasePatrol"), EDroneSmartObjectActivity::FriendlyBasePatrol, DroneAITags::Activity_FriendlyBasePatrol, false},
		{TEXT("Ambient"), EDroneSmartObjectActivity::Ambient, DroneAITags::Activity_Ambient, false},
		{TEXT("Guard"), EDroneSmartObjectActivity::Guard, DroneAITags::Activity_Guard, false},
		{TEXT("Cover"), EDroneSmartObjectActivity::Cover, DroneAITags::Activity_Cover, false},
		{TEXT("MGTurret"), EDroneSmartObjectActivity::MGTurret, DroneAITags::Activity_MGTurret, true},
	};

	for (const FStationAssetExpectation& Expectation : Expectations)
	{
		const FString DefinitionName = FString::Printf(TEXT("SO_Def_%s"), Expectation.Name);
		const FString DefinitionPath = FString::Printf(
			TEXT("/Game/Drone/AI/SmartObjects/Definitions/%s.%s"),
			*DefinitionName,
			*DefinitionName);
		USmartObjectDefinition* Definition = LoadObject<USmartObjectDefinition>(nullptr, *DefinitionPath);
		TestNotNull(*FString::Printf(TEXT("%s Definition loads"), Expectation.Name), Definition);
		if (!Definition)
		{
			continue;
		}

		TestTrue(*FString::Printf(TEXT("%s Definition validates"), Expectation.Name), Definition->Validate());
		TestEqual(*FString::Printf(TEXT("%s has one Slot"), Expectation.Name), Definition->GetSlots().Num(), 1);
		if (Definition->GetSlots().Num() == 1)
		{
			FGameplayTagContainer ActivityTags;
			Definition->GetSlotActivityTags(0, ActivityTags);
			TestTrue(
				*FString::Printf(TEXT("%s Slot has its Activity Tag"), Expectation.Name),
				ActivityTags.HasTagExact(Expectation.ActivityTag));

			const UGameplayInteractionSmartObjectBehaviorDefinition* Behavior =
				Cast<UGameplayInteractionSmartObjectBehaviorDefinition>(Definition->GetBehaviorDefinition(
					0,
					UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass()));
			TestNotNull(*FString::Printf(TEXT("%s has Gameplay Interaction Behavior"), Expectation.Name), Behavior);
			if (Behavior)
			{
				TestNull(
					*FString::Printf(TEXT("%s Interaction StateTree is intentionally deferred"), Expectation.Name),
					Behavior->GetStateTree());
			}
		}

		const FString BlueprintName = FString::Printf(TEXT("BP_SO_%s"), Expectation.Name);
		const FString BlueprintPath = FString::Printf(
			TEXT("/Game/Drone/AI/SmartObjects/Blueprints/%s.%s"),
			*BlueprintName,
			*BlueprintName);
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		TestNotNull(*FString::Printf(TEXT("%s Station Blueprint loads"), Expectation.Name), Blueprint);
		if (!Blueprint || !Blueprint->GeneratedClass)
		{
			continue;
		}

		TestTrue(
			*FString::Printf(TEXT("%s Station uses project parent"), Expectation.Name),
			Blueprint->GeneratedClass->IsChildOf(ADroneSmartObjectStation::StaticClass()));
		TestTrue(
			*FString::Printf(TEXT("%s Station uses the exact intended native parent"), Expectation.Name),
			Blueprint->ParentClass == (Expectation.bIsMGTurret
				? ADroneMGTurretStation::StaticClass()
				: ADroneSmartObjectStation::StaticClass()));
		const ADroneSmartObjectStation* Station = Cast<ADroneSmartObjectStation>(Blueprint->GeneratedClass->GetDefaultObject());
		TestNotNull(*FString::Printf(TEXT("%s Station CDO exists"), Expectation.Name), Station);
		if (!Station)
		{
			continue;
		}

		TestTrue(
			*FString::Printf(TEXT("%s Station Activity matches"), Expectation.Name),
			Station->GetActivity() == Expectation.Activity);
		TestTrue(
			*FString::Printf(TEXT("%s Station expected Tag matches"), Expectation.Name),
			Station->GetExpectedActivityTag().MatchesTagExact(Expectation.ActivityTag));
		TestTrue(
			*FString::Printf(TEXT("%s Station Definition matches"), Expectation.Name),
			Station->GetSmartObjectDefinition() == Definition);
		if (Expectation.bIsMGTurret)
		{
			const ADroneMGTurretStation* Turret = Cast<ADroneMGTurretStation>(Station);
			TestNotNull(TEXT("MG Station CDO uses dedicated 3-part class"), Turret);
			if (Turret)
			{
				TestNotNull(TEXT("MG Station has temporary base mesh"), Turret->GetMGTurretBaseMesh());
				TestNotNull(TEXT("MG Station has temporary body mesh"), Turret->GetMGTurretBodyMesh());
				TestNotNull(TEXT("MG Station has temporary barrel mesh"), Turret->GetMGTurretBarrelMesh());
				if (Turret->GetMGTurretBaseMount()
					&& Turret->GetMGTurretYawPivot()
					&& Turret->GetMGTurretAimPivot())
				{
					TestTrue(TEXT("MG Asset Yaw pivot follows fixed base"), Turret->GetMGTurretYawPivot()->GetAttachParent() == Turret->GetMGTurretBaseMount());
					TestTrue(TEXT("MG Asset Pitch pivot follows Yaw pivot"), Turret->GetMGTurretAimPivot()->GetAttachParent() == Turret->GetMGTurretYawPivot());
				}
			}
		}
		else
		{
			TestNull(*FString::Printf(TEXT("%s has no MG base component"), Expectation.Name), Station->GetMGTurretBaseMount());
			TestNull(*FString::Printf(TEXT("%s has no MG Yaw component"), Expectation.Name), Station->GetMGTurretYawPivot());
			TestNull(*FString::Printf(TEXT("%s has no MG Pitch component"), Expectation.Name), Station->GetMGTurretAimPivot());
			TestNull(*FString::Printf(TEXT("%s has no MG Muzzle component"), Expectation.Name), Station->GetMGTurretMuzzle());
		}
	}

	return true;
}

#endif
