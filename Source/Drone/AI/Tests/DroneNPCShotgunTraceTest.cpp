#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/BoxComponent.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
	AActor* SpawnShotgunTestActor(
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

	UDroneNPCWeaponComponent* AddShotgunTestWeapon(
		AActor* Owner,
		const TCHAR* Name,
		const int32 PelletCount,
		const float SpreadHalfAngleDegrees)
	{
		if (!Owner)
		{
			return nullptr;
		}
		UDroneNPCWeaponComponent* Weapon = NewObject<UDroneNPCWeaponComponent>(Owner, FName(Name));
		Owner->AddInstanceComponent(Weapon);
		Weapon->RegisterComponent();
		Weapon->ConfigureWeapon(EDroneNPCWeaponType::Shotgun);
		Weapon->ConfigureShotgunGreybox(2000.0f, 1.0f, PelletCount, SpreadHalfAngleDegrees);
		return Weapon;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCShotgunTraceTest,
	"Drone.AI.ShotgunTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCShotgunTraceTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Shotgun Trace test World exists"), World);
	if (!World)
	{
		return false;
	}

	AActor* Shooter = SpawnShotgunTestActor(World, FVector(0.0f, 0.0f, 100.0f), FVector(20.0f), TEXT("ShotgunShooter"));
	AActor* Target = SpawnShotgunTestActor(World, FVector(1000.0f, 0.0f, 100.0f), FVector(100.0f), TEXT("ShotgunTarget"));
	TestNotNull(TEXT("Shotgun Shooter exists"), Shooter);
	TestNotNull(TEXT("Shotgun Target exists"), Target);
	if (!Shooter || !Target)
	{
		return false;
	}

	UDroneNPCWeaponComponent* CenteredWeapon = AddShotgunTestWeapon(Shooter, TEXT("CenteredShotgunWeapon"), 7, 0.0f);
	TestNotNull(TEXT("Centered Shotgun Weapon exists"), CenteredWeapon);
	if (!CenteredWeapon)
	{
		return false;
	}

	TestTrue(TEXT("Shotgun accepts an in-range Target"), CenteredWeapon->CanFire(Target, Target->GetActorLocation()));
	TestTrue(TEXT("Shotgun starts and hits an unobstructed Target"), CenteredWeapon->StartFire(Target, Target->GetActorLocation()));
	TestEqual(TEXT("One Shotgun Volley consumes one shell, not one per Pellet"), CenteredWeapon->GetCurrentMagazineAmmo(), 7);
	TestEqual(TEXT("One Trigger creates exactly one Shotgun Volley"), CenteredWeapon->GetShotgunVolleyAttemptCount(), 1);
	TestEqual(TEXT("One Shotgun Volley emits one visual fire event, not one per Pellet"), CenteredWeapon->GetWeaponFiredEventCount(), 1);
	TestEqual(TEXT("One Volley emits every configured Pellet"), CenteredWeapon->GetShotgunPelletTraceCount(), 7);
	TestEqual(TEXT("Zero Spread sends every Pellet into the Target"), CenteredWeapon->GetShotgunTargetHitPelletCount(), 7);
	TestEqual(TEXT("Last Volley records every Pellet endpoint"), CenteredWeapon->GetLastShotgunPelletTraceEnds().Num(), 7);
	TestFalse(TEXT("Immediate repeated Shotgun Volley is rejected by Cooldown"), CenteredWeapon->TryFireShotgunVolley());
	TestEqual(TEXT("Cooldown rejection creates no extra Volley"), CenteredWeapon->GetShotgunVolleyAttemptCount(), 1);
	TestEqual(TEXT("Cooldown rejection emits no extra Shotgun visual event"), CenteredWeapon->GetWeaponFiredEventCount(), 1);
	CenteredWeapon->StopFire();

	AActor* Blocker = SpawnShotgunTestActor(World, FVector(500.0f, 0.0f, 100.0f), FVector(120.0f), TEXT("ShotgunBlocker"));
	UDroneNPCWeaponComponent* BlockedWeapon = AddShotgunTestWeapon(Shooter, TEXT("BlockedShotgunWeapon"), 7, 0.0f);
	TestNotNull(TEXT("Visibility Blocker exists"), Blocker);
	TestNotNull(TEXT("Blocked Shotgun Weapon exists"), BlockedWeapon);
	if (Blocker && BlockedWeapon)
	{
		TestTrue(TEXT("Blocked Shotgun request remains a valid firing request"), BlockedWeapon->StartFire(Target, Target->GetActorLocation()));
		TestEqual(TEXT("Blocked Shotgun still emits every Pellet"), BlockedWeapon->GetShotgunPelletTraceCount(), 7);
		TestEqual(TEXT("Blocker prevents all Target Pellet hits"), BlockedWeapon->GetShotgunTargetHitPelletCount(), 0);
		BlockedWeapon->StopFire();
	}

	Target->SetActorLocation(FVector(3000.0f, 0.0f, 100.0f));
	UDroneNPCWeaponComponent* OutOfRangeWeapon = AddShotgunTestWeapon(Shooter, TEXT("OutOfRangeShotgunWeapon"), 7, 0.0f);
	TestNotNull(TEXT("Out-of-range Shotgun Weapon exists"), OutOfRangeWeapon);
	if (OutOfRangeWeapon)
	{
		TestFalse(TEXT("Shotgun rejects a Target beyond its configured range"), OutOfRangeWeapon->CanFire(Target, Target->GetActorLocation()));
		TestFalse(TEXT("Out-of-range Shotgun StartFire is rejected"), OutOfRangeWeapon->StartFire(Target, Target->GetActorLocation()));
		TestEqual(TEXT("Out-of-range request emits no Pellet"), OutOfRangeWeapon->GetShotgunPelletTraceCount(), 0);
	}

	Target->SetActorLocation(FVector(1000.0f, 0.0f, 100.0f));
	UDroneNPCWeaponComponent* SpreadWeapon = AddShotgunTestWeapon(Shooter, TEXT("SpreadShotgunWeapon"), 7, 8.0f);
	TestNotNull(TEXT("Spread Shotgun Weapon exists"), SpreadWeapon);
	if (SpreadWeapon)
	{
		SpreadWeapon->ConfigureMagazineGreybox(30, 1);
		TestTrue(TEXT("Spread Shotgun starts through the common Trigger"), SpreadWeapon->StartFire(Target, Target->GetActorLocation()));
		const TArray<FVector>& TraceEnds = SpreadWeapon->GetLastShotgunPelletTraceEnds();
		bool bFoundSeparatedPellet = false;
		for (int32 Index = 1; Index < TraceEnds.Num(); ++Index)
		{
			bFoundSeparatedPellet |= !TraceEnds[Index].Equals(TraceEnds[0], 1.0f);
		}
		TestTrue(TEXT("Configured Spread produces separated Pellet endpoints"), bFoundSeparatedPellet);
		TestEqual(TEXT("Last Shotgun shell empties the magazine"), SpreadWeapon->GetCurrentMagazineAmmo(), 0);
		TestFalse(TEXT("Empty Shotgun stops its repeating fire state"), SpreadWeapon->IsFiring());
		TestFalse(TEXT("Empty Shotgun rejects another Trigger"), SpreadWeapon->StartFire(Target, Target->GetActorLocation()));
		TestTrue(TEXT("Explicit Shotgun Reload refills the empty magazine"), SpreadWeapon->Reload());
		TestEqual(TEXT("Shotgun Reload restores configured capacity"), SpreadWeapon->GetCurrentMagazineAmmo(), 1);
		TestEqual(TEXT("Shotgun records one accepted Reload"), SpreadWeapon->GetAcceptedReloadRequestCount(), 1);
		TestEqual(TEXT("Accepted Shotgun Reload emits one completion event"), SpreadWeapon->GetReloadCompletedEventCount(), 1);
	}

	UDroneNPCWeaponComponent* Rifle = NewObject<UDroneNPCWeaponComponent>(Shooter, TEXT("RifleWeaponForShotgunTest"));
	Shooter->AddInstanceComponent(Rifle);
	Rifle->RegisterComponent();
	Rifle->ConfigureWeapon(EDroneNPCWeaponType::Rifle);
	Rifle->ConfigureRifleGreybox(2000.0f, 1.0f);
	TestTrue(TEXT("Rifle keeps the common StartFire contract"), Rifle->StartFire(Target, Target->GetActorLocation()));
	TestEqual(TEXT("Rifle does not execute Shotgun Volley code"), Rifle->GetShotgunVolleyAttemptCount(), 0);
	Rifle->StopFire();

	return true;
}

#endif
