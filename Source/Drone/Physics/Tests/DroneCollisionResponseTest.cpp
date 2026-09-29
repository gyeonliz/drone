#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Physics/DroneCollisionResponseComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCollisionResponseTest,
	"Drone.Physics.CollisionResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneCollisionResponseTest::RunTest(const FString& Parameters)
{
	const FVector Incoming(1000.0f, 0.0f, 0.0f);
	const FVector WallNormal(-1.0f, 0.0f, 0.0f);
	const FVector Reflected = UDroneCollisionResponseComponent::ComputeReflectedVelocity(
		Incoming, WallNormal, 0.35f, 90.0f, 900.0f);
	TestTrue(TEXT("Wall impact reverses the incoming X direction"), Reflected.X < 0.0f);
	TestTrue(TEXT("Wall response stays within the configured maximum"), Reflected.Size() <= 900.0f + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Wall response clears the surface with the configured minimum speed"),
		FVector::DotProduct(Reflected, WallNormal) >= 90.0f - KINDA_SMALL_NUMBER);

	const FVector Glancing = UDroneCollisionResponseComponent::ComputeReflectedVelocity(
		FVector(800.0f, 400.0f, 0.0f), WallNormal, 0.5f, 0.0f, 900.0f);
	TestTrue(TEXT("Glancing response reverses only the wall-normal direction"), Glancing.X < 0.0f && Glancing.Y > 0.0f);
	return !HasAnyErrors();
}

#endif
