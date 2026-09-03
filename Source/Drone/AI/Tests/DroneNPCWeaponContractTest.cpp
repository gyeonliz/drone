#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/DroneNPCCharacter.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
	TestNotNull(TEXT("NPC Character CDO owns a replaceable Weapon Visual Component"), NPCDefaults ? NPCDefaults->GetWeaponVisualComponent() : nullptr);
	TestNotNull(TEXT("NPC Character CDO owns a separate Weapon Muzzle Component"), NPCDefaults ? NPCDefaults->GetWeaponMuzzleComponent() : nullptr);
	if (NPCDefaults && NPCDefaults->GetWeaponVisualComponent() && NPCDefaults->GetWeaponMuzzleComponent())
	{
		TestTrue(TEXT("Weapon Visual is attached to the Character Mesh"), NPCDefaults->GetWeaponVisualComponent()->GetAttachParent() == NPCDefaults->GetMesh());
		TestEqual(TEXT("Manny Greybox uses the verified right-hand bone"), NPCDefaults->GetWeaponVisualComponent()->GetAttachSocketName(), FName(TEXT("hand_r")));
		TestEqual(TEXT("Weapon Visual never blocks gameplay collision"), NPCDefaults->GetWeaponVisualComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Weapon Visual does not affect Navigation"), NPCDefaults->GetWeaponVisualComponent()->CanEverAffectNavigation());
		TestTrue(TEXT("Muzzle marker is a child of the replaceable Weapon Visual"), NPCDefaults->GetWeaponMuzzleComponent()->GetAttachParent() == NPCDefaults->GetWeaponVisualComponent());
	}
	if (NPCDefaults)
	{
		TestNotNull(TEXT("Rifle has a temporary Manny fire Animation"), NPCDefaults->GetGreyboxFireAnimation(EDroneNPCWeaponType::Rifle));
		TestNotNull(TEXT("Shotgun reuses a temporary Manny fire Animation"), NPCDefaults->GetGreyboxFireAnimation(EDroneNPCWeaponType::Shotgun));
		TestNotNull(TEXT("Rifle has a temporary Manny reload Animation"), NPCDefaults->GetGreyboxReloadAnimation(EDroneNPCWeaponType::Rifle));
		TestNotNull(TEXT("Shotgun reuses a temporary Manny reload Animation"), NPCDefaults->GetGreyboxReloadAnimation(EDroneNPCWeaponType::Shotgun));
		TestNull(TEXT("Unarmed NPC has no fire Animation"), NPCDefaults->GetGreyboxFireAnimation(EDroneNPCWeaponType::Unarmed));
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
	TestEqual(TEXT("Rifle starts with a 30-round Greybox magazine"), Rifle->GetMagazineCapacity(), 30);
	TestEqual(TEXT("Rifle magazine starts full"), Rifle->GetCurrentMagazineAmmo(), 30);
	TestEqual(TEXT("Shotgun starts with an 8-shell Greybox magazine"), Shotgun->GetMagazineCapacity(), 8);
	TestEqual(TEXT("Shotgun magazine starts full"), Shotgun->GetCurrentMagazineAmmo(), 8);
	TestEqual(TEXT("Unarmed has no magazine"), Unarmed->GetMagazineCapacity(), 0);

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
	TestFalse(TEXT("A full Rifle magazine rejects redundant Reload"), Rifle->Reload());
	TestFalse(TEXT("A full Shotgun magazine rejects redundant Reload"), Shotgun->Reload());
	TestFalse(TEXT("Unarmed rejects Reload"), Unarmed->Reload());
	TestEqual(TEXT("Rifle records one Reload request"), Rifle->GetReloadRequestCount(), 1);
	TestEqual(TEXT("Shotgun records one Reload request"), Shotgun->GetReloadRequestCount(), 1);

	return true;
}

#endif
