#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Physics/DroneCollisionResponseComponent.h"
#include "Physics/DroneNetPlacementRig.h"
#include "Prototype/DronePrototypePawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneContactSmoothingTest, "Drone.Physics.ContactSmoothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneContactSmoothingTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>(
		ADronePrototypePawn::StaticClass(), FTransform(FVector(0, 0, 500)), Spawn);
	AActor* Wall = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Spawn);
	ADroneNetPlacementRig* Net = World->SpawnActor<ADroneNetPlacementRig>(
		ADroneNetPlacementRig::StaticClass(), FTransform(FVector(10000, 0, 0)), Spawn);
	if (!TestNotNull(TEXT("Drone exists"), Drone) || !TestNotNull(TEXT("Wall exists"), Wall)
		|| !TestNotNull(TEXT("Net exists"), Net))
	{
		return false;
	}
	World->BeginPlay();
	if (!Drone->HasActorBegunPlay()) Drone->DispatchBeginPlay();
	UDroneCollisionResponseComponent* Response = Drone->GetCollisionResponseComponent();
	UFloatingPawnMovement* Movement = Drone->GetPrototypeMovementComponent();
	if (!TestNotNull(TEXT("Response exists"), Response) || !TestNotNull(TEXT("Movement exists"), Movement)) return false;

	Movement->Velocity = FVector(600, 0, 0);
	FHitResult Hit;
	Hit.ImpactPoint = Drone->GetActorLocation() + FVector(95, 60, 0);
	Hit.ImpactNormal = FVector(-1, 0, 0);
	Hit.Normal = Hit.ImpactNormal;
	const FTransform BeforeHit = Drone->GetActorTransform();
	Drone->OnActorHit.Broadcast(Drone, Wall, FVector::ZeroVector, Hit);
	TestTrue(TEXT("Wall contact still produces an outward push"), Movement->Velocity.X < 0);
	TestTrue(TEXT("Wall contact does not teleport the Drone position"),
		Drone->GetActorLocation().Equals(BeforeHit.GetLocation(), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Wall contact does not snap the Drone attitude"),
		Drone->GetActorQuat().Equals(BeforeHit.GetRotation(), KINDA_SMALL_NUMBER));
	const int32 FirstImpactCount = Response->GetResolvedImpactCount();
	Drone->SetActorTickEnabled(false);
	Movement->SetComponentTickEnabled(false);
	for (int32 Index = 0; Index < 12; ++Index)
	{
		WorldWrapper.TickTestWorld(0.06f);
		Movement->Velocity = FVector(20, 70, 15);
		Drone->OnActorHit.Broadcast(Drone, Wall, FVector::ZeroVector, Hit);
		TestEqual(TEXT("Sustained contact is not replayed as a fresh impact"),
			Response->GetResolvedImpactCount(), FirstImpactCount);
		TestTrue(TEXT("Sustained contact still prevents inward pressure"), Movement->Velocity.X <= 0);
		TestTrue(TEXT("Sustained contact preserves tangential movement"), FMath::IsNearlyEqual(Movement->Velocity.Y, 70.0f));
	}
	WorldWrapper.TickTestWorld(0.3f);
	Movement->Velocity = FVector(600, 0, 0);
	Drone->OnActorHit.Broadcast(Drone, Wall, FVector::ZeroVector, Hit);
	TestEqual(TEXT("A separate impact after leaving contact can respond again"),
		Response->GetResolvedImpactCount(), FirstImpactCount + 1);

	Movement->Velocity = FVector(620, 80, 120);
	const FVector BeforeNetContact = Movement->Velocity;
	const int32 DamageShakesBefore = Drone->GetDamageShakeEventCount();
	TestTrue(TEXT("Net contact applies entanglement"), Net->ApplyDroneImpact(Drone,
		Drone->GetActorLocation() + FVector(40, 0, 0), FVector(-1, 0, 0), BeforeNetContact));
	TestEqual(TEXT("Net contact must not play damage-shake feedback"),
		Drone->GetDamageShakeEventCount(), DamageShakesBefore);
	TestTrue(TEXT("Net drag must be integrated over time, not an instant velocity cut"),
		Movement->Velocity.Equals(BeforeNetContact, KINDA_SMALL_NUMBER));
	const float SpeedBeforeTick = Movement->Velocity.Size2D();
	Response->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Net drag remains active"), Movement->Velocity.Size2D() < SpeedBeforeTick);
	TestTrue(TEXT("First net frame does not abruptly remove a large part of the speed"),
		Movement->Velocity.Size2D() > SpeedBeforeTick * 0.9f);
	TestTrue(TEXT("Entanglement still impairs control"), Response->GetFlightControlEffectivenessMultiplier() < 1);

	UInstancedStaticMeshComponent* Strands = Net->FindComponentByClass<UInstancedStaticMeshComponent>();
	TestTrue(TEXT("Net strands must not repeatedly retract the camera arm"),
		Strands && Strands->GetCollisionResponseToChannel(ECC_Camera) == ECR_Ignore);
	TestTrue(TEXT("Net strands still block the Drone"),
		Strands && Strands->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
	Drone->Tick(1.0f / 60.0f);
	Drone->SetFirstPersonViewEnabled(true);
	FTransform CameraOffsetBefore;
	float CameraFOVBefore;
	Drone->GetFollowCamera()->GetAdditiveOffset(CameraOffsetBefore, CameraFOVBefore);
	const FQuat CameraBeforeContact = Drone->GetFollowCamera()->GetComponentQuat();
	Response->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Drone->Tick(1.0f / 60.0f);
	FTransform CameraOffsetAfter;
	float CameraFOVAfter;
	Drone->GetFollowCamera()->GetAdditiveOffset(CameraOffsetAfter, CameraFOVAfter);
	TestFalse(TEXT("Drone contact body feedback remains visible"), Drone->GetVisualTiltPivot()->GetRelativeRotation().IsNearlyZero());
	TestTrue(TEXT("First-person camera does not inherit wall/net body feedback rotation"),
		Drone->GetFollowCamera()->GetComponentQuat().Equals(CameraBeforeContact, 0.001f));
	TestTrue(TEXT("Wall/net contact does not add camera shake"),
		CameraOffsetAfter.Equals(CameraOffsetBefore, 0.001f));
	TestEqual(TEXT("Wall/net contact does not change camera FOV"), CameraFOVAfter, CameraFOVBefore);
	Drone->TriggerDamageShakeGreybox(10.0f);
	Drone->Tick(0.02f);
	Drone->GetFollowCamera()->GetAdditiveOffset(CameraOffsetAfter, CameraFOVAfter);
	TestFalse(TEXT("Real damage still shakes the first-person camera"),
		CameraOffsetAfter.Equals(CameraOffsetBefore, 0.001f));
	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneContactSmoothingBlueprintTest, "Drone.Physics.ContactSmoothingBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneContactSmoothingBlueprintTest::RunTest(const FString& Parameters)
{
	const TCHAR* Paths[] = {
		TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypePawn.BP_DronePrototypePawn"),
		TEXT("/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration.BP_DroneFPVIntegration"),
		TEXT("/Game/Drone/Integrations/RoleDrones/BP_DroneFiberOpticIntegration.BP_DroneFiberOpticIntegration"),
		TEXT("/Game/Drone/Integrations/RoleDrones/BP_DroneGroundUGVIntegration.BP_DroneGroundUGVIntegration"),
		TEXT("/Game/Drone/Physics/Blueprints/BP_DronePhysicsCollisionTest.BP_DronePhysicsCollisionTest"),
		TEXT("/Game/Drone/Physics/Blueprints/BP_DroneNetPlacementRig.BP_DroneNetPlacementRig") };
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game)) return false;
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	int32 Index = 0;
	for (const TCHAR* Path : Paths)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
		if (!TestNotNull(Path, Blueprint)) continue;
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
		TestEqual(*FString::Printf(TEXT("%s compile errors"), Path), Results.NumErrors, 0);
		TestEqual(*FString::Printf(TEXT("%s compile warnings"), Path), Results.NumWarnings, 0);
		if (!TestNotNull(TEXT("Generated class exists"), Blueprint->GeneratedClass.Get())) continue;
		AActor* Actor = WorldWrapper.GetTestWorld()->SpawnActor<AActor>(Blueprint->GeneratedClass,
			FTransform(FVector(++Index * 2000.0f, 0, 500)), Spawn);
		if (ADronePrototypePawn* Pawn = Cast<ADronePrototypePawn>(Actor))
		{
			TestNotNull(TEXT("Saved Pawn Blueprint inherits the stable flight-camera pivot"), Pawn->GetCameraFlightPivot());
			Pawn->SetFirstPersonViewEnabled(true);
			TestTrue(TEXT("Saved Pawn FPV camera isolates contact feedback"),
				Pawn->GetCameraBoom()->GetAttachParent() == Pawn->GetCameraFlightPivot());
			TestTrue(TEXT("Saved Pawn camera smooths translation"), Pawn->GetCameraBoom()->bEnableCameraLag);
			FTransform Before;
			float FOV;
			Pawn->GetFollowCamera()->GetAdditiveOffset(Before, FOV);
			Pawn->TriggerDamageShakeGreybox(10.0f);
			Pawn->Tick(0.02f);
			FTransform After;
			Pawn->GetFollowCamera()->GetAdditiveOffset(After, FOV);
			TestFalse(TEXT("Saved Pawn still shows real damage camera feedback"), After.Equals(Before, 0.001f));
		}
	}
	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors(); // Never save compiled Blueprint packages or any production map.
}

#endif
