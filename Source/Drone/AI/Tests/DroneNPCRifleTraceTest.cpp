#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/BoxComponent.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
	AActor* SpawnRifleTestActor(
		UWorld* World,
		const FVector& Location,
		const FVector& BoxExtent,
		const TCHAR* Name)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = FName(Name);
		AActor* Actor = World ? World->SpawnActor<AActor>(Location, FRotator::ZeroRotator, SpawnParameters) : nullptr;
		if (!Actor)
		{
			return nullptr;
		}

		UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("VisibilityCollision"));
		Actor->SetRootComponent(Box);
		Actor->AddInstanceComponent(Box);
		Box->SetBoxExtent(BoxExtent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	}

	UDroneNPCWeaponComponent* AddRifleTestWeapon(AActor* Owner, const TCHAR* Name)
	{
		if (!Owner)
		{
			return nullptr;
		}
		UDroneNPCWeaponComponent* Weapon = NewObject<UDroneNPCWeaponComponent>(Owner, FName(Name));
		Owner->AddInstanceComponent(Weapon);
		Weapon->RegisterComponent();
		Weapon->ConfigureWeapon(EDroneNPCWeaponType::Rifle);
		Weapon->ConfigureRifleGreybox(2000.0f, 1.0f);
		return Weapon;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCRifleTraceTest,
	"Drone.AI.RifleTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCRifleTraceTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Rifle Trace test World exists"), World);
	if (!World)
	{
		return false;
	}

	AActor* Shooter = SpawnRifleTestActor(World, FVector(0.0f, 0.0f, 100.0f), FVector(20.0f), TEXT("RifleShooter"));
	AActor* Target = SpawnRifleTestActor(World, FVector(1000.0f, 0.0f, 100.0f), FVector(50.0f), TEXT("RifleTarget"));
	TestNotNull(TEXT("Rifle Shooter exists"), Shooter);
	TestNotNull(TEXT("Rifle Target exists"), Target);
	if (!Shooter || !Target)
	{
		return false;
	}

	UDroneNPCWeaponComponent* OpenWeapon = AddRifleTestWeapon(Shooter, TEXT("OpenRifleWeapon"));
	TestNotNull(TEXT("Open Rifle Weapon exists"), OpenWeapon);
	if (!OpenWeapon)
	{
		return false;
	}

	TestTrue(TEXT("Rifle accepts an in-range Target"), OpenWeapon->CanFire(Target, Target->GetActorLocation()));
	TestTrue(TEXT("Rifle starts and hits an unobstructed Target"), OpenWeapon->StartFire(Target, Target->GetActorLocation()));
	TestEqual(TEXT("One StartFire creates exactly one Rifle Trace"), OpenWeapon->GetRifleTraceAttemptCount(), 1);
	TestEqual(TEXT("Unobstructed Rifle Trace records one Target hit"), OpenWeapon->GetRifleTargetHitCount(), 1);
	TestTrue(TEXT("Unobstructed Rifle Trace reports the Target Actor"), OpenWeapon->GetLastRifleHitActor() == Target);
	TestFalse(TEXT("Immediate repeated Rifle shot is rejected by Cooldown"), OpenWeapon->TryFireRifleShot());
	TestEqual(TEXT("Cooldown rejection does not create another Trace"), OpenWeapon->GetRifleTraceAttemptCount(), 1);
	OpenWeapon->StopFire();

	AActor* Blocker = SpawnRifleTestActor(World, FVector(500.0f, 0.0f, 100.0f), FVector(60.0f), TEXT("RifleBlocker"));
	UDroneNPCWeaponComponent* BlockedWeapon = AddRifleTestWeapon(Shooter, TEXT("BlockedRifleWeapon"));
	TestNotNull(TEXT("Visibility Blocker exists"), Blocker);
	TestNotNull(TEXT("Blocked Rifle Weapon exists"), BlockedWeapon);
	if (Blocker && BlockedWeapon)
	{
		TestTrue(TEXT("Blocked Rifle request remains a valid firing request"), BlockedWeapon->StartFire(Target, Target->GetActorLocation()));
		TestEqual(TEXT("Blocked Rifle still performs one Trace"), BlockedWeapon->GetRifleTraceAttemptCount(), 1);
		TestEqual(TEXT("Blocker prevents a Target hit"), BlockedWeapon->GetRifleTargetHitCount(), 0);
		TestTrue(TEXT("Rifle reports the first Visibility Blocker"), BlockedWeapon->GetLastRifleHitActor() == Blocker);
		BlockedWeapon->StopFire();
	}

	Target->SetActorLocation(FVector(3000.0f, 0.0f, 100.0f));
	UDroneNPCWeaponComponent* OutOfRangeWeapon = AddRifleTestWeapon(Shooter, TEXT("OutOfRangeRifleWeapon"));
	TestNotNull(TEXT("Out-of-range Rifle Weapon exists"), OutOfRangeWeapon);
	if (OutOfRangeWeapon)
	{
		TestFalse(TEXT("Rifle rejects a Target beyond its configured range"), OutOfRangeWeapon->CanFire(Target, Target->GetActorLocation()));
		TestFalse(TEXT("Out-of-range StartFire is rejected"), OutOfRangeWeapon->StartFire(Target, Target->GetActorLocation()));
		TestEqual(TEXT("Out-of-range request performs no Trace"), OutOfRangeWeapon->GetRifleTraceAttemptCount(), 0);
	}

	UDroneNPCWeaponComponent* Shotgun = NewObject<UDroneNPCWeaponComponent>(Shooter, TEXT("ShotgunWeapon"));
	Shooter->AddInstanceComponent(Shotgun);
	Shotgun->RegisterComponent();
	Shotgun->ConfigureWeapon(EDroneNPCWeaponType::Shotgun);
	Target->SetActorLocation(FVector(1000.0f, 0.0f, 100.0f));
	TestTrue(TEXT("Shotgun keeps the common StartFire contract"), Shotgun->StartFire(Target, Target->GetActorLocation()));
	TestEqual(TEXT("Shotgun does not execute Rifle Trace code"), Shotgun->GetRifleTraceAttemptCount(), 0);
	Shotgun->StopFire();

	return true;
}

#endif
