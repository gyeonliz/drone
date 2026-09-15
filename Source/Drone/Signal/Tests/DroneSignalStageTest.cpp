#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Prototype/DronePrototypePawn.h"
#include "Signal/DroneJammingVolume.h"
#include "Signal/DroneSignalComponent.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSignalStageTest,
	"Drone.Signal.StageContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneSignalStageTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	TestNotNull(TEXT("Signal test Editor World exists"), World);
	if (!World)
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>(Params);
	AActor* WeakSource = World->SpawnActor<AActor>(Params);
	AActor* StrongSource = World->SpawnActor<AActor>(Params);
	ADroneJammingVolume* Zone = World->SpawnActor<ADroneJammingVolume>(Params);
	UDroneSignalComponent* Signal = Drone ? Drone->GetSignalComponent() : nullptr;
	TestNotNull(TEXT("Every prototype Drone owns a Signal Component"), Signal);
	TestTrue(
		TEXT("Jamming Volume provides an editable Pawn overlap Box without Tick"),
		Zone && Zone->GetJammingBounds()
			&& Zone->GetJammingBounds()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap
			&& !Zone->PrimaryActorTick.bCanEverTick);
	if (Signal && WeakSource && StrongSource && Zone)
	{
		TestEqual(TEXT("Clear signal starts at 100%"), Signal->GetSnapshot().NormalizedSignalQuality, 1.0f);
		TestTrue(TEXT("Weak source is accepted"), Signal->SetJammingSource(WeakSource, 0.25f));
		TestEqual(TEXT("Weak stage is deterministic"), Signal->GetSnapshot().Stage, EDroneSignalInterferenceStage::Weak);
		TestTrue(TEXT("Moderate source update is accepted"), Signal->SetJammingSource(WeakSource, 0.60f));
		TestEqual(TEXT("Moderate stage is deterministic"), Signal->GetSnapshot().Stage, EDroneSignalInterferenceStage::Moderate);
		TestTrue(TEXT("Moderate stage exposes video warning intensity"), Signal->GetSnapshot().VideoNoiseIntensity > 0.0f);
		TestTrue(TEXT("Strong second source is accepted"), Signal->SetJammingSource(StrongSource, 0.90f));
		TestEqual(TEXT("Multiple sources use strongest stage"), Signal->GetSnapshot().Stage, EDroneSignalInterferenceStage::Strong);
		TestTrue(TEXT("Strong stage slows response without random input loss"), Signal->GetSnapshot().ControlResponseMultiplier < 1.0f);
		Signal->ConfigureJammingImmunity(true);
		TestTrue(TEXT("Fiber-optic capability can enable jamming immunity"), Signal->IsJammingImmune());
		TestEqual(TEXT("Immune Drone keeps Normal signal inside active sources"), Signal->GetSnapshot().Stage, EDroneSignalInterferenceStage::None);
		Signal->ConfigureJammingImmunity(false);
		TestEqual(TEXT("Disabling immunity immediately restores strongest active source"), Signal->GetSnapshot().Stage, EDroneSignalInterferenceStage::Strong);
		Signal->RemoveJammingSource(StrongSource);
		TestEqual(TEXT("Removing strongest source restores Moderate"), Signal->GetSnapshot().Stage, EDroneSignalInterferenceStage::Moderate);
		Signal->RemoveJammingSource(WeakSource);
		TestEqual(TEXT("Leaving all sources restores Normal"), Signal->GetSnapshot().Stage, EDroneSignalInterferenceStage::None);
		TestEqual(TEXT("Clear signal returns to 100%"), Signal->GetSnapshot().NormalizedSignalQuality, 1.0f);
		TestFalse(TEXT("NaN strength is rejected"), Signal->SetJammingSource(WeakSource, std::numeric_limits<float>::quiet_NaN()));
		TestTrue(TEXT("Jammer disables once"), Zone->DisableJammer());
		TestFalse(TEXT("Jammer cannot disable twice"), Zone->DisableJammer());
	}
	if (Zone) Zone->Destroy();
	if (StrongSource) StrongSource->Destroy();
	if (WeakSource) WeakSource->Destroy();
	if (Drone) Drone->Destroy();
	return !HasAnyErrors();
}

#endif
