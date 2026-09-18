#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AI/DroneNPCAIController.h"
#include "AI/DroneNPCCharacter.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneNPCEngagementMaintenanceTest,
	"Drone.AI.PersonalWeaponMaintenanceTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneNPCEngagementMaintenanceTest::RunTest(const FString& Parameters)
{
	// Transient editor world only: no existing map or asset is saved.
	AddExpectedError(TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains, 0);
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Transient world"), World))
	{
		return false;
	}
	constexpr float Step = 0.05f;
	World->Tick(LEVELTICK_TimeOnly, Step);
	TestTrue(TEXT("Maintenance sees a nonzero world delta"), World->GetDeltaSeconds() > 0.0f);
	ADroneNPCCharacter* NPC = World->SpawnActor<ADroneNPCCharacter>();
	ADroneNPCAIController* Controller = World->SpawnActor<ADroneNPCAIController>();
	ADroneNPCCharacter* Target = World->SpawnActor<ADroneNPCCharacter>();
	if (!NPC || !Controller || !Target)
	{
		AddError(TEXT("Failed to spawn timing fixtures"));
		return false;
	}
	FDroneNPCProfile Profile;
	Profile.Faction = EDroneNPCFaction::Hostile;
	Profile.WeaponType = EDroneNPCWeaponType::Shotgun;
	NPC->GetNPCProfileComponent()->SetProfile(Profile);
	NPC->GetNPCWeaponComponent()->ConfigureWeapon(EDroneNPCWeaponType::Shotgun);
	Controller->Possess(NPC);
	Target->SetActorLocation(NPC->GetActorLocation() + FVector(2200.0f, 0.0f, 0.0f));

	// Inject perception state without running live perception, navigation or the StateTree.
	FWeakObjectProperty* TargetProperty = FindFProperty<FWeakObjectProperty>(Controller->GetClass(), TEXT("DetectedDrone"));
	FFloatProperty* OutOfRange = FindFProperty<FFloatProperty>(Controller->GetClass(), TEXT("PersonalWeaponOutOfRangeElapsedSeconds"));
	FFloatProperty* NoProgress = FindFProperty<FFloatProperty>(Controller->GetClass(), TEXT("PersonalWeaponPursuitNoProgressSeconds"));
	FFloatProperty* Repath = FindFProperty<FFloatProperty>(Controller->GetClass(), TEXT("PersonalWeaponPursuitRepathRemainingSeconds"));
	FEnumProperty* State = FindFProperty<FEnumProperty>(Controller->GetClass(), TEXT("ResponseState"));
	if (!TargetProperty || !OutOfRange || !NoProgress || !Repath || !State)
	{
		AddError(TEXT("Timing fixture properties are missing"));
		return false;
	}
	TargetProperty->SetObjectPropertyValue_InContainer(Controller, Target);
	Controller->EnterDroneDetectedResponse();
	TestTrue(TEXT("Maintenance accepts a live personal-weapon target"), Controller->MaintainCurrentResponseStateAction());
	TestEqual(TEXT("Maintenance does not advance range confirmation"), OutOfRange->GetPropertyValue_InContainer(Controller), 0.0f);
	Controller->Tick(Step);
	TestTrue(TEXT("Maintenance plus Tick advances confirmation once"),
		FMath::IsNearlyEqual(OutOfRange->GetPropertyValue_InContainer(Controller), Step));
	TestTrue(TEXT("One short frame remains in the detected response"),
		Controller->GetResponseState() == EDroneNPCAIResponseState::DroneDetected);

	State->GetUnderlyingProperty()->SetIntPropertyValue(State->ContainerPtrToValuePtr<void>(Controller),
		static_cast<uint64>(EDroneNPCAIResponseState::PursueDrone));
	NoProgress->SetPropertyValue_InContainer(Controller, 0.5f);
	Repath->SetPropertyValue_InContainer(Controller, 1.0f);
	Controller->MaintainCurrentResponseStateAction();
	Controller->Tick(Step);
	TestTrue(TEXT("Pursuit no-progress time advances once"),
		FMath::IsNearlyEqual(NoProgress->GetPropertyValue_InContainer(Controller), 0.5f + Step));
	TestTrue(TEXT("Pursuit repath countdown advances once"),
		FMath::IsNearlyEqual(Repath->GetPropertyValue_InContainer(Controller), 1.0f - Step));
	Controller->UnPossess();
	Controller->Destroy();
	Target->Destroy();
	NPC->Destroy();
	return true;
}

#endif
