#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "AI/DroneAutomaticTurret.h"
#include "AI/Weapons/DroneNPCProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneAutomaticTurretTargetingTest,
	"Drone.AI.AutomaticTurretTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneAutomaticTurretTargetingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Automatic turret test World exists"), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADroneEmplacedAutomaticTurret* Emplaced = World->SpawnActor<ADroneEmplacedAutomaticTurret>(
		ADroneEmplacedAutomaticTurret::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>(
		ADronePrototypePawn::StaticClass(),
		FVector(1000.0f, 0.0f, 110.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	TestNotNull(TEXT("Emplaced automatic turret spawns"), Emplaced);
	TestNotNull(TEXT("Prototype Drone target spawns"), Drone);
	if (!Emplaced || !Drone)
	{
		return false;
	}

	Emplaced->ConfigureAutomaticTargetingGreybox(2000.0f, 2500.0f, 0.1f, false);
	Emplaced->ConfigureMGTurretAccuracyGreybox(0.0f);
	TestTrue(TEXT("Immediate scan acquires the nearest Prototype Drone"), Emplaced->RefreshAutomaticTarget() == Drone);
	TestTrue(TEXT("Automatic turret self-operates the shared MG firing pipeline"), Emplaced->GetMGTurretUser() == Emplaced);
	TestTrue(TEXT("Automatic turret forwards its acquired Drone to MG aim"), Emplaced->GetMGTurretTarget() == Drone);
	ADroneNPCProjectile* Projectile = Emplaced->GetLastMGTurretProjectile();
	TestNotNull(TEXT("Aligned automatic turret spawns a projectile"), Projectile);
	if (Projectile)
	{
		TestTrue(TEXT("Automatic turret projectile keeps a distinct source"),
			Projectile->GetProjectileSource() == EDroneNPCProjectileSource::AutomaticTurret);
		TestTrue(TEXT("Automatic turret projectile records the Drone target"), Projectile->GetIntendedTarget() == Drone);
		Projectile->SetActorEnableCollision(false);
		Projectile->Destroy();
	}

	Drone->SetActorLocation(FVector(4000.0f, 0.0f, 110.0f));
	TestNull(TEXT("Target beyond lose range is released"), Emplaced->RefreshAutomaticTarget());
	TestFalse(TEXT("Shared MG use ends when automatic target is lost"), Emplaced->IsMGTurretInUse());

	Drone->SetActorLocation(FVector(1000.0f, 0.0f, 110.0f));
	AActor* SightBlocker = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		FVector(500.0f, 0.0f, 110.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	TestNotNull(TEXT("Visibility blocker spawns"), SightBlocker);
	if (SightBlocker)
	{
		UBoxComponent* BlockerCollision = NewObject<UBoxComponent>(SightBlocker, TEXT("AutomaticTurretSightBlocker"));
		SightBlocker->SetRootComponent(BlockerCollision);
		SightBlocker->AddInstanceComponent(BlockerCollision);
		BlockerCollision->SetBoxExtent(FVector(50.0f, 200.0f, 200.0f));
		BlockerCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BlockerCollision->SetCollisionResponseToAllChannels(ECR_Block);
		BlockerCollision->RegisterComponent();
		SightBlocker->SetActorLocation(FVector(500.0f, 0.0f, 110.0f));

		Emplaced->ConfigureAutomaticTargetingGreybox(2000.0f, 2500.0f, 0.1f, true);
		TestNull(TEXT("Visibility blocker prevents automatic target acquisition"), Emplaced->RefreshAutomaticTarget());
		BlockerCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TestTrue(TEXT("Removing the blocker allows immediate reacquisition"), Emplaced->RefreshAutomaticTarget() == Drone);
	}

	AActor* VehicleCarrier = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		FVector(0.0f, 1000.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	ADroneVehicleAutomaticTurret* VehicleTurret = World->SpawnActor<ADroneVehicleAutomaticTurret>(
		ADroneVehicleAutomaticTurret::StaticClass(),
		FVector(0.0f, 1000.0f, 100.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	TestNotNull(TEXT("Vehicle carrier spawns"), VehicleCarrier);
	TestNotNull(TEXT("Vehicle automatic turret spawns"), VehicleTurret);
	if (VehicleCarrier && VehicleTurret)
	{
		USceneComponent* CarrierRoot = NewObject<USceneComponent>(VehicleCarrier, TEXT("VehicleCarrierRoot"));
		VehicleCarrier->SetRootComponent(CarrierRoot);
		VehicleCarrier->AddInstanceComponent(CarrierRoot);
		CarrierRoot->RegisterComponent();
		VehicleTurret->AttachToActor(VehicleCarrier, FAttachmentTransformRules::KeepWorldTransform);
		const FVector InitialRelativeLocation = VehicleTurret->GetActorLocation() - VehicleCarrier->GetActorLocation();
		VehicleCarrier->SetActorLocation(FVector(500.0f, 1000.0f, 0.0f));
		TestTrue(TEXT("Vehicle turret follows the carrier Actor transform"),
			VehicleTurret->GetActorLocation().Equals(VehicleCarrier->GetActorLocation() + InitialRelativeLocation, 0.1f));
	}

	return !HasAnyErrors();
}

#endif
