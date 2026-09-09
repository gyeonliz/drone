#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/DroneDroppedPayload.h"
#include "Abilities/DronePayloadDropComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Mission/DroneDefinition.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationCommon.h"

namespace DroneRoleIntegrationAssets
{
struct FRoleAssetContract
{
	const TCHAR* DefinitionPath;
	const TCHAR* PawnClassPath;
	TSet<FString> ExpectedRoleMeshes;
	int32 ExpectedRotorCount;
};

const FRoleAssetContract Scout = {
	TEXT("/Game/Drone/Data/Drones/DA_Drone_Scout_Greybox.DA_Drone_Scout_Greybox"),
	TEXT("/Game/Drone/Integrations/RoleDrones/BP_DroneScoutIntegration.BP_DroneScoutIntegration_C"),
	{
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01Body.SM_Drone01Body"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_pCamera.SM_Drone01_pCamera"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r1.SM_Drone01_r1"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r2.SM_Drone01_r2"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r3.SM_Drone01_r3"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r4.SM_Drone01_r4")
	},
	4
};

const FRoleAssetContract Drop = {
	TEXT("/Game/Drone/Data/Drones/DA_Drone_Drop_Greybox.DA_Drone_Drop_Greybox"),
	TEXT("/Game/Drone/Integrations/RoleDrones/BP_DroneDropIntegration.BP_DroneDropIntegration_C"),
	{
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_DeliveryBody.SM_DeliveryBody"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_CAM.SM_CAM"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R1.SM_R1"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R2.SM_R2"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R3.SM_R3"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R4.SM_R4"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R5.SM_R5"),
		TEXT("/Game/Drone/ThirdParty/DronePack/D_Mesh/Delivery/SM_R6.SM_R6")
	},
	6
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneRoleIntegrationAssetTest,
	"Drone.Integration.RoleDroneAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneRoleIntegrationAssetTest::RunTest(const FString& Parameters)
{
	UClass* CarryablePayloadClass = LoadClass<ADroneDroppedPayload>(
		nullptr,
		TEXT("/Game/Drone/Abilities/Payload/BP_DroneCarryablePayload.BP_DroneCarryablePayload_C"));
	TestNotNull(TEXT("Map-placeable carryable Payload Blueprint loads"), CarryablePayloadClass);
	if (CarryablePayloadClass)
	{
		const ADroneDroppedPayload* PayloadDefaults = CarryablePayloadClass->GetDefaultObject<ADroneDroppedPayload>();
		TestTrue(TEXT("Carryable Payload Blueprint starts as an available pickup"),
			PayloadDefaults && PayloadDefaults->DoesStartAsCarryablePickup());
		TestTrue(TEXT("Carryable Payload Blueprint uses a visible crate mesh"),
			PayloadDefaults
				&& PayloadDefaults->GetPayloadVisual()
				&& PayloadDefaults->GetPayloadVisual()->GetStaticMesh()
				&& PayloadDefaults->GetPayloadVisual()->GetStaticMesh()->GetName().Contains(TEXT("Crate")));
	}

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("Role Drone asset test World exists"), World);
	if (!World)
	{
		return false;
	}

	const TArray<DroneRoleIntegrationAssets::FRoleAssetContract> Contracts = {
		DroneRoleIntegrationAssets::Scout,
		DroneRoleIntegrationAssets::Drop
	};
	for (const DroneRoleIntegrationAssets::FRoleAssetContract& Contract : Contracts)
	{
		UDroneDefinition* Definition = LoadObject<UDroneDefinition>(nullptr, Contract.DefinitionPath);
		UClass* PawnClass = LoadClass<ADronePrototypePawn>(nullptr, Contract.PawnClassPath);
		TestNotNull(TEXT("Role Definition loads"), Definition);
		TestNotNull(TEXT("Role Pawn class loads"), PawnClass);
		if (!Definition || !PawnClass)
		{
			continue;
		}
		TestEqual(TEXT("Role Definition points at the expected distinct Pawn"), Definition->PawnClass.LoadSynchronous(), PawnClass);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADronePrototypePawn* Pawn = World->SpawnActor<ADronePrototypePawn>(PawnClass, FTransform::Identity, SpawnParameters);
		TestNotNull(TEXT("Role-specific Pawn spawns"), Pawn);
		if (!Pawn || !Pawn->ApplyDroneDefinition(Definition))
		{
			continue;
		}

		TSet<FString> ActualRoleMeshes;
		int32 PayloadVisualCount = 0;
		int32 RotorVisualCount = 0;
		UStaticMeshComponent* FirstRotorVisual = nullptr;
		TInlineComponentArray<UStaticMeshComponent*> MeshComponents;
		Pawn->GetComponents(MeshComponents);
		for (UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			if (!MeshComponent || MeshComponent->IsA<UCameraProxyMeshComponent>())
			{
				continue;
			}
			TestEqual(TEXT("Role visual has no collision"), MeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Role visual cannot affect Navigation"), MeshComponent->CanEverAffectNavigation());
			if (MeshComponent->ComponentHasTag(TEXT("DroneRoleVisual")) && MeshComponent->GetStaticMesh())
			{
				ActualRoleMeshes.Add(MeshComponent->GetStaticMesh()->GetPathName());
			}
			if (MeshComponent->ComponentHasTag(TEXT("DroneCarriedPayload")))
			{
				++PayloadVisualCount;
				TestTrue(TEXT("Loaded Drop payload is visible before release"), MeshComponent->IsVisible() && !MeshComponent->bHiddenInGame);
			}
			if (MeshComponent->ComponentHasTag(TEXT("DroneRotor")))
			{
				++RotorVisualCount;
				FirstRotorVisual = FirstRotorVisual ? FirstRotorVisual : MeshComponent;
			}
		}
		TestTrue(TEXT("Role Pawn references exactly its selected Drone Pack meshes"),
			ActualRoleMeshes.Num() == Contract.ExpectedRoleMeshes.Num()
				&& ActualRoleMeshes.Includes(Contract.ExpectedRoleMeshes));
		TestEqual(TEXT("Role Pawn tags every expected Rotor visual"), RotorVisualCount, Contract.ExpectedRotorCount);
		TestEqual(TEXT("Runtime Pawn collects every tagged Rotor visual"),
			Pawn->GetRotorVisualComponentCount(), Contract.ExpectedRotorCount);
		TestTrue(TEXT("Rotor visual speed is enabled by a positive BP-adjustable value"),
			Pawn->GetRotorVisualSpinDegreesPerSecond() > 0.0f);
		if (FirstRotorVisual)
		{
			const FQuat InitialRotorRotation = FirstRotorVisual->GetRelativeRotation().Quaternion();
			const FVector InitialRotorVisualCenter = FirstRotorVisual->GetComponentTransform().TransformPosition(
				FirstRotorVisual->GetStaticMesh()->GetBoundingBox().GetCenter());
			Pawn->Tick(0.01f);
			TestFalse(TEXT("Rotor visual changes local rotation during gameplay Tick"),
				FirstRotorVisual->GetRelativeRotation().Quaternion().Equals(InitialRotorRotation, KINDA_SMALL_NUMBER));
			const FVector UpdatedRotorVisualCenter = FirstRotorVisual->GetComponentTransform().TransformPosition(
				FirstRotorVisual->GetStaticMesh()->GetBoundingBox().GetCenter());
			TestTrue(TEXT("Rotor visual spins around its own Mesh center instead of orbiting the Pawn"),
				InitialRotorVisualCenter.Equals(UpdatedRotorVisualCenter, 0.1f));
		}

		if (Definition->DroneId == FName(TEXT("Drone.Drop.Greybox")))
		{
			TestEqual(TEXT("Drop Pawn has exactly one carried payload visual"), PayloadVisualCount, 1);
			TestEqual(TEXT("Drop Pawn spawns the reusable carryable crate class"),
				Pawn->GetPayloadDropComponent()->GetPayloadClass().Get(),
				CarryablePayloadClass);
			Pawn->GetPayloadDropComponent()->ReloadPayloadsForMission();
		}
		else
		{
			TestEqual(TEXT("Scout Pawn has no carried payload visual"), PayloadVisualCount, 0);
		}
		Pawn->Destroy();
	}

	WorldWrapper.TickTestWorld();
	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif
