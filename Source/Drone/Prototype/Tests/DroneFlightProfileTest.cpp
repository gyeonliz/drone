#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Abilities/DroneImpactDetonationComponent.h"
#include "Abilities/DronePayloadDropComponent.h"
#include "Abilities/DroneReconScanComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneDefinition.h"
#include "Prototype/DronePrototypePawn.h"
#include "Signal/DroneSignalComponent.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFlightProfileTest,
	"Drone.Prototype.FlightProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneFlightProfileTest::RunTest(const FString& Parameters)
{
	const ADronePrototypePawn* BasePawnDefaults = GetDefault<ADronePrototypePawn>();
	TestNotNull(TEXT("Prototype Pawn defaults exist"), BasePawnDefaults);
	TestFalse(TEXT("Definition profile remains the default until a Drone BP explicitly enables its override"),
		BasePawnDefaults && BasePawnDefaults->UsesBlueprintFlightProfileOverride());

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
	TestEqual(TEXT("FPV Strike starts in Rate/Acro control"),
		FPVStrikeDefinition->FlightProfile.DefaultControlMode,
		EDroneControlMode::AcroRateRealisticGreybox);
	TestEqual(TEXT("FPV Strike uses the fast speed step"),
		FPVStrikeDefinition->FlightProfile.DefaultHandlingPreset,
		EDroneHandlingPreset::Agile);
	TestTrue(TEXT("FPV Strike uses the 650 deg/s pitch rate reference"), FMath::IsNearlyEqual(
		FPVStrikeDefinition->FlightProfile.AcroRateSettings.MaximumPitchRateDegreesPerSecond,
		650.0f));
	TestTrue(TEXT("FPV Strike uses the 650 deg/s roll rate reference"), FMath::IsNearlyEqual(
		FPVStrikeDefinition->FlightProfile.AcroRateSettings.MaximumRollRateDegreesPerSecond,
		650.0f));
	TestTrue(TEXT("FPV Strike uses the 400 deg/s yaw rate reference"), FMath::IsNearlyEqual(
		FPVStrikeDefinition->FlightProfile.AcroRateSettings.MaximumYawRateDegreesPerSecond,
		400.0f));
	TestTrue(TEXT("FPV Strike caps world vertical speed at 9 m/s"), FMath::IsNearlyEqual(
		FPVStrikeDefinition->FlightProfile.AcroRateSettings.MaximumWorldVerticalSpeedCentimetersPerSecond,
		900.0f));
	TestFalse(TEXT("Scout profile starts in third person"), ScoutDefinition->FlightProfile.bStartInFirstPersonView);
	TestFalse(TEXT("Drop profile starts in third person"), DropDefinition->FlightProfile.bStartInFirstPersonView);
	UClass* ScoutPawnClass = LoadClass<ADronePrototypePawn>(nullptr,
		TEXT("/Game/Drone/Integrations/RoleDrones/BP_DroneScoutIntegration.BP_DroneScoutIntegration_C"));
	UClass* FPVPawnClass = LoadClass<ADronePrototypePawn>(nullptr,
		TEXT("/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration.BP_DroneFPVIntegration_C"));
	UClass* DropPawnClass = LoadClass<ADronePrototypePawn>(nullptr,
		TEXT("/Game/Drone/Integrations/RoleDrones/BP_DroneDropIntegration.BP_DroneDropIntegration_C"));
	TestNotNull(TEXT("Scout integration Pawn class exists"), ScoutPawnClass);
	TestNotNull(TEXT("FPV integration Pawn class exists"), FPVPawnClass);
	TestNotNull(TEXT("Drop integration Pawn class exists"), DropPawnClass);
	if (!ScoutPawnClass || !FPVPawnClass || !DropPawnClass)
	{
		return false;
	}

	const TArray<UDroneDefinition*> Definitions = {ScoutDefinition, FPVStrikeDefinition, DropDefinition};
	const TMap<FName, UClass*> ExpectedPawnClasses = {
		{ScoutDefinition->DroneId, ScoutPawnClass},
		{FPVStrikeDefinition->DroneId, FPVPawnClass},
		{DropDefinition->DroneId, DropPawnClass}
	};
	for (UDroneDefinition* Definition : Definitions)
	{
		UClass* PawnClass = Definition->PawnClass.LoadSynchronous();
		TestTrue(
			*FString::Printf(TEXT("%s uses a Prototype Pawn subclass"), *Definition->DroneId.ToString()),
			PawnClass && PawnClass->IsChildOf(ADronePrototypePawn::StaticClass()));
		TestEqual(
			*FString::Printf(TEXT("%s uses its role-specific Drone Pack integration Pawn"), *Definition->DroneId.ToString()),
			PawnClass,
			ExpectedPawnClasses.FindRef(Definition->DroneId));
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
		if (Definition == FPVStrikeDefinition)
		{
			TestTrue(TEXT("FPV Agile runtime max speed reaches the 27 m/s reference"), FMath::IsNearlyEqual(
				Pawn->GetPrototypeMovementComponent()->MaxSpeed,
				2700.0f));
		}
		TestEqual(TEXT("Only the Definition's Recon feature state applies"),
			Pawn->GetReconScanComponent()->IsFeatureEnabled(),
			Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ReconScan));
		TestEqual(TEXT("Only the Definition's Impact feature state applies"),
			Pawn->GetImpactDetonationComponent()->IsFeatureEnabled(),
			Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ImpactDetonation));
		TestEqual(TEXT("Only the Definition's Drop feature state applies"),
			Pawn->GetPayloadDropComponent()->IsFeatureEnabled(),
			Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::PayloadDrop));
		TestEqual(TEXT("Only the Definition's implemented immunity applies"),
			Pawn->GetSignalComponent()->IsJammingImmune(),
			Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::JammingImmunity));

		// 두 축을 명시적으로 바꿔 기체 역할과 무관하게 같은 API로 동작하는지 확인한다.
		Pawn->SetControlMode(EDroneControlMode::AssistedEasy);
		Pawn->SetHandlingPreset(EDroneHandlingPreset::Balanced);
		TestTrue(TEXT("Normal speed uses the Definition base max speed"), FMath::IsNearlyEqual(
			Pawn->GetPrototypeMovementComponent()->MaxSpeed,
			Definition->FlightProfile.MaxSpeedCentimetersPerSecond));
		TestTrue(TEXT("Assisted Easy uses the Definition base acceleration"), FMath::IsNearlyEqual(
			Pawn->GetPrototypeMovementComponent()->Acceleration,
			Definition->FlightProfile.AccelerationCentimetersPerSecondSquared));
		TestTrue(TEXT("Normal speed uses the Definition base yaw rate"), FMath::IsNearlyEqual(
			Pawn->GetPrototypeYawRateDegreesPerSecond(),
			Definition->FlightProfile.YawRateDegreesPerSecond));

		Pawn->SetHandlingPreset(EDroneHandlingPreset::Stable);
		const float StableSpeed = Pawn->GetPrototypeMovementComponent()->MaxSpeed;
		const float SlowAcceleration = Pawn->GetPrototypeMovementComponent()->Acceleration;
		const float SlowYawRate = Pawn->GetPrototypeYawRateDegreesPerSecond();
		Pawn->SetHandlingPreset(EDroneHandlingPreset::Agile);
		const float AgileSpeed = Pawn->GetPrototypeMovementComponent()->MaxSpeed;
		TestTrue(TEXT("Slow is slower than Normal"),
			StableSpeed < Definition->FlightProfile.MaxSpeedCentimetersPerSecond);
		TestTrue(TEXT("Fast is faster than Normal"),
			AgileSpeed > Definition->FlightProfile.MaxSpeedCentimetersPerSecond);
		TestTrue(TEXT("Speed steps do not secretly alter acceleration"), FMath::IsNearlyEqual(
			SlowAcceleration,
			Pawn->GetPrototypeMovementComponent()->Acceleration));
		TestTrue(TEXT("Speed steps do not secretly alter yaw rate"), FMath::IsNearlyEqual(
			SlowYawRate,
			Pawn->GetPrototypeYawRateDegreesPerSecond()));

		Pawn->SetHandlingPreset(EDroneHandlingPreset::Balanced);
		const float AssistedDeceleration = Pawn->GetPrototypeMovementComponent()->Deceleration;
		Pawn->SetControlMode(EDroneControlMode::ManualRealisticGreybox);
		TestEqual(TEXT("Manual mode can be selected"), Pawn->GetControlMode(), EDroneControlMode::ManualRealisticGreybox);
		TestTrue(TEXT("Manual mode preserves more inertia than Assisted Easy"),
			Pawn->GetPrototypeMovementComponent()->Deceleration < AssistedDeceleration);
		Pawn->ToggleControlMode();
		TestEqual(TEXT("Control mode 3 is Rate/Acro transmitter Mode 1"),
			Pawn->GetControlMode(),
			EDroneControlMode::AcroRateMode1Greybox);
		Pawn->ToggleControlMode();
		TestEqual(TEXT("Control mode 4 is Rate/Acro transmitter Mode 2"),
			Pawn->GetControlMode(),
			EDroneControlMode::AcroRateRealisticGreybox);
		Pawn->ToggleControlMode();
		TestEqual(TEXT("Control mode cycles from Mode 2 to Assisted Easy"),
			Pawn->GetControlMode(),
			EDroneControlMode::AssistedEasy);

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
			Pawn->SetVisualTiltInputGreybox(1.0f, 1.0f);
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

			// Rate/Acro는 Stick 끝에서 Profile 최대 각속도를 내고 Stick을 놓으면 자동 수평 복귀하지 않는다.
			Pawn->SetActorRotation(FRotator::ZeroRotator);
			Pawn->SetControlMode(EDroneControlMode::AcroRateRealisticGreybox);
			Pawn->SetAcroRateInputGreybox(0.0f, 0.0f, 0.0f);
			Pawn->SetAcroThrottleInputGreybox(0.0f);
			Pawn->GetPrototypeMovementComponent()->Velocity = FVector::ZeroVector;
			Pawn->Tick(0.1f);
			TestTrue(TEXT("Centered Acro throttle balances gravity while level"),
				FMath::Abs(Pawn->GetPrototypeMovementComponent()->Velocity.Z) < 0.1f);

			Pawn->SetAcroThrottleInputGreybox(1.0f);
			Pawn->GetPrototypeMovementComponent()->Velocity = FVector::ZeroVector;
			Pawn->Tick(0.1f);
			TestTrue(TEXT("Positive Acro throttle climbs"),
				Pawn->GetPrototypeMovementComponent()->Velocity.Z > 1.0f);
			Pawn->SetAcroThrottleInputGreybox(-1.0f);
			Pawn->GetPrototypeMovementComponent()->Velocity = FVector::ZeroVector;
			Pawn->Tick(0.1f);
			TestTrue(TEXT("Negative Acro throttle removes thrust and descends"),
				Pawn->GetPrototypeMovementComponent()->Velocity.Z < -1.0f);

			Pawn->SetAcroThrottleInputGreybox(0.0f);
			Pawn->GetPrototypeMovementComponent()->Velocity = FVector::ZeroVector;
			Pawn->SetAcroRateInputGreybox(0.35f, 0.0f, 0.0f);
			for (int32 Step = 0; Step < 20; ++Step)
			{
				Pawn->Tick(0.01f);
			}
			TestTrue(TEXT("Nose-down Acro attitude creates forward thrust"),
				FVector::DotProduct(
					Pawn->GetPrototypeMovementComponent()->Velocity,
					FVector::ForwardVector) > 1.0f);

			Pawn->SetActorRotation(FRotator::ZeroRotator);
			Pawn->GetPrototypeMovementComponent()->Velocity = FVector::ZeroVector;
			Pawn->SetAcroRateInputGreybox(1.0f, 1.0f, 1.0f);
			const FRotator FullStickRates = Pawn->GetCurrentAcroBodyRateSetpointDegreesPerSecond();
			TestTrue(TEXT("Full forward stick commands maximum nose-down pitch rate"), FMath::IsNearlyEqual(
				FullStickRates.Pitch,
				-Definition->FlightProfile.AcroRateSettings.MaximumPitchRateDegreesPerSecond));
			TestTrue(TEXT("Full lateral stick commands maximum roll rate"), FMath::IsNearlyEqual(
				FullStickRates.Roll,
				Definition->FlightProfile.AcroRateSettings.MaximumRollRateDegreesPerSecond));
			TestTrue(TEXT("Full yaw stick commands maximum yaw rate"), FMath::IsNearlyEqual(
				FullStickRates.Yaw,
				Definition->FlightProfile.AcroRateSettings.MaximumYawRateDegreesPerSecond));
			Pawn->Tick(0.1f);
			Pawn->SetAcroRateInputGreybox(0.0f, 0.0f, 0.0f);
			for (int32 Step = 0; Step < 100; ++Step)
			{
				Pawn->Tick(0.01f);
			}
			const FQuat RotationAfterRateSettled = Pawn->GetActorQuat();
			Pawn->Tick(0.01f);
			TestTrue(TEXT("Centered Rate/Acro sticks stop body rate without auto-leveling"),
				Pawn->GetCurrentAcroBodyRateDegreesPerSecond().IsNearlyZero(0.01f)
				&& FQuat::ErrorAutoNormalize(RotationAfterRateSettled, Pawn->GetActorQuat()) < 0.001f
				&& FQuat::ErrorAutoNormalize(FQuat::Identity, Pawn->GetActorQuat()) > 0.01f);
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
