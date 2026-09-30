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

	// Regression: wall contact must scale continuously from a gentle touch to a hard strike.
	// A fixed minimum kick makes both cases leave the wall at the same speed and feels like
	// a binary arcade bounce instead of the contacted rotor/wing pushing the drone away.
	const FVector GentleContact = UDroneCollisionResponseComponent::ComputeReflectedVelocity(
		FVector(20.0f, 0.0f, 0.0f), WallNormal, 0.35f, 90.0f, 900.0f);
	const FVector MediumContact = UDroneCollisionResponseComponent::ComputeReflectedVelocity(
		FVector(80.0f, 0.0f, 0.0f), WallNormal, 0.35f, 90.0f, 900.0f);
	TestTrue(TEXT("Gentle wall contact still produces an outward response"),
		FVector::DotProduct(GentleContact, WallNormal) > 0.0f);
	TestTrue(TEXT("Wall pushback grows with contact speed instead of using one fixed kick"),
		FVector::DotProduct(GentleContact, WallNormal)
			< FVector::DotProduct(MediumContact, WallNormal));
	const float GentleSeparation = UDroneCollisionResponseComponent::ComputeSpeedScaledSeparationDistance(
		20.0f, 0.25f, 6.0f, 600.0f);
	const float HardSeparation = UDroneCollisionResponseComponent::ComputeSpeedScaledSeparationDistance(
		600.0f, 0.25f, 6.0f, 600.0f);
	TestTrue(TEXT("Gentle contact uses a small non-zero depenetration distance"), GentleSeparation > 0.0f);
	TestTrue(TEXT("Hard contact separates farther than gentle contact"), HardSeparation > GentleSeparation);

	const FVector GentleWingKick = UDroneCollisionResponseComponent::ComputeContactAngularKickAxisAngleDegrees(
		FVector(0.0f, 120.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), 20.0f, 12.0f, 600.0f, 120.0f);
	const FVector HardWingKick = UDroneCollisionResponseComponent::ComputeContactAngularKickAxisAngleDegrees(
		FVector(0.0f, 120.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), 600.0f, 12.0f, 600.0f, 120.0f);
	TestTrue(TEXT("Wing-tip contact produces an attitude kick"), !GentleWingKick.IsNearlyZero());
	TestTrue(TEXT("Wing-tip attitude kick grows with impact speed"), HardWingKick.Size() > GentleWingKick.Size());
	TestTrue(TEXT("Floor contact does not create the wall-contact attitude kick"),
		UDroneCollisionResponseComponent::ComputeContactAngularKickAxisAngleDegrees(
			FVector(120.0f, 0.0f, 0.0f), FVector::UpVector, 600.0f, 12.0f, 600.0f, 120.0f).IsNearlyZero());

	const UDroneCollisionResponseComponent* Defaults = GetDefault<UDroneCollisionResponseComponent>();
	TestNotNull(TEXT("Collision response defaults exist"), Defaults);
	if (Defaults)
	{
		TestTrue(TEXT("Gentle wall contact is not discarded by the default speed gate"),
			Defaults->GetMinimumWallContactSpeed() <= 1.0f);
		TestTrue(TEXT("Rotor/wing contact probes are enabled by default"), Defaults->UsesRotorContactProbes());
	}

	TestTrue(
		TEXT("Horizontal wall normals are eligible for common Drone bounce"),
		UDroneCollisionResponseComponent::IsWallLikeSurfaceNormal(FVector(-1.0f, 0.0f, 0.0f), 0.72f));
	TestTrue(
		TEXT("Sloped structural wall normals remain eligible"),
		UDroneCollisionResponseComponent::IsWallLikeSurfaceNormal(FVector(-0.8f, 0.0f, 0.45f), 0.72f));
	TestFalse(
		TEXT("Floor normals are not treated as wall bounce surfaces"),
		UDroneCollisionResponseComponent::IsWallLikeSurfaceNormal(FVector::UpVector, 0.72f));
	TestFalse(
		TEXT("Ceiling normals are not treated as wall bounce surfaces"),
		UDroneCollisionResponseComponent::IsWallLikeSurfaceNormal(FVector::DownVector, 0.72f));
	return !HasAnyErrors();
}

#endif
