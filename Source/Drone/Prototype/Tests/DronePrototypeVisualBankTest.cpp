#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePrototypeVisualBankTest,
	"Drone.Prototype.VisualBank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDronePrototypeVisualBankTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Visual bank test World exists"), World);
	if (!World)
	{
		return false;
	}

	UClass* IntegrationClass = LoadClass<ADronePrototypePawn>(
		nullptr,
		TEXT("/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration.BP_DroneFPVIntegration_C"));
	TestNotNull(TEXT("FPV integration Class loads"), IntegrationClass);
	if (!IntegrationClass)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePrototypePawn* Pawn = World->SpawnActor<ADronePrototypePawn>(
		IntegrationClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	TestNotNull(TEXT("FPV integration Pawn spawns"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	int32 VisualMeshCount = 0;
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Pawn);
	for (const UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent || MeshComponent->GetClass()->GetName().Contains(TEXT("CameraProxyMeshComponent")))
		{
			continue;
		}
		++VisualMeshCount;
		TestTrue(
			*FString::Printf(TEXT("%s follows the visual tilt pivot"), *MeshComponent->GetName()),
			MeshComponent->GetAttachParent() == Pawn->GetVisualTiltPivot());
	}
	TestEqual(TEXT("FPV body and four rotors participate in visual bank"), VisualMeshCount, 5);
	TestFalse(TEXT("Drone starts in third-person view"), Pawn->IsFirstPersonViewEnabled());
	TestTrue(TEXT("Third-person CameraBoom follows the collision root"),
		Pawn->GetCameraBoom()->GetAttachParent() == Pawn->GetCollisionComponent());
	TestTrue(TEXT("Third-person CameraBoom uses a nonzero chase arm"), Pawn->GetCameraBoom()->TargetArmLength > 0.0f);

	Pawn->SetVisualTiltInputGreybox(0.0f, 1.0f);
	Pawn->Tick(1.0f);
	TestTrue(TEXT("Right movement input banks the visual to positive Roll"), Pawn->GetCurrentVisualBankRollDegrees() > 1.0f);
	TestTrue(TEXT("Pure right movement keeps visual Pitch level"), FMath::Abs(Pawn->GetCurrentVisualTiltPitchDegrees()) < 0.1f);
	TestTrue(TEXT("Collision root remains unbanked"), Pawn->GetCollisionComponent()->GetRelativeRotation().IsNearlyZero(0.01f));
	TestTrue(TEXT("Camera boom remains unbanked"), Pawn->GetCameraBoom()->GetRelativeRotation().Roll == 0.0f);

	Pawn->SetFirstPersonViewEnabled(true);
	TestTrue(TEXT("First-person view mode is enabled"), Pawn->IsFirstPersonViewEnabled());
	TestTrue(TEXT("First-person CameraBoom follows the visual tilt pivot"),
		Pawn->GetCameraBoom()->GetAttachParent() == Pawn->GetVisualTiltPivot());
	TestTrue(TEXT("First-person CameraBoom removes the chase arm"), FMath::IsNearlyZero(Pawn->GetCameraBoom()->TargetArmLength));
	Pawn->SetVisualTiltInputGreybox(1.0f, 1.0f);
	Pawn->Tick(1.0f);
	TestTrue(TEXT("First-person camera follows movement Pitch"), Pawn->GetCameraBoom()->GetComponentRotation().Pitch < -1.0f);
	TestTrue(TEXT("First-person camera follows movement Roll"), Pawn->GetCameraBoom()->GetComponentRotation().Roll > 1.0f);

	Pawn->ToggleFirstPersonView();
	TestFalse(TEXT("View toggle returns to third-person"), Pawn->IsFirstPersonViewEnabled());
	TestTrue(TEXT("Returning to third-person restores collision-root attachment"),
		Pawn->GetCameraBoom()->GetAttachParent() == Pawn->GetCollisionComponent());
	TestTrue(TEXT("Returning to third-person restores the chase arm"), Pawn->GetCameraBoom()->TargetArmLength > 0.0f);

	Pawn->SetVisualTiltInputGreybox(1.0f, 0.0f);
	Pawn->Tick(1.0f);
	TestTrue(TEXT("Forward movement pitches the visual nose down"), Pawn->GetCurrentVisualTiltPitchDegrees() < -1.0f);
	TestTrue(TEXT("Pure forward movement returns visual Roll to level"), FMath::Abs(Pawn->GetCurrentVisualBankRollDegrees()) < 0.1f);
	TestTrue(TEXT("Collision root remains untilted during forward movement"), Pawn->GetCollisionComponent()->GetRelativeRotation().IsNearlyZero(0.01f));
	TestTrue(TEXT("Camera boom remains unpitched by movement tilt"), Pawn->GetCameraBoom()->GetRelativeRotation().Pitch == 0.0f);

	Pawn->SetVisualTiltInputGreybox(-1.0f, 0.0f);
	Pawn->Tick(1.0f);
	TestTrue(TEXT("Backward movement pitches the visual nose up"), Pawn->GetCurrentVisualTiltPitchDegrees() > 1.0f);

	Pawn->SetVisualTiltInputGreybox(0.0f, 0.0f);
	Pawn->Tick(1.0f);
	TestTrue(TEXT("Visual Roll returns toward level after release"),
		FMath::Abs(Pawn->GetCurrentVisualBankRollDegrees()) < 0.1f);
	TestTrue(TEXT("Visual Pitch returns toward level after release"),
		FMath::Abs(Pawn->GetCurrentVisualTiltPitchDegrees()) < 0.1f);
	return !HasAnyErrors();
}

#endif
