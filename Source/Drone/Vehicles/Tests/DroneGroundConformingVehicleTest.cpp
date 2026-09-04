#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "Vehicles/DroneGroundConformingVehicle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneGroundConformingVehicleTest,
	"Drone.Vehicle.GroundConformingSuspension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneGroundConformingVehicleTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Ground-conforming vehicle test World exists"), World);
	if (!World)
	{
		return false;
	}

	struct FPlatformSpec
	{
		FVector Location;
		float SurfaceZ;
	};
	const FPlatformSpec Platforms[] = {
		{FVector(110.0f, -78.0f, 0.0f), 42.0f},
		{FVector(110.0f, 78.0f, 0.0f), 22.0f},
		{FVector(-110.0f, -78.0f, 0.0f), 2.0f},
		{FVector(-110.0f, 78.0f, 0.0f), -18.0f},
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Platforms); ++Index)
	{
		AActor* Platform = World->SpawnActor<AActor>();
		TestNotNull(*FString::Printf(TEXT("Contact platform %d spawns"), Index), Platform);
		if (!Platform)
		{
			continue;
		}
		UBoxComponent* Collision = NewObject<UBoxComponent>(Platform);
		Collision->InitBoxExtent(FVector(70.0f, 70.0f, 10.0f));
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->SetCollisionResponseToAllChannels(ECR_Block);
		Platform->SetRootComponent(Collision);
		Collision->RegisterComponent();
		Platform->SetActorLocation(FVector(
			Platforms[Index].Location.X,
			Platforms[Index].Location.Y,
			Platforms[Index].SurfaceZ - 10.0f));
	}

	ADroneGroundConformingVehicle* Vehicle = World->SpawnActor<ADroneGroundConformingVehicle>(
		FVector(0.0f, 0.0f, 180.0f),
		FRotator::ZeroRotator);
	TestNotNull(TEXT("Ground-conforming vehicle spawns"), Vehicle);
	if (!Vehicle)
	{
		return false;
	}

	Vehicle->SetGreyboxAutoDriveEnabled(false);
	TestTrue(TEXT("Four contact traces conform to the four platforms"), Vehicle->RefreshGroundConformNow(true));
	TestEqual(TEXT("All four suspension points found ground"), Vehicle->GetLastGroundContactCount(), 4);
	TestTrue(TEXT("Vehicle follows the front/rear terrain pitch"), FMath::Abs(Vehicle->GetActorRotation().Pitch) > 1.0f);
	TestTrue(TEXT("Vehicle follows the left/right terrain roll"), FMath::Abs(Vehicle->GetActorRotation().Roll) > 1.0f);
	TestTrue(
		TEXT("Ground angle stays within the configured clamp"),
		FMath::Abs(Vehicle->GetActorRotation().Pitch) <= Vehicle->GetMaximumGroundAngleDegrees() + 0.1f
			&& FMath::Abs(Vehicle->GetActorRotation().Roll) <= Vehicle->GetMaximumGroundAngleDegrees() + 0.1f);
	TestFalse(TEXT("Greybox vehicle does not simulate full rigid-body physics"), Vehicle->GetVehicleCollision()->IsSimulatingPhysics());
	TestEqual(TEXT("Greybox vehicle exposes four separate wheel visuals"), Vehicle->GetWheelMeshes().Num(), 4);
	TestEqual(TEXT("Greybox Cylinder uses the manually verified forward spin direction"), Vehicle->GetWheelVisualSpinDirectionMultiplier(), 1.0f);
	for (const UStaticMeshComponent* Wheel : Vehicle->GetWheelMeshes())
	{
		TestNotNull(TEXT("Wheel visual exists"), Wheel);
		TestTrue(TEXT("Wheel visual follows the vehicle body root"), Wheel && Wheel->GetAttachParent() == Vehicle->GetVehicleCollision());
	}
	TestNotNull(TEXT("Vehicle exposes a turret mount"), Vehicle->GetTurretMount());

	TArray<FRotator> InitialWheelRotations;
	for (const UStaticMeshComponent* Wheel : Vehicle->GetWheelMeshes())
	{
		InitialWheelRotations.Add(Wheel ? Wheel->GetRelativeRotation() : FRotator::ZeroRotator);
	}
	const FVector StartLocation = Vehicle->GetActorLocation();
	Vehicle->SetDriveInput(1.0f, 0.0f);
	Vehicle->Tick(0.05f);
	TestTrue(TEXT("Drive input advances the lightweight vehicle"), Vehicle->GetActorLocation().X > StartLocation.X);
	const float FullSpeed = Vehicle->GetCurrentForwardSpeedCentimetersPerSecond();
	const float FullSpeedWheelDelta = Vehicle->GetCurrentWheelRotationDegrees();
	TestTrue(TEXT("Forward travel reports positive vehicle speed"), FullSpeed > 0.0f);
	TestTrue(TEXT("Forward travel advances wheel rotation"), FullSpeedWheelDelta > 0.0f);
	const TArray<UStaticMeshComponent*> WheelsAfterForwardTravel = Vehicle->GetWheelMeshes();
	for (int32 Index = 0; Index < WheelsAfterForwardTravel.Num(); ++Index)
	{
		TestFalse(
			TEXT("Each wheel visual rotates after forward travel"),
			WheelsAfterForwardTravel[Index]->GetRelativeRotation().Equals(InitialWheelRotations[Index], 0.01f));
	}

	Vehicle->SetDriveInput(0.5f, 0.0f);
	Vehicle->Tick(0.05f);
	const float HalfSpeed = Vehicle->GetCurrentForwardSpeedCentimetersPerSecond();
	const float HalfSpeedWheelDelta = Vehicle->GetCurrentWheelRotationDegrees() - FullSpeedWheelDelta;
	TestTrue(TEXT("Half throttle produces about half the forward speed"),
		HalfSpeed > FullSpeed * 0.4f && HalfSpeed < FullSpeed * 0.6f);
	TestTrue(TEXT("Wheel rotation rate is proportional to movement speed"),
		HalfSpeedWheelDelta > FullSpeedWheelDelta * 0.4f && HalfSpeedWheelDelta < FullSpeedWheelDelta * 0.6f);

	const float RotationBeforeReverse = Vehicle->GetCurrentWheelRotationDegrees();
	Vehicle->SetDriveInput(-1.0f, 0.0f);
	Vehicle->Tick(0.05f);
	TestTrue(TEXT("Reverse travel reports negative vehicle speed"), Vehicle->GetCurrentForwardSpeedCentimetersPerSecond() < 0.0f);
	TestTrue(TEXT("Reverse travel rotates wheels in the opposite direction"),
		Vehicle->GetCurrentWheelRotationDegrees() < RotationBeforeReverse);
	return !HasAnyErrors();
}

#endif
