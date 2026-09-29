#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Physics/DroneBreakableWallPanel.h"
#include "Physics/DroneCollisionResponseComponent.h"
#include "Physics/DroneNetPlacementRig.h"
#include "Prototype/DronePrototypePawn.h"

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
			}
		}
	}

	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif
