#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/GameInstance.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Mission/DroneDefinition.h"
#include "Mission/DroneMissionDefinition.h"
#include "Modules/ModuleManager.h"

/**
 * 새 Mission·Drone은 /Game/Drone/Data/{Missions,Drones}에 DA만 만들면 로비 Catalog에 들어가야 한다.
 * 폴더의 모든 Definition이 같은 Asset으로 등록됐는지 대조한다. 개수를 고정하지 않으므로 DA를 추가해도 깨지지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCatalogAutoRegistrationTest,
	"Drone.Flow.CatalogAutoRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace DroneCatalogAutoRegistrationTest
{
template <typename TDefinition>
TArray<TDefinition*> LoadUnder(const TCHAR* Path)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({FString(Path)});
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(Path), Assets, true);
	TArray<TDefinition*> Result;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.IsInstanceOf(TDefinition::StaticClass()))
		{
			if (TDefinition* Definition = Cast<TDefinition>(Asset.GetAsset()))
			{
				Result.Add(Definition);
			}
		}
	}
	return Result;
}
}

bool FDroneCatalogAutoRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace DroneCatalogAutoRegistrationTest;

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* Flow = NewObject<UDroneGameFlowSubsystem>(GameInstance);
	if (!TestTrue(TEXT("Catalog builds from defaults and Data folders"), Flow && Flow->EnsureDefaultCatalog()))
	{
		return false;
	}

	const TArray<UDroneDefinition*> FolderDrones = LoadUnder<UDroneDefinition>(TEXT("/Game/Drone/Data/Drones"));
	const TArray<UDroneMissionDefinition*> FolderMissions = LoadUnder<UDroneMissionDefinition>(TEXT("/Game/Drone/Data/Missions"));
	TestTrue(TEXT("Data/Drones folder has the five baseline Drone Definitions or more"), FolderDrones.Num() >= 5);
	TestTrue(TEXT("Data/Missions folder has the fourteen baseline Mission Definitions or more"), FolderMissions.Num() >= 14);

	for (const UDroneDefinition* Drone : FolderDrones)
	{
		TestTrue(FString::Printf(TEXT("Folder Drone '%s' is registered as the same asset"), *GetPathNameSafe(Drone)),
			Flow->FindDroneDefinition(Drone->DroneId) == Drone);
	}
	for (const UDroneMissionDefinition* Mission : FolderMissions)
	{
		TestTrue(FString::Printf(TEXT("Folder Mission '%s' is registered as the same asset"), *GetPathNameSafe(Mission)),
			Flow->FindMissionDefinition(Mission->MissionId) == Mission);
	}
	TestEqual(TEXT("Catalog has no Mission outside the Data/Missions folder"), Flow->GetRegisteredMissionCount(), FolderMissions.Num());

	// 두 번 불러도 같은 결과(중복 등록·경고 없이 유지)여야 한다.
	const int32 MissionCount = Flow->GetRegisteredMissionCount();
	TestTrue(TEXT("Second catalog build succeeds"), Flow->EnsureDefaultCatalog());
	TestEqual(TEXT("Second catalog build keeps the same Mission count"), Flow->GetRegisteredMissionCount(), MissionCount);
	return true;
}

#endif
