#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "AI/DroneSmartObjectStation.h"
#include "AI/Weapons/DroneNPCProjectile.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
AActor* SpawnProjectileTestActor(UWorld* World, const FVector& Location, const TCHAR* Name)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = FName(Name);
	AActor* Actor = World
		? World->SpawnActor<AActor>(Location, FRotator::ZeroRotator, SpawnParameters)
		: nullptr;
	if (!Actor)
	{
		return nullptr;
	}

	UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("ProjectileCollision"));
	Actor->SetRootComponent(Box);
	Actor->AddInstanceComponent(Box);
	Box->SetBoxExtent(FVector(30.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	Actor->SetActorLocation(Location);
	return Actor;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCProjectileBallisticsTest,
	"Drone.AI.ProjectileBallistics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCProjectileBallisticsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Projectile test World exists"), World);
	if (!World)
	{
		return false;
	}

	AActor* Shooter = SpawnProjectileTestActor(World, FVector(0.0f, 0.0f, 100.0f), TEXT("ProjectileShooter"));
	AActor* Target = SpawnProjectileTestActor(World, FVector(1000.0f, 0.0f, 100.0f), TEXT("ProjectileTarget"));
	TestNotNull(TEXT("Projectile Shooter exists"), Shooter);
	TestNotNull(TEXT("Projectile Target exists"), Target);
	if (!Shooter || !Target)
	{
		return false;
	}

	UDroneNPCWeaponComponent* Weapon = NewObject<UDroneNPCWeaponComponent>(Shooter, TEXT("ProjectileWeapon"));
	Shooter->AddInstanceComponent(Weapon);
	Weapon->RegisterComponent();
	TestTrue(TEXT("Personal weapons use projectile ballistics by default"), Weapon->UsesProjectileBallistics());
	TestEqual(TEXT("Rifle default projectile speed is 4500 cm/s"), Weapon->GetRifleProjectileSpeed(), 4500.0f);
	TestEqual(TEXT("Shotgun default projectile speed is 3500 cm/s"), Weapon->GetShotgunProjectileSpeed(), 3500.0f);
	TestEqual(TEXT("Rifle default cone half-angle is 2.5 degrees"), Weapon->GetRifleSpreadHalfAngleDegrees(), 2.5f);
	TestEqual(TEXT("Shotgun default cone half-angle is 6 degrees"), Weapon->GetShotgunSpreadHalfAngleDegrees(), 6.0f);

	Weapon->ConfigureWeapon(EDroneNPCWeaponType::Rifle);
	Weapon->ConfigureRifleGreybox(2000.0f, 1.0f);
	TestTrue(TEXT("Rifle accepts a projectile firing request"), Weapon->StartFire(Target, Target->GetActorLocation()));
	TestEqual(TEXT("Rifle spawns one moving projectile"), Weapon->GetRifleProjectileSpawnCount(), 1);
	TestEqual(TEXT("Projectile mode performs no instant Rifle Trace"), Weapon->GetRifleTraceAttemptCount(), 0);
	TestEqual(TEXT("Projectile spawn consumes one Rifle round"), Weapon->GetCurrentMagazineAmmo(), 29);
	ADroneNPCProjectile* RifleProjectile = Weapon->GetLastSpawnedProjectile();
	TestNotNull(TEXT("Rifle exposes its last spawned projectile"), RifleProjectile);
	if (RifleProjectile)
	{
		TestTrue(TEXT("Rifle projectile records its source"),
			RifleProjectile->GetProjectileSource() == EDroneNPCProjectileSource::Rifle);
		TestTrue(TEXT("Rifle projectile records its intended Target"), RifleProjectile->GetIntendedTarget() == Target);
		TestEqual(TEXT("Rifle projectile carries 10 damage"), RifleProjectile->GetProjectileDamage(), 10.0f);
		TestEqual(TEXT("Rifle projectile carries configured speed"), RifleProjectile->GetProjectileSpeed(), 4500.0f);
		const FVector RifleCenterDirection = (Target->GetActorLocation() - RifleProjectile->GetActorLocation()).GetSafeNormal();
		const float RifleShotAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(RifleCenterDirection, RifleProjectile->GetActorForwardVector()),
			-1.0f,
			1.0f)));
		TestTrue(TEXT("Rifle projectile direction remains inside its 2.5 degree cone"), RifleShotAngleDegrees <= 2.51f);

		// 실제 World Tick 시간에 의존하지 않고 Component Hit 계약부터 Weapon 결과까지 직접 검증한다.
		FHitResult TargetHit;
		RifleProjectile->NotifyHit(
			RifleProjectile->GetCollisionComponent(),
			Target,
			Cast<UPrimitiveComponent>(Target->GetRootComponent()),
			true,
			Target->GetActorLocation(),
			-FVector::ForwardVector,
			FVector::ZeroVector,
			TargetHit);
		TestEqual(TEXT("Rifle projectile Target impact is reported once"), Weapon->GetRifleTargetHitCount(), 1);
		TestTrue(TEXT("Rifle projectile impact records the intended Target"), Weapon->GetLastRifleHitActor() == Target);
	}
	Weapon->StopFire();

	Weapon->ConfigureWeapon(EDroneNPCWeaponType::Shotgun);
	Weapon->ConfigureShotgunGreybox(1600.0f, 1.0f, 4, 6.0f);
	TestTrue(TEXT("Shotgun accepts a projectile Volley"), Weapon->StartFire(Target, Target->GetActorLocation()));
	TestEqual(TEXT("Four-pellet Shotgun spawns four moving projectiles"), Weapon->GetShotgunProjectileSpawnCount(), 4);
	TestEqual(TEXT("Projectile mode performs no instant Shotgun Pellet Trace"), Weapon->GetShotgunPelletTraceCount(), 0);
	TestEqual(TEXT("Projectile Volley consumes one shell"), Weapon->GetCurrentMagazineAmmo(), 7);
	ADroneNPCProjectile* ShotgunProjectile = Weapon->GetLastSpawnedProjectile();
	TestNotNull(TEXT("Shotgun exposes its last spawned projectile"), ShotgunProjectile);
	if (ShotgunProjectile)
	{
		TestTrue(TEXT("Shotgun projectile records its source"),
			ShotgunProjectile->GetProjectileSource() == EDroneNPCProjectileSource::Shotgun);
		TestEqual(TEXT("Shotgun projectile carries configured speed"), ShotgunProjectile->GetProjectileSpeed(), 3500.0f);
		const FVector ShotgunCenterDirection = (Target->GetActorLocation() - ShotgunProjectile->GetActorLocation()).GetSafeNormal();
		const float ShotgunPelletAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(ShotgunCenterDirection, ShotgunProjectile->GetActorForwardVector()),
			-1.0f,
			1.0f)));
		TestTrue(TEXT("Shotgun projectile direction remains inside its 6 degree cone"), ShotgunPelletAngleDegrees <= 6.01f);

		FHitResult TargetHit;
		ShotgunProjectile->NotifyHit(
			ShotgunProjectile->GetCollisionComponent(),
			Target,
			Cast<UPrimitiveComponent>(Target->GetRootComponent()),
			true,
			Target->GetActorLocation(),
			-FVector::ForwardVector,
			FVector::ZeroVector,
			TargetHit);
		TestEqual(TEXT("Shotgun projectile Target impact is reported per Pellet"),
			Weapon->GetShotgunTargetHitPelletCount(),
			1);
	}
	Weapon->StopFire();

	const ADroneSmartObjectStation* StationDefaults = GetDefault<ADroneSmartObjectStation>();
	TestTrue(TEXT("MG Turret uses projectile ballistics by default"),
		StationDefaults && StationDefaults->UsesMGTurretProjectileBallistics());
	TestEqual(TEXT("MG Turret default projectile speed is 5500 cm/s"),
		StationDefaults ? StationDefaults->GetMGTurretProjectileSpeed() : 0.0f,
		5500.0f);
	TestEqual(TEXT("MG Turret default cone half-angle is 3.5 degrees"),
		StationDefaults ? StationDefaults->GetMGTurretSpreadHalfAngleDegrees() : 0.0f,
		3.5f);
	return true;
}

#endif
