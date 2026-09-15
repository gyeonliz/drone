#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/DroneNPCAIController.h"
#include "AI/DroneNPCCharacter.h"
#include "AI/DroneNPCNavigationFloor.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"

namespace DroneShotgunSystemsTestMap
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneShotgunSystemsTest");
constexpr const TCHAR* MapObjectPath =
	TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneShotgunSystemsTest.Lvl_DroneShotgunSystemsTest");
constexpr const TCHAR* ShotgunNPCClassPath =
	TEXT("/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Shotgun.BP_NPC_Hostile_Shotgun_C");
constexpr const TCHAR* GameModeClassPath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
const FName ShooterTag(TEXT("DroneShotgunSystemsTest.Shooter"));

UWorld* FindPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			return Context.World();
		}
	}
	return nullptr;
}

FRequestPlaySessionParams MakePlayParams()
{
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(1);
	Settings->bLaunchSeparateServer = false;
	Settings->AddToRoot();

	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.EditorPlaySettings = Settings;
	Params.bAllowOnlineSubsystem = false;
	return Params;
}

class FValidateShotgunFirePIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateShotgunFirePIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		if (!PIEWorld || !PIEWorld->HasBegunPlay())
		{
			return FinishOnTimeout(Now, TEXT("Shotgun systems PIE World did not begin play"));
		}

		ADroneNPCCharacter* ShotgunNPC = nullptr;
		int32 ShotgunNPCCount = 0;
		for (TActorIterator<ADroneNPCCharacter> It(PIEWorld); It; ++It)
		{
			if (It->ActorHasTag(ShooterTag))
			{
				ShotgunNPC = *It;
				++ShotgunNPCCount;
			}
		}

		ADronePrototypePawn* Drone = nullptr;
		for (TActorIterator<ADronePrototypePawn> It(PIEWorld); It; ++It)
		{
			Drone = *It;
			break;
		}

		ADroneNPCAIController* Controller = ShotgunNPC
			? Cast<ADroneNPCAIController>(ShotgunNPC->GetController())
			: nullptr;
		UDroneNPCWeaponComponent* Weapon = ShotgunNPC
			? ShotgunNPC->GetNPCWeaponComponent()
			: nullptr;
		LastNPC = ShotgunNPC;
		LastController = Controller;
		LastWeapon = Weapon;
		const bool bVolleyObserved = Weapon
			&& Weapon->GetShotgunVolleyAttemptCount() >= 1
			&& Weapon->GetShotgunProjectileSpawnCount() >= Weapon->GetShotgunPelletCount()
			&& Weapon->GetWeaponFiredEventCount() >= 1;
		if ((!ShotgunNPC || !Drone || !Controller || !Weapon || !bVolleyObserved)
			&& Now - StartedAt <= 15.0)
		{
			return false;
		}

		Test->TestEqual(TEXT("Shotgun systems PIE has exactly one dedicated Shotgun NPC"), ShotgunNPCCount, 1);
		Test->TestNotNull(TEXT("Shotgun systems PIE spawns the playable Drone"), Drone);
		Test->TestNotNull(TEXT("Shotgun NPC is possessed by the project AI Controller"), Controller);
		Test->TestNotNull(TEXT("Shotgun NPC owns the common Weapon Component"), Weapon);
		if (!ShotgunNPC || !Drone || !Controller || !Weapon)
		{
			return FinishOnTimeout(Now, TEXT("Shotgun systems PIE actors or components are missing"));
		}

		const UDroneNPCProfileComponent* Profile = ShotgunNPC->GetNPCProfileComponent();
		Test->TestNotNull(TEXT("Shotgun NPC owns its Profile Component"), Profile);
		Test->TestTrue(TEXT("Dedicated NPC profile is Hostile"), Profile && Profile->IsHostile());
		Test->TestTrue(
			TEXT("Dedicated NPC profile selects Shotgun"),
			Profile && Profile->GetProfile().WeaponType == EDroneNPCWeaponType::Shotgun);
		Test->TestTrue(TEXT("Controller reports the Shotgun role"), Controller->UsesShotgun());
		Test->TestFalse(TEXT("Shotgun NPC is not an MG operator"), Controller->CanUseMGTurret());
		Test->TestTrue(TEXT("Shotgun uses moving Projectile ballistics"), Weapon->UsesProjectileBallistics());
		Test->TestEqual(TEXT("Shotgun test keeps eight Pellets per Volley"), Weapon->GetShotgunPelletCount(), 8);
		Test->TestEqual(TEXT("Shotgun test keeps a six-degree cone half-angle"), Weapon->GetShotgunSpreadHalfAngleDegrees(), 6.0f);
		Test->TestEqual(TEXT("Shotgun test keeps 3500 cm/s projectile speed"), Weapon->GetShotgunProjectileSpeed(), 3500.0f);
		Test->TestEqual(TEXT("Shotgun test keeps an eight-shell magazine"), Weapon->GetMagazineCapacity(), 8);
		Test->TestTrue(TEXT("PIE observes at least one Shotgun Volley"), Weapon->GetShotgunVolleyAttemptCount() >= 1);
		Test->TestTrue(
			TEXT("One observed Volley spawns at least eight Pellet projectiles"),
			Weapon->GetShotgunProjectileSpawnCount() >= Weapon->GetShotgunPelletCount());
		Test->TestTrue(TEXT("Shotgun Volley consumes at least one shell"), Weapon->GetCurrentMagazineAmmo() < Weapon->GetMagazineCapacity());
		Test->TestTrue(TEXT("Shotgun Volley emits its common fire event"), Weapon->GetWeaponFiredEventCount() >= 1);
		Test->TestTrue(TEXT("Shotgun cone debug display is enabled for this test"), Weapon->IsShotgunDebugTraceEnabled());

		const TArray<FVector>& PelletEnds = Weapon->GetLastShotgunPelletTraceEnds();
		const TArray<FVector> BlueprintPelletEnds = Weapon->GetLastShotgunPelletEndpoints();
		Test->TestEqual(TEXT("Last Volley records all eight Pellet endpoints"), PelletEnds.Num(), 8);
		Test->TestEqual(TEXT("Blueprint endpoint getter exposes the same Pellet count"), BlueprintPelletEnds.Num(), PelletEnds.Num());

		FVector EyeLocation = FVector::ZeroVector;
		FRotator EyeRotation = FRotator::ZeroRotator;
		ShotgunNPC->GetActorEyesViewPoint(EyeLocation, EyeRotation);
		const FVector CenterDirection = (Weapon->GetCurrentAimPoint() - EyeLocation).GetSafeNormal();
		bool bEveryPelletInsideCone = !CenterDirection.IsNearlyZero() && PelletEnds.Num() == 8;
		bool bFoundSeparatedPellet = false;
		for (int32 Index = 0; Index < PelletEnds.Num(); ++Index)
		{
			const FVector PelletDirection = (PelletEnds[Index] - EyeLocation).GetSafeNormal();
			const float PelletAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
				FVector::DotProduct(CenterDirection, PelletDirection),
				-1.0f,
				1.0f)));
			bEveryPelletInsideCone &= PelletAngleDegrees <= 6.01f;
			if (Index > 0)
			{
				bFoundSeparatedPellet |= !PelletEnds[Index].Equals(PelletEnds[0], 1.0f);
			}
		}
		Test->TestTrue(TEXT("Every Projectile direction stays inside the six-degree cone"), bEveryPelletInsideCone);
		Test->TestTrue(TEXT("Shotgun Pellets use independently randomized directions"), bFoundSeparatedPellet);
		return true;
	}

private:
	bool FinishOnTimeout(const double Now, const TCHAR* Reason)
	{
		if (Now - StartedAt <= 15.0)
		{
			return false;
		}
		Test->AddError(FString::Printf(TEXT(
			"%s after 15 seconds (NPC=%s Controller=%s Weapon=%s Volleys=%d Projectiles=%d FireEvents=%d)"),
			Reason,
			*GetNameSafe(LastNPC.Get()),
			*GetNameSafe(LastController.Get()),
			*GetNameSafe(LastWeapon.Get()),
			LastWeapon.IsValid() ? LastWeapon->GetShotgunVolleyAttemptCount() : -1,
			LastWeapon.IsValid() ? LastWeapon->GetShotgunProjectileSpawnCount() : -1,
			LastWeapon.IsValid() ? LastWeapon->GetWeaponFiredEventCount() : -1));
		return true;
	}

	FAutomationTestBase* Test;
	double StartedAt = 0.0;
	TWeakObjectPtr<ADroneNPCCharacter> LastNPC;
	TWeakObjectPtr<ADroneNPCAIController> LastController;
	TWeakObjectPtr<UDroneNPCWeaponComponent> LastWeapon;
};
} // namespace DroneShotgunSystemsTestMap

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneShotgunSystemsTestMapAssetTest,
	"Drone.AI.ShotgunSystemsTestMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneShotgunSystemsTestMapAssetTest::RunTest(const FString& Parameters)
{
	using namespace DroneShotgunSystemsTestMap;
	UClass* ShotgunNPCClass = LoadClass<ADroneNPCCharacter>(nullptr, ShotgunNPCClassPath);
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, GameModeClassPath);
	UWorld* World = LoadObject<UWorld>(nullptr, MapObjectPath);
	TestNotNull(TEXT("Dedicated Shotgun NPC Blueprint Class loads"), ShotgunNPCClass);
	TestNotNull(TEXT("Prototype GameMode Class loads"), GameModeClass);
	TestNotNull(TEXT("Shotgun systems test map loads"), World);
	if (!ShotgunNPCClass || !GameModeClass || !World)
	{
		return false;
	}

	TestTrue(TEXT("Shotgun systems test map keeps Prototype GameMode"), World->GetWorldSettings()->DefaultGameMode.Get() == GameModeClass);
	int32 ShotgunNPCCount = 0;
	int32 PlayerStartCount = 0;
	int32 NavigationFloorCount = 0;
	int32 NavBoundsCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		if (Actor->IsA(ShotgunNPCClass) && Actor->ActorHasTag(ShooterTag))
		{
			++ShotgunNPCCount;
		}
		PlayerStartCount += Actor->IsA<APlayerStart>() ? 1 : 0;
		NavigationFloorCount += Actor->IsA<ADroneNPCNavigationFloor>() ? 1 : 0;
		NavBoundsCount += Actor->GetClass()->GetPathName() == TEXT("/Script/NavigationSystem.NavMeshBoundsVolume") ? 1 : 0;
	}
	TestEqual(TEXT("Shotgun test map contains one dedicated Shotgun NPC"), ShotgunNPCCount, 1);
	TestEqual(TEXT("Shotgun test map contains one PlayerStart"), PlayerStartCount, 1);
	TestEqual(TEXT("Shotgun test map contains one project Navigation Floor"), NavigationFloorCount, 1);
	TestEqual(TEXT("Shotgun test map contains one NavMesh bounds volume"), NavBoundsCount, 1);

	const ADroneNPCCharacter* ShotgunDefaults = Cast<ADroneNPCCharacter>(ShotgunNPCClass->GetDefaultObject());
	const UDroneNPCProfileComponent* Profile = ShotgunDefaults ? ShotgunDefaults->GetNPCProfileComponent() : nullptr;
	const UDroneNPCWeaponComponent* Weapon = ShotgunDefaults ? ShotgunDefaults->GetNPCWeaponComponent() : nullptr;
	TestNotNull(TEXT("Shotgun Blueprint CDO owns Profile Component"), Profile);
	TestNotNull(TEXT("Shotgun Blueprint CDO owns Weapon Component"), Weapon);
	TestTrue(TEXT("Shotgun Blueprint profile is Hostile"), Profile && Profile->IsHostile());
	TestTrue(
		TEXT("Shotgun Blueprint profile selects Shotgun"),
		Profile && Profile->GetProfile().WeaponType == EDroneNPCWeaponType::Shotgun);
	TestTrue(TEXT("Shotgun Blueprint cannot claim an MG turret"), Profile && !Profile->GetProfile().bCanUseMGTurret);
	TestTrue(TEXT("Shotgun Blueprint defaults to projectile ballistics"), Weapon && Weapon->UsesProjectileBallistics());
	TestEqual(TEXT("Shotgun Blueprint keeps eight Pellets"), Weapon ? Weapon->GetShotgunPelletCount() : 0, 8);
	TestEqual(TEXT("Shotgun Blueprint keeps six-degree spread"), Weapon ? Weapon->GetShotgunSpreadHalfAngleDegrees() : 0.0f, 6.0f);
	TestEqual(TEXT("Shotgun Blueprint keeps 3500 cm/s Pellet speed"), Weapon ? Weapon->GetShotgunProjectileSpeed() : 0.0f, 3500.0f);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneShotgunSystemsTestMapPIETest,
	"Drone.AI.ShotgunSystemsTestMapPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneShotgunSystemsTestMapPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneShotgunSystemsTestMap;
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains,
		0);
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Shotgun systems PIE test requires an idle Editor"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapPackage));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateShotgunFirePIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
