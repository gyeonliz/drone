#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Health/DroneHealthComponent.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePrototypeDamageShakeTest,
	"Drone.Prototype.DamageShake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDronePrototypeDamageShakeTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Damage shake test World exists"), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePrototypePawn* Pawn = World->SpawnActor<ADronePrototypePawn>(
		ADronePrototypePawn::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	TestNotNull(TEXT("Prototype Pawn spawns"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	UDroneHealthComponent* Health = Pawn->GetHealthComponent();
	TestNotNull(TEXT("Prototype Pawn owns Health"), Health);
	if (!Health)
	{
		return false;
	}

	FTransform InitialCameraOffset = FTransform::Identity;
	float InitialCameraFOVOffset = 0.0f;
	Pawn->GetFollowCamera()->GetAdditiveOffset(InitialCameraOffset, InitialCameraFOVOffset);
	const FVector InitialActorLocation = Pawn->GetActorLocation();
	const FRotator InitialCollisionRotation = Pawn->GetCollisionComponent()->GetRelativeRotation();

	TestTrue(TEXT("Shared Health Damage is accepted"), Health->ApplyHealthDamage(10.0f, nullptr, nullptr));
	TestEqual(TEXT("One Health hit starts one shake"), Pawn->GetDamageShakeEventCount(), 1);
	TestTrue(TEXT("Damage shake becomes active"), Pawn->IsDamageShakeActive());
	Pawn->Tick(0.02f);

	FTransform ActiveCameraOffset = FTransform::Identity;
	float ActiveCameraFOVOffset = 0.0f;
	Pawn->GetFollowCamera()->GetAdditiveOffset(ActiveCameraOffset, ActiveCameraFOVOffset);
	TestTrue(TEXT("Damage shake drives a non-zero visual response"),
		!Pawn->GetVisualTiltPivot()->GetRelativeRotation().IsNearlyZero(0.01f));
	TestTrue(TEXT("Damage shake drives a non-zero camera view offset"),
		!ActiveCameraOffset.Equals(InitialCameraOffset, 0.01f));
	TestTrue(TEXT("Damage shake never moves the collision Actor"),
		Pawn->GetActorLocation().Equals(InitialActorLocation, 0.01f));
	TestTrue(TEXT("Damage shake never rotates Collision"),
		Pawn->GetCollisionComponent()->GetRelativeRotation().Equals(InitialCollisionRotation, 0.01f));

	Pawn->Tick(1.0f);
	FTransform RestoredCameraOffset = FTransform::Identity;
	float RestoredCameraFOVOffset = 0.0f;
	Pawn->GetFollowCamera()->GetAdditiveOffset(RestoredCameraOffset, RestoredCameraFOVOffset);
	TestFalse(TEXT("Damage shake finishes after its short duration"), Pawn->IsDamageShakeActive());
	TestTrue(TEXT("Camera additive offset is restored after shake"),
		RestoredCameraOffset.Equals(InitialCameraOffset, 0.01f));
	TestEqual(TEXT("Camera FOV additive offset is restored after shake"),
		RestoredCameraFOVOffset, InitialCameraFOVOffset);
	TestTrue(TEXT("Visual tilt returns to level when no bank input remains"),
		Pawn->GetVisualTiltPivot()->GetRelativeRotation().IsNearlyZero(0.01f));

	return !HasAnyErrors();
}

#endif
