#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/DroneImpactDetonationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Mission/DroneDefinition.h"
#include "Prototype/DronePrototypePawn.h"
#include "Signal/DroneSignalComponent.h"
#include "Tests/AutomationCommon.h"
#include "Weather/DroneWeatherResponseComponent.h"
#include "Weapons/DroneGroundWeaponComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneExtendedRoleTest,
	"Drone.Integration.ExtendedRoleDrones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneExtendedRoleTest::RunTest(const FString& Parameters)
{
	UDroneDefinition* FiberDefinition = LoadObject<UDroneDefinition>(
		nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_FiberOptic_Greybox.DA_Drone_FiberOptic_Greybox"));
	UDroneDefinition* GroundDefinition = LoadObject<UDroneDefinition>(
		nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_GroundUGV_Greybox.DA_Drone_GroundUGV_Greybox"));
	TestNotNull(TEXT("Fiber Optic Definition loads"), FiberDefinition);
	TestNotNull(TEXT("Ground UGV Definition loads"), GroundDefinition);
	if (!FiberDefinition || !GroundDefinition)
	{
		return false;
	}

	TestEqual(TEXT("Fiber uses the Fiber Optic mission role"),
		FiberDefinition->MissionRole, EDroneMissionRole::FiberOpticStrike);
	TestTrue(TEXT("Fiber implements jamming immunity"),
		FiberDefinition->ImplementedCapabilities.Contains(EDroneGameplayCapability::JammingImmunity));
	TestTrue(TEXT("Fiber implements impact detonation"),
		FiberDefinition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ImpactDetonation));
	TestEqual(TEXT("Ground uses the Ground UGV mission role"),
		GroundDefinition->MissionRole, EDroneMissionRole::GroundUGV);
	TestTrue(TEXT("Ground implements ground drive"),
		GroundDefinition->ImplementedCapabilities.Contains(EDroneGameplayCapability::GroundDrive));
	TestTrue(TEXT("Ground implements player ground weapons"),
		GroundDefinition->ImplementedCapabilities.Contains(EDroneGameplayCapability::GroundWeapons));

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();
	AStaticMeshActor* GroundPlane = World->SpawnActor<AStaticMeshActor>(
		FVector(0.0f, 0.0f, -50.0f),
		FRotator::ZeroRotator);
	UStaticMesh* GroundCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("Ground-drive regression floor spawns"), GroundPlane);
	TestNotNull(TEXT("Ground-drive regression floor mesh loads"), GroundCube);
	if (GroundPlane && GroundCube)
	{
		GroundPlane->GetStaticMeshComponent()->SetStaticMesh(GroundCube);
		GroundPlane->GetStaticMeshComponent()->SetWorldScale3D(FVector(20.0f, 20.0f, 1.0f));
		GroundPlane->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		GroundPlane->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	WorldWrapper.TickTestWorld();
	for (UDroneDefinition* Definition : {FiberDefinition, GroundDefinition})
	{
		UClass* PawnClass = Definition->PawnClass.LoadSynchronous();
		TestTrue(TEXT("Extended role Pawn is a Prototype Pawn subclass"),
			PawnClass && PawnClass->IsChildOf(ADronePrototypePawn::StaticClass()));
		if (!PawnClass)
		{
			continue;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector SpawnLocation = Definition == GroundDefinition
			? FVector(0.0f, 0.0f, 1200.0f)
			: FVector(0.0f, 0.0f, 300.0f);
		ADronePrototypePawn* Pawn = World->SpawnActor<ADronePrototypePawn>(
			PawnClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
		TestNotNull(TEXT("Extended role Pawn spawns"), Pawn);
		if (!Pawn)
		{
			continue;
		}
		TestTrue(TEXT("Extended role Definition applies"), Pawn->ApplyDroneDefinition(Definition));

		if (Definition == FiberDefinition)
		{
			TestTrue(TEXT("Fiber Pawn ignores jamming"), Pawn->GetSignalComponent()->IsJammingImmune());
			TestTrue(TEXT("Fiber Pawn exposes collision detonation"),
				Pawn->GetImpactDetonationComponent()->IsFeatureEnabled());
			TInlineComponentArray<UStaticMeshComponent*> StaticMeshes;
			Pawn->GetComponents(StaticMeshes);
			const TSet<FString> ExpectedFiberMeshPaths = {
				TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_DroneFPVBody.SM_DroneFPVBody"),
				TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_RotorA.SM_RotorA"),
				TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_RotorB.SM_RotorB"),
				TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_RotorC.SM_RotorC"),
				TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_RotorD.SM_RotorD")
			};
			TSet<FString> ActualFiberMeshPaths;
			for (const UStaticMeshComponent* Component : StaticMeshes)
			{
				if (Component && Component->ComponentHasTag(TEXT("DroneRoleVisual")) && Component->GetStaticMesh())
				{
					ActualFiberMeshPaths.Add(Component->GetStaticMesh()->GetPathName());
				}
			}
			TestTrue(
				TEXT("Fiber Pawn reuses the same FPV body and four rotor meshes as the suicide drone"),
				ActualFiberMeshPaths.Num() == ExpectedFiberMeshPaths.Num()
					&& ActualFiberMeshPaths.Includes(ExpectedFiberMeshPaths));
			const UStaticMeshComponent* FiberSpool = nullptr;
			for (const UStaticMeshComponent* Component : StaticMeshes)
			{
				if (Component && Component->ComponentHasTag(TEXT("FiberSpoolVisual")))
				{
					FiberSpool = Component;
					break;
				}
			}
			TestTrue(TEXT("Fiber Pawn exposes an empty replaceable spool mesh slot at a configured mount"),
				FiberSpool
					&& FiberSpool->GetStaticMesh() == nullptr
					&& !FiberSpool->GetRelativeLocation().IsNearlyZero());
			TInlineComponentArray<USplineComponent*> Splines;
			Pawn->GetComponents(Splines);
			const USplineComponent* FiberSpline = nullptr;
			for (const USplineComponent* Component : Splines)
			{
				if (Component && Component->ComponentHasTag(TEXT("FiberOpticSpline")))
				{
					FiberSpline = Component;
					break;
				}
			}
			TestTrue(TEXT("Fiber Pawn builds a trailing fiber spline from its spool"),
				FiberSpline && FiberSpline->GetNumberOfSplinePoints() >= 4);
			if (FiberSpline && FiberSpline->GetNumberOfSplinePoints() >= 4)
			{
				TestTrue(TEXT("Fiber hanging section uses non-zero smooth spline tangents"),
					!FiberSpline->GetTangentAtSplinePoint(
						1,
						ESplineCoordinateSpace::World).IsNearlyZero());
			}
			TInlineComponentArray<USplineMeshComponent*> SplineMeshes;
			Pawn->GetComponents(SplineMeshes);
			TestTrue(TEXT("Fiber spline has at least one visible cable segment"),
				SplineMeshes.ContainsByPredicate([](const USplineMeshComponent* Component)
				{
					return Component && Component->ComponentHasTag(TEXT("FiberOpticCableSegment"))
						&& Component->IsVisible() && Component->GetStaticMesh();
				}));
		}
		else
		{
			TestTrue(TEXT("Ground Pawn enters Ground Drive mode"), Pawn->IsGroundDriveModeActive());
			TestTrue(TEXT("Ground Pawn enables its player weapon component"),
				Pawn->GetGroundWeaponComponent() && Pawn->GetGroundWeaponComponent()->IsFeatureEnabled());
			TestFalse(TEXT("Ground Pawn does not receive airborne wind drift"),
				Pawn->GetWeatherResponseComponent()->IsWindResponseEnabled());
			TestEqual(TEXT("Ground Drive is held in assisted controls"),
				Pawn->GetControlMode(), EDroneControlMode::AssistedEasy);
			TInlineComponentArray<UPoseableMeshComponent*> PoseableMeshes;
			Pawn->GetComponents(PoseableMeshes);
			UPoseableMeshComponent* GroundVisual = nullptr;
			for (UPoseableMeshComponent* Component : PoseableMeshes)
			{
				if (Component && Component->ComponentHasTag(TEXT("GroundDroneVisual")))
				{
					GroundVisual = Component;
					break;
				}
			}
			TestTrue(
				TEXT("Ground Pawn uses a poseable GC Drone 1 visual without modifying vendor Skeletons"),
				GroundVisual
					&& GroundVisual->GetSkinnedAsset()
					&& GroundVisual->GetSkinnedAsset()->GetPathName().Contains(TEXT("GC_Drone_1_SK")));
			if (GroundVisual)
			{
				TestTrue(TEXT("Ground visual exposes a Turret yaw bone"), GroundVisual->GetBoneIndex(TEXT("Turret")) != INDEX_NONE);
				TestTrue(TEXT("Ground visual exposes a Turret_Swivel pitch bone"), GroundVisual->GetBoneIndex(TEXT("Turret_Swivel")) != INDEX_NONE);
			}
			TestNotNull(TEXT("Ground Pawn exposes an upper yaw pivot"), Pawn->GetGroundUpperYawPivot());
			TestNotNull(TEXT("Ground Pawn exposes a weapon pitch pivot"), Pawn->GetGroundWeaponPitchPivot());
			TestNotNull(TEXT("Ground Pawn exposes a future gun muzzle anchor"), Pawn->GetGroundGunMuzzleAnchor());
			TestNotNull(TEXT("Ground Pawn exposes a future grenade muzzle anchor"), Pawn->GetGroundGrenadeMuzzleAnchor());
			const FRotator ChassisRotationBeforeAim = Pawn->GetActorRotation();
			const FRotator TurretRotationBeforeAim = GroundVisual
				? GroundVisual->GetBoneRotationByName(TEXT("Turret"), EBoneSpaces::ComponentSpace)
				: FRotator::ZeroRotator;
			Pawn->SetGroundUpperAimGreybox(45.0f, 12.0f);
			TestTrue(TEXT("Ground camera aim does not rotate the chassis"), Pawn->GetActorRotation().Equals(ChassisRotationBeforeAim, 0.01f));
			TestEqual(TEXT("Ground upper yaw pivot follows camera yaw"), Pawn->GetGroundUpperYawDegrees(), 45.0f);
			TestEqual(TEXT("Ground weapon pitch pivot follows camera pitch"), Pawn->GetGroundWeaponPitchDegrees(), 12.0f);
			if (GroundVisual)
			{
				TestFalse(
					TEXT("Ground upper aim rotates the Turret bone instead of the whole chassis"),
					GroundVisual->GetBoneRotationByName(TEXT("Turret"), EBoneSpaces::ComponentSpace)
						.Equals(TurretRotationBeforeAim, 0.01f));
			}
			for (int32 Step = 0; Step < 5; ++Step)
			{
				Pawn->Tick(0.1f);
			}
			TestTrue(TEXT("Ground Pawn spawned above the map acquires and returns to the ground"),
				Pawn->GetActorLocation().Z < 200.0f);
		}
		Pawn->Destroy();
	}

	WorldWrapper.TickTestWorld();
	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif
