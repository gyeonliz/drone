#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Abilities/DroneImpactDetonationComponent.h"
#include "Abilities/DronePayloadDropComponent.h"
#include "Abilities/DroneReconScanComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneDefinition.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFlightProfileTest,
	"Drone.Prototype.FlightProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneFlightProfileTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Flight profile test World exists"), World);
	if (!World)
	{
		return false;
	}

	UDroneDefinition* ScoutDefinition = LoadObject<UDroneDefinition>(
		nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_Scout_Greybox.DA_Drone_Scout_Greybox"));
	UDroneDefinition* FPVStrikeDefinition = LoadObject<UDroneDefinition>(
		nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_FPVStrike_Greybox.DA_Drone_FPVStrike_Greybox"));
	UDroneDefinition* DropDefinition = LoadObject<UDroneDefinition>(
		nullptr,
		TEXT("/Game/Drone/Data/Drones/DA_Drone_Drop_Greybox.DA_Drone_Drop_Greybox"));
	TestNotNull(TEXT("Scout flight profile exists"), ScoutDefinition);
	TestNotNull(TEXT("FPV Strike flight profile exists"), FPVStrikeDefinition);
	TestNotNull(TEXT("Drop flight profile exists"), DropDefinition);
	if (!ScoutDefinition || !FPVStrikeDefinition || !DropDefinition)
	{
		return false;
	}

	TestEqual(TEXT("Scout is a reconnaissance role"), ScoutDefinition->MissionRole, EDroneMissionRole::Reconnaissance);
	TestEqual(TEXT("FPV Strike has its own role"), FPVStrikeDefinition->MissionRole, EDroneMissionRole::FPVStrike);
	TestEqual(TEXT("Drop has its own role"), DropDefinition->MissionRole, EDroneMissionRole::DropDelivery);
	TestTrue(TEXT("Scout plans Recon Scan"), ScoutDefinition->PlannedCapabilities.Contains(EDroneGameplayCapability::ReconScan));
	TestTrue(TEXT("FPV plans Impact Detonation"), FPVStrikeDefinition->PlannedCapabilities.Contains(EDroneGameplayCapability::ImpactDetonation));
	TestTrue(TEXT("Drop plans Payload Drop"), DropDefinition->PlannedCapabilities.Contains(EDroneGameplayCapability::PayloadDrop));
	TestTrue(TEXT("Scout marks Recon Scan implemented"), ScoutDefinition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ReconScan));
	TestTrue(TEXT("FPV marks Impact Detonation implemented"), FPVStrikeDefinition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ImpactDetonation));
	TestTrue(TEXT("Drop marks Payload Drop implemented"), DropDefinition->ImplementedCapabilities.Contains(EDroneGameplayCapability::PayloadDrop));
	TestTrue(TEXT("FPV Strike profile starts in FPV"), FPVStrikeDefinition->FlightProfile.bStartInFirstPersonView);
	TestFalse(TEXT("Scout profile starts in third person"), ScoutDefinition->FlightProfile.bStartInFirstPersonView);
	TestFalse(TEXT("Drop profile starts in third person"), DropDefinition->FlightProfile.bStartInFirstPersonView);

	const TArray<UDroneDefinition*> Definitions = {ScoutDefinition, FPVStrikeDefinition, DropDefinition};
	for (UDroneDefinition* Definition : Definitions)
	{
		UClass* PawnClass = Definition->PawnClass.LoadSynchronous();
		TestTrue(
			*FString::Printf(TEXT("%s uses a Prototype Pawn subclass"), *Definition->DroneId.ToString()),
			PawnClass && PawnClass->IsChildOf(ADronePrototypePawn::StaticClass()));
		if (!PawnClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADronePrototypePawn* Pawn = World->SpawnActor<ADronePrototypePawn>(
			PawnClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
		TestNotNull(
			*FString::Printf(TEXT("%s Pawn spawns"), *Definition->DroneId.ToString()),
			Pawn);
		if (!Pawn)
		{
			continue;
		}

		TestTrue(
			*FString::Printf(TEXT("%s profile applies"), *Definition->DroneId.ToString()),
			Pawn->ApplyDroneDefinition(Definition));
		TestEqual(TEXT("Applied Drone ID is preserved"), Pawn->GetAppliedDroneId(), Definition->DroneId);
		TestEqual(TEXT("Default control mode applies"), Pawn->GetControlMode(), Definition->FlightProfile.DefaultControlMode);
		TestEqual(TEXT("Default handling preset applies"), Pawn->GetHandlingPreset(), Definition->FlightProfile.DefaultHandlingPreset);
		TestEqual(TEXT("Only the Definition's Recon feature state applies"),
			Pawn->GetReconScanComponent()->IsFeatureEnabled(),
			Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ReconScan));
		TestEqual(TEXT("Only the Definition's Impact feature state applies"),
			Pawn->GetImpactDetonationComponent()->IsFeatureEnabled(),
			Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ImpactDetonation));
		TestEqual(TEXT("Only the Definition's Drop feature state applies"),
			Pawn->GetPayloadDropComponent()->IsFeatureEnabled(),
			Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::PayloadDrop));

		// 두 축을 명시적으로 바꿔 기체 역할과 무관하게 같은 API로 동작하는지 확인한다.
		Pawn->SetControlMode(EDroneControlMode::AssistedEasy);
		Pawn->SetHandlingPreset(EDroneHandlingPreset::Balanced);
		TestTrue(TEXT("Balanced preset uses the Definition base max speed"), FMath::IsNearlyEqual(
			Pawn->GetPrototypeMovementComponent()->MaxSpeed,
			Definition->FlightProfile.MaxSpeedCentimetersPerSecond));
		TestTrue(TEXT("Assisted Easy uses the Definition base acceleration"), FMath::IsNearlyEqual(
			Pawn->GetPrototypeMovementComponent()->Acceleration,
			Definition->FlightProfile.AccelerationCentimetersPerSecondSquared));
		TestTrue(TEXT("Balanced preset uses the Definition base yaw rate"), FMath::IsNearlyEqual(
			Pawn->GetPrototypeYawRateDegreesPerSecond(),
			Definition->FlightProfile.YawRateDegreesPerSecond));

		Pawn->SetHandlingPreset(EDroneHandlingPreset::Stable);
		const float StableSpeed = Pawn->GetPrototypeMovementComponent()->MaxSpeed;
		Pawn->SetHandlingPreset(EDroneHandlingPreset::Agile);
		const float AgileSpeed = Pawn->GetPrototypeMovementComponent()->MaxSpeed;
		TestTrue(TEXT("Stable is slower than Balanced"),
			StableSpeed < Definition->FlightProfile.MaxSpeedCentimetersPerSecond);
		TestTrue(TEXT("Agile is faster than Balanced"),
			AgileSpeed > Definition->FlightProfile.MaxSpeedCentimetersPerSecond);

		Pawn->SetHandlingPreset(EDroneHandlingPreset::Balanced);
		const float AssistedDeceleration = Pawn->GetPrototypeMovementComponent()->Deceleration;
		Pawn->SetControlMode(EDroneControlMode::ManualRealisticGreybox);
		TestEqual(TEXT("Manual mode can be selected"), Pawn->GetControlMode(), EDroneControlMode::ManualRealisticGreybox);
		TestTrue(TEXT("Manual mode preserves more inertia than Assisted Easy"),
			Pawn->GetPrototypeMovementComponent()->Deceleration < AssistedDeceleration);
		Pawn->ToggleControlMode();
		TestEqual(TEXT("Control mode toggles back to Assisted Easy"), Pawn->GetControlMode(), EDroneControlMode::AssistedEasy);

		if (Definition == ScoutDefinition)
		{
			// 쉬운 조작은 Root를 수평으로 두고 외형만 기울인다.
			Pawn->SetActorRotation(FRotator::ZeroRotator);
			Pawn->SetVisualTiltInputGreybox(1.0f, 1.0f);
			Pawn->Tick(0.1f);
			TestTrue(TEXT("Assisted Easy keeps the collision Root level"),
				FMath::IsNearlyZero(Pawn->GetActorRotation().Pitch)
				&& FMath::IsNearlyZero(Pawn->GetActorRotation().Roll));
			TestTrue(TEXT("Assisted Easy still tilts the visual presentation"),
				!FMath::IsNearlyZero(Pawn->GetCurrentVisualTiltPitchDegrees())
				&& !FMath::IsNearlyZero(Pawn->GetCurrentVisualBankRollDegrees()));

			// 실제 조작형은 같은 입력을 Root 자세에 반영하고 쉬운 조작 복귀 시 다시 수평화한다.
			Pawn->SetControlMode(EDroneControlMode::ManualRealisticGreybox);
			Pawn->Tick(0.1f);
			TestTrue(TEXT("Manual Greybox tilts the collision Root"),
				!FMath::IsNearlyZero(Pawn->GetActorRotation().Pitch)
				&& !FMath::IsNearlyZero(Pawn->GetActorRotation().Roll));
			Pawn->SetVisualTiltInputGreybox(0.0f, 0.0f);
			Pawn->SetControlMode(EDroneControlMode::AssistedEasy);
			for (int32 Step = 0; Step < 20; ++Step)
			{
				Pawn->Tick(0.1f);
			}
			TestTrue(TEXT("Returning to Assisted Easy levels the collision Root"),
				FMath::Abs(Pawn->GetActorRotation().Pitch) < 0.25f
				&& FMath::Abs(Pawn->GetActorRotation().Roll) < 0.25f);
		}
		TestTrue(
			TEXT("Maximum health matches the Definition"),
			FMath::IsNearlyEqual(
				Pawn->GetHealthComponent()->GetMaxHealth(),
				Definition->FlightProfile.MaxHealth));
		TestEqual(
			TEXT("Starting camera mode matches the Definition"),
			Pawn->IsFirstPersonViewEnabled(),
			Definition->FlightProfile.bStartInFirstPersonView);
	}

	return !HasAnyErrors();
}

#endif
