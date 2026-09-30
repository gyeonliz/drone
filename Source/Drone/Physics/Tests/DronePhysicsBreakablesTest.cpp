#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Physics/DroneBreakableWallPanel.h"
#include "Physics/DroneCollisionResponseComponent.h"
#include "Physics/DroneNetPlacementRig.h"
#include "Prototype/DronePrototypePawn.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/FloatingPawnMovement.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePhysicsBreakablesTest,
	"Drone.Physics.Breakables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDronePhysicsBreakablesTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("Physics breakables test World exists"), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADroneNetPlacementRig* Net = World->SpawnActor<ADroneNetPlacementRig>(
		ADroneNetPlacementRig::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("Net rig spawns"), Net);
	if (Net)
	{
		TestTrue(TEXT("Net defaults to Drone entanglement on contact"), Net->EntanglesDronesOnImpact());
		TestFalse(TEXT("Net no longer tears apart from ordinary Drone contact by default"), Net->BreaksOnDroneImpact());
		const float NetWidth = FVector::Distance(Net->GetTopLeftCorner(), Net->GetTopRightCorner());
		const float NetHeight = FVector::Distance(Net->GetTopLeftCorner(), Net->GetBottomLeftCorner());
		TestTrue(TEXT("Default net width stays within a readable Drone-scale six metres"), NetWidth <= 650.0f);
		TestTrue(TEXT("Default net height stays within a readable Drone-scale four metres"), NetHeight <= 400.0f);
		const int32 InitialCount = Net->GetIntactStrandSegmentCount();
		TestTrue(TEXT("Net starts with intact strand segments"), InitialCount > 12);
		const FVector CenterImpact = Net->GetActorTransform().TransformPosition(FVector(70.0f, 0.0f, 180.0f));
		const int32 BrokenNow = Net->BreakNetAtWorldLocation(CenterImpact, 90.0f);
		TestTrue(TEXT("Local impact breaks at least one strand segment"), BrokenNow > 0);
		TestTrue(TEXT("Local impact leaves some strand segments intact"), Net->GetIntactStrandSegmentCount() > 0);
		TestTrue(TEXT("Local impact does not hide the whole net"), Net->GetIntactStrandSegmentCount() < InitialCount);
		TestEqual(TEXT("Broken strand count is tracked"), Net->GetBrokenStrandSegmentCount(), BrokenNow);
		TestTrue(TEXT("Broken net strands become visible physics debris"),
			Net->GetLiveDetachedPhysicsSegmentCount() > 0);
		Net->ResetNetGreybox();
		TestEqual(TEXT("Net reset restores all strand segments"), Net->GetIntactStrandSegmentCount(), InitialCount);
		TestEqual(TEXT("Net reset clears broken strand state"), Net->GetBrokenStrandSegmentCount(), 0);
		TestEqual(TEXT("Net reset removes detached physics debris"),
			Net->GetLiveDetachedPhysicsSegmentCount(), 0);
	}

	ADroneBreakableWallPanel* Wall = World->SpawnActor<ADroneBreakableWallPanel>(
		ADroneBreakableWallPanel::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("Breakable wall panel spawns"), Wall);
	if (Wall)
	{
		const FVector WallSize = Wall->GetConfiguredWallSizeCentimeters();
		TestTrue(TEXT("Default wall width stays within a readable Drone-scale six metres"), WallSize.Y <= 650.0f);
		TestTrue(TEXT("Default wall height stays within a readable Drone-scale four metres"), WallSize.Z <= 450.0f);
		const int32 InitialCount = Wall->GetIntactPieceCount();
		TestTrue(TEXT("Wall starts as a multi-piece grid"), InitialCount >= 12);
		const int32 BrokenNow = Wall->BreakWallAtWorldLocation(
			Wall->GetActorLocation(), 250.0f, FVector(1.0f, 0.0f, 0.25f));
		TestTrue(TEXT("Local wall impact breaks at least one piece"), BrokenNow > 0);
		TestTrue(TEXT("Local wall impact leaves other pieces intact"), Wall->GetIntactPieceCount() > 0);
		TestEqual(TEXT("Wall broken-piece state is tracked"), Wall->GetBrokenPieceCount(), BrokenNow);
		Wall->ResetWall();
		TestEqual(TEXT("Wall reset restores every piece"), Wall->GetIntactPieceCount(), InitialCount);
		TestEqual(TEXT("Wall reset clears broken-piece state"), Wall->GetBrokenPieceCount(), 0);

		ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>(
			ADronePrototypePawn::StaticClass(), FTransform(FVector(-200.0f, 0.0f, 90.0f)), SpawnParameters);
		TestNotNull(TEXT("Collision test Drone spawns"), Drone);
		if (Drone)
		{
			if (!World->HasBegunPlay())
			{
				World->BeginPlay();
			}
			if (!Drone->HasActorBegunPlay())
			{
				Drone->DispatchBeginPlay();
			}
			UDroneCollisionResponseComponent* Response = Drone->GetCollisionResponseComponent();
			UFloatingPawnMovement* Movement = Cast<UFloatingPawnMovement>(Drone->GetMovementComponent());
			TestNotNull(TEXT("Collision test Drone owns response component"), Response);
			TestNotNull(TEXT("Collision test Drone owns floating movement"), Movement);
			if (Response && Movement)
			{
				TestTrue(TEXT("Common flight Drone wall response is enabled by default"), Response->IsCollisionResponseEnabled());
				Response->ConfigureCollisionResponse(true, 0.65f, 1.0f, 140.0f, 1200.0f);
				Movement->Velocity = FVector(900.0f, 0.0f, 0.0f);
				FHitResult Impact;
				Impact.ImpactPoint = Wall->GetActorTransform().TransformPosition(FVector(0.0f, 0.0f, 90.0f));
				Impact.ImpactNormal = FVector(-1.0f, 0.0f, 0.0f);
				Impact.Normal = Impact.ImpactNormal;
				Drone->OnActorHit.Broadcast(Drone, Wall, FVector::ZeroVector, Impact);
				TestTrue(TEXT("Real Drone hit event reaches collision response"),
					Response->GetResolvedImpactCount() > 0);
				TestTrue(TEXT("Real Drone hit event forwards local damage into the breakable wall"),
					Wall->GetBrokenPieceCount() > 0);

				Movement->Velocity = FVector(620.0f, 80.0f, 120.0f);
				UInstancedStaticMeshComponent* NetStrands = Net
					? Net->FindComponentByClass<UInstancedStaticMeshComponent>()
					: nullptr;
				TestNotNull(TEXT("Net exposes its collision strand component"), NetStrands);
				TestTrue(TEXT("Net collision strand delegates runtime impacts to the net actor"),
					NetStrands && NetStrands->OnComponentHit.IsBound());
				const int32 NetSegmentsBeforeDroneContact = Net ? Net->GetIntactStrandSegmentCount() : 0;
				const bool bEntanglementApplied = Net && Net->ApplyDroneImpact(
					Drone,
					Drone->GetActorLocation() + FVector(40.0f, 0.0f, 0.0f),
					FVector(-1.0f, 0.0f, 0.0f),
					Movement->Velocity);
				TestTrue(TEXT("Net contact applies entanglement to a flight Drone"), bEntanglementApplied);
				TestTrue(TEXT("Net contact exposes an active entanglement state"), Response->IsNetEntangled());
				TestEqual(TEXT("Ordinary Drone contact entangles without tearing the net"),
					Net ? Net->GetIntactStrandSegmentCount() : 0,
					NetSegmentsBeforeDroneContact);
				const float VerticalSpeedBeforeTick = Movement->Velocity.Z;
				Response->TickComponent(0.1f, LEVELTICK_All, nullptr);
				TestTrue(TEXT("Net entanglement smoothly reduces flight control effectiveness"),
					Response->GetFlightControlEffectivenessMultiplier() < 1.0f);
				TestTrue(TEXT("Net entanglement pulls the Drone downward"),
					Movement->Velocity.Z < VerticalSpeedBeforeTick);
				Response->ClearNetEntanglement();
				TestFalse(TEXT("Clearing the net state restores an unentangled Drone"), Response->IsNetEntangled());
				TestEqual(TEXT("Clearing the net state restores full control"),
					Response->GetFlightControlEffectivenessMultiplier(), 1.0f);
			}
		}
	}

	// The visual rotor span is wider than the single movement sphere. Verify that a wall
	// outside the root sphere is still found by the default wing/rotor probes and that the
	// real runtime response scales from a gentle contact to a hard impact.
	AActor* ProbeWall = World->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("Rotor probe test wall actor spawns"), ProbeWall);
	if (ProbeWall)
	{
		UBoxComponent* ProbeWallCollision = NewObject<UBoxComponent>(ProbeWall, TEXT("ProbeWallCollision"));
		ProbeWall->AddInstanceComponent(ProbeWallCollision);
		ProbeWall->SetRootComponent(ProbeWallCollision);
		ProbeWallCollision->InitBoxExtent(FVector(450.0f, 10.0f, 180.0f));
		ProbeWallCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ProbeWallCollision->SetCollisionObjectType(ECC_WorldStatic);
		ProbeWallCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
		ProbeWallCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		ProbeWallCollision->RegisterComponent();
		ProbeWall->SetActorLocation(FVector(0.0f, 105.0f, 90.0f));
		ProbeWallCollision->UpdateBounds();

		auto SpawnProbeDrone = [&](const float X, const float InwardSpeed)
		{
			ADronePrototypePawn* ProbeDrone = World->SpawnActor<ADronePrototypePawn>(
				ADronePrototypePawn::StaticClass(),
				FTransform(FVector(X, 0.0f, 90.0f)),
				SpawnParameters);
			if (ProbeDrone && !ProbeDrone->HasActorBegunPlay())
			{
				ProbeDrone->DispatchBeginPlay();
			}
			if (ProbeDrone && ProbeDrone->GetPrototypeMovementComponent())
			{
				ProbeDrone->GetPrototypeMovementComponent()->Velocity = FVector(0.0f, InwardSpeed, 0.0f);
			}
			return ProbeDrone;
		};

		ADronePrototypePawn* GentleProbeDrone = SpawnProbeDrone(-220.0f, 20.0f);
		ADronePrototypePawn* HardProbeDrone = SpawnProbeDrone(220.0f, 600.0f);
		TestNotNull(TEXT("Gentle rotor probe Drone spawns"), GentleProbeDrone);
		TestNotNull(TEXT("Hard rotor probe Drone spawns"), HardProbeDrone);
		if (GentleProbeDrone && HardProbeDrone)
		{
			UDroneCollisionResponseComponent* GentleResponse = GentleProbeDrone->GetCollisionResponseComponent();
			UDroneCollisionResponseComponent* HardResponse = HardProbeDrone->GetCollisionResponseComponent();
			UFloatingPawnMovement* GentleMovement = GentleProbeDrone->GetPrototypeMovementComponent();
			UFloatingPawnMovement* HardMovement = HardProbeDrone->GetPrototypeMovementComponent();
			TestNotNull(TEXT("Gentle rotor probe response exists"), GentleResponse);
			TestNotNull(TEXT("Hard rotor probe response exists"), HardResponse);
			if (GentleResponse && HardResponse && GentleMovement && HardMovement)
			{
				const FRotator GentleRotationBefore = GentleProbeDrone->GetActorRotation();
				GentleResponse->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
				HardResponse->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
				GentleResponse->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
				TestTrue(TEXT("Wing probe resolves a wall before the root sphere reaches it"),
					GentleResponse->GetResolvedImpactCount() > 0);
				TestTrue(TEXT("Gentle wing contact pushes the Drone away from the wall"),
					GentleMovement->Velocity.Y < 0.0f);
				TestTrue(TEXT("Hard wing contact pushes away faster than gentle contact"),
					FMath::Abs(HardMovement->Velocity.Y) > FMath::Abs(GentleMovement->Velocity.Y));
				TestTrue(TEXT("Wing contact does not fight the flight controller Root attitude"),
					GentleProbeDrone->GetActorRotation().Equals(GentleRotationBefore, KINDA_SMALL_NUMBER));
				TestFalse(TEXT("Wing contact supplies a smoothly interpolated visual lean"),
					GentleResponse->GetContactVisualRotation().IsNearlyZero());
			}
		}
	}

	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif
