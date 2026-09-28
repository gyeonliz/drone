#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationCommon.h"
#include "Weapons/DroneGroundWeaponComponent.h"
#include "Weapons/DronePlayerProjectile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneGroundWeaponTest,
	"Drone.Weapons.GroundUGV",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneGroundWeaponTest::RunTest(const FString& Parameters)
{
	const UDroneGroundWeaponComponent* Defaults = GetDefault<UDroneGroundWeaponComponent>();
	const ADronePlayerProjectile* ProjectileDefaults = GetDefault<ADronePlayerProjectile>();
	TestNotNull(TEXT("Ground Weapon defaults exist"), Defaults);
	TestNotNull(TEXT("Player Projectile defaults exist"), ProjectileDefaults);
	if (!Defaults || !ProjectileDefaults)
	{
		return false;
	}
	TestFalse(TEXT("Ground Weapon does not Tick"), Defaults->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Player Projectile does not Tick"), ProjectileDefaults->PrimaryActorTick.bCanEverTick);
	TestEqual(TEXT("Primary Greybox damage needs four hits for health 100"), Defaults->GetPrimaryDamage(), 25.0f);
	TestEqual(TEXT("Secondary Greybox damage can destroy health 100 target"), Defaults->GetSecondaryDamage(), 100.0f);

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		AddError(TEXT("Could not create Ground Weapon test World"));
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();
	ADronePrototypePawn* Pawn = World->SpawnActor<ADronePrototypePawn>();
	TestNotNull(TEXT("Ground Weapon test Pawn spawns"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	UDroneGroundWeaponComponent* Weapon = Pawn->GetGroundWeaponComponent();
	TestNotNull(TEXT("Prototype Pawn owns Ground Weapon"), Weapon);
	TestNotNull(TEXT("Prototype Pawn owns gun muzzle anchor"), Pawn->GetGroundGunMuzzleAnchor());
	TestNotNull(TEXT("Prototype Pawn owns grenade muzzle anchor"), Pawn->GetGroundGrenadeMuzzleAnchor());
	if (Weapon)
	{
		TestFalse(TEXT("Ground Weapon starts disabled before applying a UGV Definition"), Weapon->IsFeatureEnabled());
		Weapon->ConfigureFeatureEnabled(true);
		TestTrue(TEXT("Enabled Ground Weapon fires primary projectile"), Weapon->FirePrimary(Pawn->GetGroundGunMuzzleAnchor()));
		ADronePlayerProjectile* PrimaryProjectile = Weapon->GetLastSpawnedProjectile();
		TestNotNull(TEXT("Primary projectile is retained for presentation/debug"), PrimaryProjectile);
		if (PrimaryProjectile)
		{
			TestEqual(TEXT("Primary uses direct damage mode"), PrimaryProjectile->GetProjectileMode(), EDronePlayerProjectileMode::Direct);
			TestEqual(TEXT("Primary projectile receives configured damage"), PrimaryProjectile->GetProjectileDamage(), 25.0f);
		}
		TestTrue(TEXT("Enabled Ground Weapon fires independent secondary projectile"), Weapon->FireSecondary(Pawn->GetGroundGrenadeMuzzleAnchor()));
		ADronePlayerProjectile* SecondaryProjectile = Weapon->GetLastSpawnedProjectile();
		TestNotNull(TEXT("Secondary projectile is retained for presentation/debug"), SecondaryProjectile);
		if (SecondaryProjectile)
		{
			TestEqual(TEXT("Secondary uses radial mode"), SecondaryProjectile->GetProjectileMode(), EDronePlayerProjectileMode::Radial);
			TestTrue(TEXT("Secondary has a positive radial damage radius"), SecondaryProjectile->GetRadialDamageRadius() > 0.0f);
		}
	}
	return !HasAnyErrors();
}

#endif
