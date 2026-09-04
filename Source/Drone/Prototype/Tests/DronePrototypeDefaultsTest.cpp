#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Prototype/DronePrototypeGameMode.h"
#include "Prototype/DronePrototypePawn.h"
#include "Prototype/DronePrototypePlayerController.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Telemetry/DroneTelemetryComponent.h"
#include "UI/DroneFlightHUDWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePrototypeDefaultsTest,
	"Drone.Prototype.PawnDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDronePrototypeDefaultsTest::RunTest(const FString& Parameters)
{
	// 첫 묶음은 native Class들이 자체 기본값만으로도 생성 가능한지 확인한다.
	TestFalse(TEXT("Prototype Pawn class is concrete"), ADronePrototypePawn::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("Prototype GameMode class is concrete"), ADronePrototypeGameMode::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("Prototype PlayerController class is concrete"), ADronePrototypePlayerController::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("Flight HUD Widget class is concrete"), UDroneFlightHUDWidget::StaticClass()->HasAnyClassFlags(CLASS_Abstract));

	// Pawn CDO에서 Component 소유권과 입력/이동 기본 구조를 검증한다.
	const ADronePrototypePawn* PawnDefaults = GetDefault<ADronePrototypePawn>();
	TestNotNull(TEXT("Prototype Pawn CDO exists"), PawnDefaults);

	if (PawnDefaults)
	{
		TestNotNull(TEXT("Collision component exists"), PawnDefaults->GetCollisionComponent());
		TestNotNull(TEXT("Visual tilt pivot exists"), PawnDefaults->GetVisualTiltPivot());
		TestNotNull(TEXT("Visual mesh component exists"), PawnDefaults->GetVisualMeshComponent());
		TestNotNull(TEXT("Camera boom exists"), PawnDefaults->GetCameraBoom());
		TestNotNull(TEXT("Follow camera exists"), PawnDefaults->GetFollowCamera());
		TestNotNull(TEXT("Floating Pawn Movement exists"), PawnDefaults->GetPrototypeMovementComponent());
		TestNotNull(TEXT("Telemetry component exists"), PawnDefaults->GetTelemetryComponent());
		TestTrue(TEXT("Collision component is the root"), PawnDefaults->GetRootComponent() == PawnDefaults->GetCollisionComponent());
		TestTrue(TEXT("Prototype Pawn ticks only for visual banking"), PawnDefaults->PrimaryActorTick.bCanEverTick);
		TestTrue(TEXT("Pawn exposes the prototype movement component"), PawnDefaults->GetMovementComponent() == PawnDefaults->GetPrototypeMovementComponent());

		if (PawnDefaults->GetCollisionComponent())
		{
			TestEqual(TEXT("Prototype collision radius"), PawnDefaults->GetCollisionComponent()->GetUnscaledSphereRadius(), 45.0f);
			TestEqual(TEXT("Prototype collision profile"), PawnDefaults->GetCollisionComponent()->GetCollisionProfileName(), FName(TEXT("Pawn")));
			TestFalse(TEXT("Prototype collision physics simulation is disabled"), PawnDefaults->GetCollisionComponent()->IsSimulatingPhysics());
		}

		if (PawnDefaults->GetVisualMeshComponent())
		{
			TestTrue(TEXT("Visual mesh follows the visual tilt pivot"),
				PawnDefaults->GetVisualMeshComponent()->GetAttachParent() == PawnDefaults->GetVisualTiltPivot());
			TestTrue(
				TEXT("Prototype visual mesh collision is disabled"),
				PawnDefaults->GetVisualMeshComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Prototype visual mesh physics simulation is disabled"), PawnDefaults->GetVisualMeshComponent()->IsSimulatingPhysics());
		}
		if (PawnDefaults->GetVisualTiltPivot())
		{
			TestTrue(TEXT("Visual tilt pivot follows the collision root"),
				PawnDefaults->GetVisualTiltPivot()->GetAttachParent() == PawnDefaults->GetCollisionComponent());
			TestTrue(TEXT("Visual bank default maximum is positive"), PawnDefaults->GetMaximumVisualBankRollDegrees() > 0.0f);
			TestTrue(TEXT("Visual pitch default maximum is positive"), PawnDefaults->GetMaximumVisualTiltPitchDegrees() > 0.0f);
		}

		if (PawnDefaults->GetCameraBoom())
		{
			TestTrue(
				TEXT("Camera boom is attached to the collision root"),
				PawnDefaults->GetCameraBoom()->GetAttachParent() == PawnDefaults->GetCollisionComponent());
			TestTrue(TEXT("Prototype Pawn defaults to third-person view"), !PawnDefaults->IsFirstPersonViewEnabled());
			TestFalse(TEXT("Camera boom ignores controller rotation"), PawnDefaults->GetCameraBoom()->bUsePawnControlRotation);
			TestTrue(TEXT("Camera boom inherits Drone yaw"), PawnDefaults->GetCameraBoom()->bInheritYaw);
		}

		if (PawnDefaults->GetFollowCamera())
		{
			TestTrue(
				TEXT("Follow camera is attached to the camera boom"),
				PawnDefaults->GetFollowCamera()->GetAttachParent() == PawnDefaults->GetCameraBoom());
			TestFalse(TEXT("Follow camera does not add controller rotation twice"), PawnDefaults->GetFollowCamera()->bUsePawnControlRotation);
		}

		if (PawnDefaults->GetPrototypeMovementComponent())
		{
			TestTrue(
				TEXT("Movement component updates the collision root"),
				PawnDefaults->GetPrototypeMovementComponent()->UpdatedComponent == PawnDefaults->GetCollisionComponent());
			TestTrue(TEXT("Prototype max speed is positive"), PawnDefaults->GetPrototypeMovementComponent()->MaxSpeed > 0.0f);
		}

		if (PawnDefaults->GetTelemetryComponent())
		{
			TestFalse(
				TEXT("Telemetry component does not tick every frame"),
				PawnDefaults->GetTelemetryComponent()->PrimaryComponentTick.bCanEverTick);
			TestEqual(
				TEXT("Telemetry component updates at 10 Hz"),
				PawnDefaults->GetTelemetryComponent()->GetUpdateIntervalSeconds(),
				0.1f);
		}
	}

	// native GameMode와 Controller가 직접 선택됐을 때 사용할 기본 Class를 검증한다.
	const ADronePrototypeGameMode* GameModeDefaults = GetDefault<ADronePrototypeGameMode>();
	TestNotNull(TEXT("Prototype GameMode CDO exists"), GameModeDefaults);
	if (GameModeDefaults)
	{
		TestTrue(
			TEXT("Prototype GameMode spawns the Prototype Pawn"),
			GameModeDefaults->DefaultPawnClass == ADronePrototypePawn::StaticClass());
		TestTrue(
			TEXT("Prototype GameMode uses the Prototype PlayerController"),
			GameModeDefaults->PlayerControllerClass == ADronePrototypePlayerController::StaticClass());
	}

	const ADronePrototypePlayerController* PlayerControllerDefaults =
		GetDefault<ADronePrototypePlayerController>();
	TestNotNull(TEXT("Prototype PlayerController CDO exists"), PlayerControllerDefaults);
	if (PlayerControllerDefaults)
	{
		TestTrue(
			TEXT("Prototype PlayerController uses the native Flight HUD fallback"),
			PlayerControllerDefaults->GetFlightHUDWidgetClass() == UDroneFlightHUDWidget::StaticClass());
	}

	return true;
}

#endif
