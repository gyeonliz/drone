#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Abilities/DroneDroppedPayload.h"
#include "Abilities/DroneImpactDetonationComponent.h"
#include "Abilities/DronePayloadDropComponent.h"
#include "Abilities/DronePayloadTargetComponent.h"
#include "Abilities/DroneReconScanComponent.h"
#include "Abilities/DroneReconScanTargetComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneDefinition.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"

namespace DroneRoleAbilityTest
{
AActor* SpawnTargetActor(UWorld* World, const FVector& Location)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World ? World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters) : nullptr;
	if (!Actor)
	{
		return nullptr;
	}
	USphereComponent* Collision = NewObject<USphereComponent>(Actor, TEXT("TestTargetCollision"));
	Collision->InitSphereRadius(50.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Actor->SetRootComponent(Collision);
	Collision->RegisterComponent();
	Actor->SetActorLocation(Location);
	return Actor;
}

ADronePrototypePawn* SpawnDrone(UWorld* World, UDroneDefinition* Definition, const FVector& Location)
{
	if (!World || !Definition)
	{
		return nullptr;
	}
	UClass* PawnClass = Definition->PawnClass.LoadSynchronous();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePrototypePawn* Pawn = World->SpawnActor<ADronePrototypePawn>(PawnClass, Location, FRotator::ZeroRotator, SpawnParameters);
	return Pawn && Pawn->ApplyDroneDefinition(Definition) ? Pawn : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneRoleAbilitiesTest,
	"Drone.Prototype.RoleAbilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneRoleAbilitiesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Role ability test World exists"), World);
	if (!World)
	{
		return false;
	}

	UDroneDefinition* ScoutDefinition = LoadObject<UDroneDefinition>(nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_Scout_Greybox.DA_Drone_Scout_Greybox"));
	UDroneDefinition* FPVDefinition = LoadObject<UDroneDefinition>(nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_FPVStrike_Greybox.DA_Drone_FPVStrike_Greybox"));
	UDroneDefinition* DropDefinition = LoadObject<UDroneDefinition>(nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_Drop_Greybox.DA_Drone_Drop_Greybox"));
	TestNotNull(TEXT("Scout Definition exists"), ScoutDefinition);
	TestNotNull(TEXT("FPV Definition exists"), FPVDefinition);
	TestNotNull(TEXT("Drop Definition exists"), DropDefinition);
	if (!ScoutDefinition || !FPVDefinition || !DropDefinition)
	{
		return false;
	}

	// DR-RECON-01: 유효 대상만 시작하고, 유지 시간이 끝나면 대상과 Drone 양쪽에 1회 완료를 남긴다.
	ADronePrototypePawn* Scout = DroneRoleAbilityTest::SpawnDrone(World, ScoutDefinition, FVector::ZeroVector);
	AActor* ReconTarget = DroneRoleAbilityTest::SpawnTargetActor(World, FVector(500.0f, 0.0f, 0.0f));
	UDroneReconScanTargetComponent* ReconTargetComponent = ReconTarget
		? NewObject<UDroneReconScanTargetComponent>(ReconTarget, TEXT("ReconTargetComponent"))
		: nullptr;
	if (ReconTargetComponent)
	{
		ReconTargetComponent->RegisterComponent();
	}
	TestNotNull(TEXT("Scout Drone spawns"), Scout);
	TestNotNull(TEXT("Recon target component exists"), ReconTargetComponent);
	if (!Scout || !ReconTargetComponent)
	{
		return false;
	}
	UDroneReconScanComponent* ReconScan = Scout->GetReconScanComponent();
	ReconScan->ConfigureScanGreybox(1000.0f, 0.25f, 30.0f, false);
	TestTrue(TEXT("Scout Recon capability is enabled"), ReconScan->IsFeatureEnabled());
	TestTrue(TEXT("Scan starts on a valid target"), ReconScan->StartScan(ReconTarget));
	ReconScan->TickComponent(0.10f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Scan reports partial progress"), ReconScan->GetScanProgressNormalized() > 0.0f);
	ReconScan->TickComponent(0.20f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Recon target is completed"), ReconTargetComponent->IsScanCompleted());
	TestEqual(TEXT("Scan completes exactly once"), ReconScan->GetCompletedScanCount(), 1);
	TestFalse(TEXT("Completed target cannot be scanned twice"), ReconScan->StartScan(ReconTarget));

	AActor* ReconInputTarget = DroneRoleAbilityTest::SpawnTargetActor(World, FVector(700.0f, 0.0f, 0.0f));
	UDroneReconScanTargetComponent* ReconInputTargetComponent = ReconInputTarget
		? NewObject<UDroneReconScanTargetComponent>(ReconInputTarget, TEXT("ReconInputTargetComponent"))
		: nullptr;
	if (ReconInputTargetComponent)
	{
		ReconInputTargetComponent->RegisterComponent();
	}
	TestNotNull(TEXT("Recon input target exists"), ReconInputTargetComponent);
	TestTrue(TEXT("Scout primary role action selects the nearest valid target"), Scout->TriggerPrimaryRoleAbility());
	TestTrue(TEXT("Scout primary action starts scanning its selected target"), ReconScan->GetActiveScanTarget() == ReconInputTarget);
	TestTrue(TEXT("Scout secondary role action cancels an active scan"), Scout->TriggerSecondaryRoleAbility());
	TestFalse(TEXT("Scout scan is idle after secondary action"), ReconScan->IsScanning());

	// 역할 분리: Scout에서 FPV/Drop 기능은 켤 수 없다.
	TestFalse(TEXT("Scout cannot arm FPV detonation"), Scout->GetImpactDetonationComponent()->ArmImpactDetonation());
	TestNull(TEXT("Scout cannot drop a payload"), Scout->GetPayloadDropComponent()->DropPayload());

	// DR-FPV-01: 명시적 Arm과 최소 속도를 모두 만족한 첫 충돌만 폭발한다.
	ADronePrototypePawn* FPV = DroneRoleAbilityTest::SpawnDrone(World, FPVDefinition, FVector(2000.0f, 0.0f, 300.0f));
	AActor* ImpactTarget = DroneRoleAbilityTest::SpawnTargetActor(World, FVector(2200.0f, 0.0f, 300.0f));
	TestNotNull(TEXT("FPV Drone spawns"), FPV);
	if (!FPV || !ImpactTarget)
	{
		return false;
	}
	UDroneImpactDetonationComponent* Impact = FPV->GetImpactDetonationComponent();
	Impact->ConfigureImpactGreybox(500.0f, 100.0f, 350.0f);
	TestTrue(TEXT("FPV Impact capability is enabled"), Impact->IsFeatureEnabled());
	TestFalse(TEXT("Unarmed impact does not detonate"), Impact->TryDetonateFromImpact(ImpactTarget, 1000.0f));
	TestTrue(TEXT("FPV primary role action arms impact detonation"), FPV->TriggerPrimaryRoleAbility());
	TestTrue(TEXT("FPV is armed after its primary action"), Impact->IsArmed());
	TestTrue(TEXT("FPV secondary role action disarms impact detonation"), FPV->TriggerSecondaryRoleAbility());
	TestFalse(TEXT("FPV is disarmed after its secondary action"), Impact->IsArmed());
	TestTrue(TEXT("FPV primary role action can arm again"), FPV->TriggerPrimaryRoleAbility());
	TestFalse(TEXT("Impact below minimum speed does not detonate"), Impact->TryDetonateFromImpact(ImpactTarget, 499.0f));
	TestTrue(TEXT("Armed valid-speed impact detonates"), Impact->TryDetonateFromImpact(ImpactTarget, 500.0f));
	TestTrue(TEXT("FPV self-destroys through shared Health"), FPV->GetHealthComponent()->IsDead());
	TestEqual(TEXT("Impact detonates exactly once"), Impact->GetDetonationCount(), 1);
	TestFalse(TEXT("Repeated impact cannot detonate again"), Impact->TryDetonateFromImpact(ImpactTarget, 1000.0f));

	// DR-DROP-01: 탑뷰, 한 발 적재, 목표 Actor 직접 접촉 성공과 Mission 재장전을 검증한다.
	ADronePrototypePawn* DropDrone = DroneRoleAbilityTest::SpawnDrone(World, DropDefinition, FVector(4000.0f, 0.0f, 1000.0f));
	AActor* DropTarget = DroneRoleAbilityTest::SpawnTargetActor(World, FVector(4000.0f, 0.0f, 0.0f));
	UDronePayloadTargetComponent* DropTargetComponent = DropTarget
		? NewObject<UDronePayloadTargetComponent>(DropTarget, TEXT("DropTargetComponent"))
		: nullptr;
	if (DropTargetComponent)
	{
		DropTargetComponent->RegisterComponent();
	}
	TestNotNull(TEXT("Drop Drone spawns"), DropDrone);
	TestNotNull(TEXT("Drop target component exists"), DropTargetComponent);
	if (!DropDrone || !DropTargetComponent)
	{
		return false;
	}
	UDronePayloadDropComponent* Drop = DropDrone->GetPayloadDropComponent();
	UPrimitiveComponent* CarriedPayloadVisual = nullptr;
	TInlineComponentArray<UPrimitiveComponent*> DropPrimitives;
	DropDrone->GetComponents(DropPrimitives);
	for (UPrimitiveComponent* Primitive : DropPrimitives)
	{
		if (Primitive && Primitive->ComponentHasTag(TEXT("DroneCarriedPayload")))
		{
			CarriedPayloadVisual = Primitive;
			break;
		}
	}
	TestTrue(TEXT("Drop capability is enabled"), Drop->IsFeatureEnabled());
	TestEqual(TEXT("Drop Drone starts with one payload"), Drop->GetRemainingPayloadCount(), 1);
	TestNotNull(TEXT("Drop Drone visibly carries one payload"), CarriedPayloadVisual);
	TestTrue(TEXT("Carried payload is visible before release"),
		CarriedPayloadVisual && CarriedPayloadVisual->IsVisible() && !CarriedPayloadVisual->bHiddenInGame);
	TestEqual(TEXT("Drop auto-selects the nearest incomplete payload target"), Drop->FindBestAvailablePayloadTarget(), DropTarget);
	TestTrue(TEXT("Drop secondary role action enters top-down view"), DropDrone->TriggerSecondaryRoleAbility());
	TestTrue(TEXT("Drop view enters top-down camera"), DropDrone->IsDropCameraViewEnabled());
	TestTrue(TEXT("Drop view uses a non-zero camera arm"), DropDrone->GetCameraBoom()->TargetArmLength > 0.0f);
	TestTrue(TEXT("Drop primary role action releases one payload"), DropDrone->TriggerPrimaryRoleAbility());
	ADroneDroppedPayload* Payload = nullptr;
	for (TActorIterator<ADroneDroppedPayload> It(World); It; ++It)
	{
		if (It->GetOwner() == DropDrone)
		{
			Payload = *It;
			break;
		}
	}
	TestNotNull(TEXT("One payload spawns"), Payload);
	TestEqual(TEXT("Payload inventory decreases"), Drop->GetRemainingPayloadCount(), 0);
	TestTrue(TEXT("Carried payload visual disappears after release"),
		CarriedPayloadVisual && (!CarriedPayloadVisual->IsVisible() || CarriedPayloadVisual->bHiddenInGame));
	TestEqual(TEXT("Drop stores the automatically selected target"), Drop->GetDropTarget(), DropTarget);
	TestNull(TEXT("Second payload cannot spawn before reload"), Drop->DropPayload());
	if (Payload)
	{
		TestTrue(TEXT("Payload resolves first impact"), Payload->ResolveImpactGreybox(DropTarget));
		TestTrue(TEXT("Payload recognizes intended target"), Payload->DidHitIntendedTarget());
	}
	TestTrue(TEXT("Drop target records delivery"), DropTargetComponent->IsPayloadDelivered());
	TestEqual(TEXT("Drop component records one successful delivery"), Drop->GetSuccessfulDeliveryCount(), 1);

	ADroneDroppedPayload* CarryablePayload = World->SpawnActor<ADroneDroppedPayload>(
		ADroneDroppedPayload::StaticClass(),
		FVector(4100.0f, 0.0f, 1000.0f),
		FRotator::ZeroRotator);
	TestNotNull(TEXT("Map-placeable carryable payload spawns"), CarryablePayload);
	if (CarryablePayload)
	{
		CarryablePayload->ActivateCarryablePickup();
	}
	TestEqual(TEXT("Empty Drop Drone finds the nearest carryable payload"), Drop->FindBestAvailableCarryablePayload(), CarryablePayload);
	TestTrue(TEXT("Primary action picks up a nearby payload when inventory is empty"), DropDrone->TriggerPrimaryRoleAbility());
	TestEqual(TEXT("Picked-up payload becomes the carried Actor"), Drop->GetCarriedPayloadActor(), CarryablePayload);
	TestTrue(TEXT("Picked-up payload is attached and carried"), CarryablePayload && CarryablePayload->IsCarried() && CarryablePayload->GetAttachParentActor() == DropDrone);
	TestEqual(TEXT("Pickup restores one payload in inventory"), Drop->GetRemainingPayloadCount(), 1);
	TestTrue(TEXT("Primary action drops the same carried Actor"), DropDrone->TriggerPrimaryRoleAbility());
	TestNull(TEXT("Carried Actor reference clears after drop"), Drop->GetCarriedPayloadActor());
	TestTrue(TEXT("Dropped map payload is no longer carried"), CarryablePayload && !CarryablePayload->IsCarried());
	TestEqual(TEXT("Dropping the carried map object empties inventory"), Drop->GetRemainingPayloadCount(), 0);
	TestTrue(TEXT("Dropped carryable payload resolves its landing"), CarryablePayload && CarryablePayload->ResolveImpactGreybox(DropTarget));
	TestTrue(TEXT("Dropped carryable payload remains available in the world"), CarryablePayload && CarryablePayload->IsAvailableForPickup() && !CarryablePayload->IsActorBeingDestroyed());
	TestEqual(TEXT("Dropped carryable payload has no automatic lifespan"), CarryablePayload ? CarryablePayload->GetLifeSpan() : -1.0f, 0.0f);

	TestTrue(TEXT("Drop secondary role action restores the previous camera"), DropDrone->TriggerSecondaryRoleAbility());
	TestFalse(TEXT("Leaving Drop view restores normal camera state"), DropDrone->IsDropCameraViewEnabled());
	Drop->ReloadPayloadsForMission();
	TestEqual(TEXT("Mission reload restores one payload"), Drop->GetRemainingPayloadCount(), 1);
	TestTrue(TEXT("Mission reload restores the carried payload visual"),
		CarriedPayloadVisual && CarriedPayloadVisual->IsVisible() && !CarriedPayloadVisual->bHiddenInGame);

	return !HasAnyErrors();
}

#endif
