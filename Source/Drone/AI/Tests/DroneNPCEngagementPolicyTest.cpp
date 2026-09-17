#include "AI/DroneNPCEngagementPolicy.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCEngagementPolicyTest,
	"Drone.AI.PersonalWeaponEngagementPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCEngagementPolicyTest::RunTest(const FString& Parameters)
{
	constexpr float WeaponRange = 1600.0f;
	constexpr float CombatLeashRadius = 3000.0f;
	const FVector Origin = FVector::ZeroVector;

	TestTrue(
		TEXT("A fireable target inside the leash is fired on"),
		FDroneNPCEngagementPolicy::Evaluate(
			true, Origin, Origin, FVector(900.0f, 0.0f, 0.0f),
			WeaponRange, CombatLeashRadius)
			== EDroneNPCEngagementDecision::Fire);

	TestTrue(
		TEXT("An out-of-range target inside the leash is pursued"),
		FDroneNPCEngagementPolicy::Evaluate(
			true, Origin, Origin, FVector(2200.0f, 0.0f, 0.0f),
			WeaponRange, CombatLeashRadius)
			== EDroneNPCEngagementDecision::Pursue);

	TestTrue(
		TEXT("A target outside the combat leash is abandoned"),
		FDroneNPCEngagementPolicy::Evaluate(
			true, Origin, Origin, FVector(3200.0f, 0.0f, 0.0f),
			WeaponRange, CombatLeashRadius)
			== EDroneNPCEngagementDecision::Disengage);

	TestTrue(
		TEXT("An NPC pulled outside its combat leash abandons the chase"),
		FDroneNPCEngagementPolicy::Evaluate(
			true, Origin, FVector(3200.0f, 0.0f, 0.0f), FVector(2500.0f, 0.0f, 0.0f),
			WeaponRange, CombatLeashRadius)
			== EDroneNPCEngagementDecision::Disengage);

	TestTrue(
		TEXT("A missing combat origin does not cause a false disengage"),
		FDroneNPCEngagementPolicy::Evaluate(
			false, Origin, FVector(5000.0f, 0.0f, 0.0f), FVector(5900.0f, 0.0f, 0.0f),
			WeaponRange, CombatLeashRadius)
			== EDroneNPCEngagementDecision::Fire);

	TestTrue(
		TEXT("A target one centimetre inside the actual weapon range is fired on"),
		FDroneNPCEngagementPolicy::Evaluate(
			true, Origin, Origin, FVector(1599.0f, 0.0f, 0.0f),
			WeaponRange, CombatLeashRadius)
			== EDroneNPCEngagementDecision::Fire);
	TestTrue(
		TEXT("A target one centimetre outside the actual weapon range is pursued"),
		FDroneNPCEngagementPolicy::Evaluate(
			true, Origin, Origin, FVector(1601.0f, 0.0f, 0.0f),
			WeaponRange, CombatLeashRadius)
			== EDroneNPCEngagementDecision::Pursue);

	TestTrue(
		TEXT("An existing pursuit stops and fires anywhere inside the actual weapon range"),
		FDroneNPCEngagementPolicy::Evaluate(
			true, Origin, Origin, FVector(1550.0f, 0.0f, 0.0f),
			WeaponRange, CombatLeashRadius)
			== EDroneNPCEngagementDecision::Fire);

	const FVector NavDestination(2200.0f, 0.0f, 0.0f);
	TestTrue(
		TEXT("The first pursuit target creates a move request"),
		FDroneNPCEngagementPolicy::ShouldRequestPursuitMove(
			false, FVector::ZeroVector, NavDestination, 150.0f, false));
	TestFalse(
		TEXT("An active move to the same target is not restarted"),
		FDroneNPCEngagementPolicy::ShouldRequestPursuitMove(
			true, NavDestination, NavDestination, 150.0f, true));
	TestFalse(
		TEXT("Small projected target jitter does not restart the path"),
		FDroneNPCEngagementPolicy::ShouldRequestPursuitMove(
			true, NavDestination, NavDestination + FVector(75.0f, 0.0f, 500.0f), 150.0f, true));
	TestTrue(
		TEXT("A meaningful horizontal target move requests a new path"),
		FDroneNPCEngagementPolicy::ShouldRequestPursuitMove(
			true, NavDestination, NavDestination + FVector(175.0f, 0.0f, 0.0f), 150.0f, true));
	TestTrue(
		TEXT("An interrupted move is requested again even when the target stayed still"),
		FDroneNPCEngagementPolicy::ShouldRequestPursuitMove(
			true, NavDestination, NavDestination, 150.0f, false));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
