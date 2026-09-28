#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Flow/DroneMissionGameMode.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneMissionDamageTarget.h"
#include "Mission/DroneMissionDirector.h"
#include "Mission/DroneMissionReturnZone.h"
#include "Mission/DroneMissionTrigger.h"
#include "Tutorial/DroneTutorialHeadingZone.h"
#include "Tutorial/DroneTutorialHoverZone.h"

namespace DroneMissionFrameworkAssetTest
{
constexpr const TCHAR* ManagerClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Managers/BP_DroneMissionManager.BP_DroneMissionManager_C");
constexpr const TCHAR* ControllerClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Managers/BP_DroneMissionPlayerController.BP_DroneMissionPlayerController_C");
constexpr const TCHAR* GameModeClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Managers/BP_DroneMissionGameMode.BP_DroneMissionGameMode_C");
constexpr const TCHAR* ObjectiveTriggerClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Triggers/BP_MissionObjectiveTrigger.BP_MissionObjectiveTrigger_C");
constexpr const TCHAR* FailureTriggerClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Triggers/BP_MissionFailureTrigger.BP_MissionFailureTrigger_C");
constexpr const TCHAR* ReturnZoneClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Triggers/BP_MissionReturnZone.BP_MissionReturnZone_C");
constexpr const TCHAR* DamageTargetClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Targets/BP_MissionDamageTarget.BP_MissionDamageTarget_C");
constexpr const TCHAR* HoverZoneClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Tutorial/BP_TutorialHoverZone.BP_TutorialHoverZone_C");
constexpr const TCHAR* HeadingZoneClassPath =
	TEXT("/Game/Drone/Mission/Blueprints/Tutorial/BP_TutorialHeadingZone.BP_TutorialHeadingZone_C");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMissionFrameworkAssetTest,
	"Drone.Mission.FrameworkAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneMissionFrameworkAssetTest::RunTest(const FString& Parameters)
{
	using namespace DroneMissionFrameworkAssetTest;

	const ADroneMissionTrigger* NativeTrigger = GetDefault<ADroneMissionTrigger>();
	TestNotNull(TEXT("Mission Trigger owns an editable Box"), NativeTrigger ? NativeTrigger->GetTriggerBox() : nullptr);
	TestTrue(
		TEXT("Mission Trigger overlaps Pawn and WorldDynamic without Tick"),
		NativeTrigger
			&& NativeTrigger->GetTriggerBox()
			&& NativeTrigger->GetTriggerBox()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap
			&& NativeTrigger->GetTriggerBox()->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Overlap
			&& !NativeTrigger->PrimaryActorTick.bCanEverTick);

	const ADroneMissionDamageTarget* NativeTarget = GetDefault<ADroneMissionDamageTarget>();
	TestNotNull(TEXT("Mission Damage Target owns collision"), NativeTarget ? NativeTarget->GetTargetCollision() : nullptr);
	TestNotNull(TEXT("Mission Damage Target owns a replaceable visual"), NativeTarget ? NativeTarget->GetTargetVisual() : nullptr);
	TestNotNull(TEXT("Mission Damage Target owns Health"), NativeTarget ? NativeTarget->GetHealthComponent() : nullptr);
	TestEqual(TEXT("Mission Damage Target defaults to 100 Health"), NativeTarget ? NativeTarget->GetMaximumHealth() : 0.0f, 100.0f);

	UClass* ManagerClass = LoadClass<ADroneMissionDirector>(nullptr, ManagerClassPath);
	UClass* ControllerClass = LoadClass<ADroneMissionPlayerController>(nullptr, ControllerClassPath);
	UClass* GameModeClass = LoadClass<ADroneMissionGameMode>(nullptr, GameModeClassPath);
	UClass* ObjectiveTriggerClass = LoadClass<ADroneMissionTrigger>(nullptr, ObjectiveTriggerClassPath);
	UClass* FailureTriggerClass = LoadClass<ADroneMissionTrigger>(nullptr, FailureTriggerClassPath);
	UClass* ReturnZoneClass = LoadClass<ADroneMissionReturnZone>(nullptr, ReturnZoneClassPath);
	UClass* DamageTargetClass = LoadClass<ADroneMissionDamageTarget>(nullptr, DamageTargetClassPath);
	UClass* HoverZoneClass = LoadClass<ADroneTutorialHoverZone>(nullptr, HoverZoneClassPath);
	UClass* HeadingZoneClass = LoadClass<ADroneTutorialHeadingZone>(nullptr, HeadingZoneClassPath);

	TestNotNull(TEXT("Blueprint Mission Manager loads"), ManagerClass);
	TestNotNull(TEXT("Blueprint Mission PlayerController loads"), ControllerClass);
	TestNotNull(TEXT("Blueprint Mission GameMode loads"), GameModeClass);
	TestNotNull(TEXT("Blueprint Objective Trigger loads"), ObjectiveTriggerClass);
	TestNotNull(TEXT("Blueprint Failure Trigger loads"), FailureTriggerClass);
	TestNotNull(TEXT("Blueprint Return Zone loads"), ReturnZoneClass);
	TestNotNull(TEXT("Blueprint Damage Target loads"), DamageTargetClass);
	TestNotNull(TEXT("Blueprint Tutorial Hover Zone loads"), HoverZoneClass);
	TestNotNull(TEXT("Blueprint Tutorial Heading Zone loads"), HeadingZoneClass);

	const ADroneMissionPlayerController* ControllerDefaults = ControllerClass
		? ControllerClass->GetDefaultObject<ADroneMissionPlayerController>()
		: nullptr;
	TestTrue(
		TEXT("Mission Controller spawns the project Blueprint Manager"),
		ControllerDefaults && ControllerDefaults->GetMissionDirectorClass().Get() == ManagerClass);

	const ADroneMissionGameMode* GameModeDefaults = GameModeClass
		? GameModeClass->GetDefaultObject<ADroneMissionGameMode>()
		: nullptr;
	TestTrue(
		TEXT("Mission GameMode uses the project Blueprint Controller"),
		GameModeDefaults && GameModeDefaults->PlayerControllerClass == ControllerClass);

	const ADroneMissionTrigger* ObjectiveDefaults = ObjectiveTriggerClass
		? ObjectiveTriggerClass->GetDefaultObject<ADroneMissionTrigger>()
		: nullptr;
	const ADroneMissionTrigger* FailureDefaults = FailureTriggerClass
		? FailureTriggerClass->GetDefaultObject<ADroneMissionTrigger>()
		: nullptr;
	TestTrue(
		TEXT("Objective Trigger reports an objective event"),
		ObjectiveDefaults
			&& ObjectiveDefaults->GetTriggerAction() == EDroneMissionTriggerAction::ReportObjectiveEvent);
	TestTrue(
		TEXT("Failure Trigger reports Mission failure"),
		FailureDefaults
			&& FailureDefaults->GetTriggerAction() == EDroneMissionTriggerAction::FailMission);

	const ADroneTutorialHoverZone* HoverDefaults = HoverZoneClass
		? HoverZoneClass->GetDefaultObject<ADroneTutorialHoverZone>()
		: nullptr;
	TestNotNull(TEXT("Tutorial Hover Zone owns its Box"), HoverDefaults ? HoverDefaults->GetHoverBox() : nullptr);
	TestEqual(TEXT("Tutorial Hover default duration is three seconds"), HoverDefaults ? HoverDefaults->GetRequiredHoldSeconds() : 0.0f, 3.0f);
	TestEqual(TEXT("Tutorial Hover default maximum speed is 75 cm/s"), HoverDefaults ? HoverDefaults->GetMaximumSpeedCentimetersPerSecond() : 0.0f, 75.0f);
	TestFalse(TEXT("Tutorial Hover Zone does not use Actor Tick"), HoverDefaults && HoverDefaults->PrimaryActorTick.bCanEverTick);

	const ADroneTutorialHeadingZone* HeadingDefaults = HeadingZoneClass
		? HeadingZoneClass->GetDefaultObject<ADroneTutorialHeadingZone>()
		: nullptr;
	TestNotNull(TEXT("Tutorial Heading Zone owns its Box"), HeadingDefaults ? HeadingDefaults->GetHeadingBox() : nullptr);
	TestEqual(TEXT("Tutorial Heading target is east"), HeadingDefaults ? HeadingDefaults->GetTargetHeadingDegrees() : 0.0f, 90.0f);
	TestEqual(TEXT("Tutorial Heading default tolerance is eight degrees"), HeadingDefaults ? HeadingDefaults->GetHeadingToleranceDegrees() : 0.0f, 8.0f);
	TestFalse(TEXT("Tutorial Heading Zone does not use Actor Tick"), HeadingDefaults && HeadingDefaults->PrimaryActorTick.bCanEverTick);

	return !HasAnyErrors();
}

#endif
