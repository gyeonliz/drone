#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/AssetManager.h"
#include "Mission/DroneMissionDefinition.h"

/**
 * BUILD-PACKAGE-01: Config/DefaultGame.ini의 Asset Manager 설정이 읽혀 미션·기체 Data Asset이
 * Primary Asset으로 등록되고 "항상 쿠킹"인지 확인한다. 미션 DA의 MissionMap은 Soft 참조라 DA와 함께 쿠킹된다.
 * 실제 쿠킹(패키징) 성공까지 보장하지는 않는다 — 그건 패키징 실행으로 따로 확인한다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePackagingPrimaryAssetsTest,
	"Drone.Flow.PackagingPrimaryAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDronePackagingPrimaryAssetsTest::RunTest(const FString& Parameters)
{
	if (!UAssetManager::IsInitialized())
	{
		AddError(TEXT("Asset Manager is not initialized"));
		return false;
	}
	UAssetManager& Manager = UAssetManager::Get();

	auto CheckType = [this, &Manager](const TCHAR* TypeName, const int32 MinimumCount)
	{
		TArray<FPrimaryAssetId> Ids;
		Manager.GetPrimaryAssetIdList(FPrimaryAssetType(TypeName), Ids);
		TestTrue(FString::Printf(TEXT("%s registers at least %d Primary Assets (found %d)"), TypeName, MinimumCount, Ids.Num()),
			Ids.Num() >= MinimumCount);
		for (const FPrimaryAssetId& Id : Ids)
		{
			const FPrimaryAssetRules Rules = Manager.GetPrimaryAssetRules(Id);
			TestEqual(FString::Printf(TEXT("%s is always cooked"), *Id.ToString()),
				static_cast<int32>(Rules.CookRule), static_cast<int32>(EPrimaryAssetCookRule::AlwaysCook));
		}
		return Ids;
	};

	const TArray<FPrimaryAssetId> Missions = CheckType(TEXT("DroneMission"), 14);
	CheckType(TEXT("DroneDefinition"), 5);

	for (const FPrimaryAssetId& Id : Missions)
	{
		const UDroneMissionDefinition* Mission = Cast<UDroneMissionDefinition>(Manager.GetPrimaryAssetPath(Id).TryLoad());
		TestTrue(FString::Printf(TEXT("%s points at a Mission Map to cook with it"), *Id.ToString()),
			Mission && !Mission->MissionMap.IsNull());
	}
	return true;
}

#endif
