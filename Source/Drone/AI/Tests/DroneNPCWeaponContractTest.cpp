#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/DroneNPCCharacter.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCWeaponContractTest,
	"Drone.AI.WeaponContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCWeaponContractTest::RunTest(const FString& Parameters)
{
	const ADroneNPCCharacter* NPCDefaults = GetDefault<ADroneNPCCharacter>();
	TestNotNull(TEXT("NPC Character CDO owns the common Weapon Component"), NPCDefaults ? NPCDefaults->GetNPCWeaponComponent() : nullptr);
	if (NPCDefaults && NPCDefaults->GetNPCWeaponComponent())
	{
		TestFalse(TEXT("Common Weapon Component does not tick"), NPCDefaults->GetNPCWeaponComponent()->PrimaryComponentTick.bCanEverTick);
	}

	AActor* TargetActor = GetMutableDefault<AActor>();
	const FVector AimPoint(120.0f, -35.0f, 80.0f);
	TestNotNull(TEXT("Weapon contract has a deterministic target Actor"), TargetActor);

	UDroneNPCWeaponComponent* Rifle = NewObject<UDroneNPCWeaponComponent>();
	UDroneNPCWeaponComponent* Shotgun = NewObject<UDroneNPCWeaponComponent>();
	UDroneNPCWeaponComponent* Unarmed = NewObject<UDroneNPCWeaponComponent>();
	TestNotNull(TEXT("Rifle contract instance exists"), Rifle);
	TestNotNull(TEXT("Shotgun contract instance exists"), Shotgun);
	TestNotNull(TEXT("Unarmed contract instance exists"), Unarmed);
	if (!Rifle || !Shotgun || !Unarmed || !TargetActor)
	{
		return false;
	}

	Rifle->ConfigureWeapon(EDroneNPCWeaponType::Rifle);
	Shotgun->ConfigureWeapon(EDroneNPCWeaponType::Shotgun);
	Unarmed->ConfigureWeapon(EDroneNPCWeaponType::Unarmed);

	TestTrue(TEXT("Rifle accepts the common CanFire contract"), Rifle->CanFire(TargetActor, AimPoint));
	TestTrue(TEXT("Shotgun accepts the same CanFire contract"), Shotgun->CanFire(TargetActor, AimPoint));
	TestFalse(TEXT("Unarmed rejects the common CanFire contract"), Unarmed->CanFire(TargetActor, AimPoint));
	TestFalse(TEXT("Common contract rejects a missing Target"), Rifle->CanFire(nullptr, AimPoint));

	TestTrue(TEXT("Rifle accepts StartFire through the common path"), Rifle->StartFire(TargetActor, AimPoint));
	TestTrue(TEXT("Shotgun accepts StartFire through the same path"), Shotgun->StartFire(TargetActor, AimPoint));
	TestFalse(TEXT("Unarmed StartFire is rejected"), Unarmed->StartFire(TargetActor, AimPoint));
	TestTrue(TEXT("Rifle records active firing state"), Rifle->IsFiring());
	TestTrue(TEXT("Shotgun records active firing state"), Shotgun->IsFiring());
	TestTrue(TEXT("Rifle records the Target Actor"), Rifle->GetCurrentTarget() == TargetActor);
	TestTrue(TEXT("Shotgun records the same Target Actor contract"), Shotgun->GetCurrentTarget() == TargetActor);
	TestTrue(TEXT("Rifle records the Aim Point"), Rifle->GetCurrentAimPoint().Equals(AimPoint));
	TestTrue(TEXT("Shotgun records the same Aim Point contract"), Shotgun->GetCurrentAimPoint().Equals(AimPoint));
	TestEqual(TEXT("Rifle records one accepted fire request"), Rifle->GetAcceptedFireRequestCount(), 1);
	TestEqual(TEXT("Shotgun records one accepted fire request"), Shotgun->GetAcceptedFireRequestCount(), 1);
	TestEqual(TEXT("Unarmed records the rejected request without accepting it"), Unarmed->GetAcceptedFireRequestCount(), 0);

	Rifle->StopFire();
	Shotgun->StopFire();
	TestFalse(TEXT("StopFire clears Rifle active state"), Rifle->IsFiring());
	TestFalse(TEXT("StopFire clears Shotgun active state"), Shotgun->IsFiring());
	TestNull(TEXT("StopFire clears the Rifle Target"), Rifle->GetCurrentTarget());
	TestNull(TEXT("StopFire clears the Shotgun Target"), Shotgun->GetCurrentTarget());
	TestTrue(TEXT("Rifle exposes the common Reload request"), Rifle->Reload());
	TestTrue(TEXT("Shotgun exposes the same Reload request"), Shotgun->Reload());
	TestFalse(TEXT("Unarmed rejects Reload"), Unarmed->Reload());
	TestEqual(TEXT("Rifle records one Reload request"), Rifle->GetReloadRequestCount(), 1);
	TestEqual(TEXT("Shotgun records one Reload request"), Shotgun->GetReloadRequestCount(), 1);

	return true;
}

#endif
