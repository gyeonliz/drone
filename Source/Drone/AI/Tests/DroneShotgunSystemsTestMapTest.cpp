#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/DroneNPCAIController.h"
#include "AI/DroneNPCCharacter.h"
#include "AI/DroneNPCNavigationFloor.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/Weapons/DroneNPCProjectile.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"

namespace DroneShotgunSystemsTestMap
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneShotgunSystemsTest");
constexpr const TCHAR* MapObjectPath =
	TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneShotgunSystemsTest.Lvl_DroneShotgunSystemsTest");
constexpr const TCHAR* ShotgunNPCClassPath =
	TEXT("/Game/Drone/AI/Blueprints/BP_NPC_Hostile_Shotgun.BP_NPC_Hostile_Shotgun_C");
constexpr const TCHAR* GameModeClassPath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
constexpr const TCHAR* ShotgunPelletProjectileClassPath =
	TEXT("/Game/Drone/AI/Blueprints/Projectiles/BP_ShotgunPelletProjectile.BP_ShotgunPelletProjectile_C");
const FName ShooterTag(TEXT("DroneShotgunSystemsTest.Shooter"));

UWorld* FindPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			return Context.World();
		}
	}
	return nullptr;
}

FRequestPlaySessionParams MakePlayParams()
{
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(1);
	Settings->bLaunchSeparateServer = false;
	Settings->AddToRoot();

	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.EditorPlaySettings = Settings;
	Params.bAllowOnlineSubsystem = false;
	return Params;
}

class FValidateShotgunFirePIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateShotgunFirePIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		if (!PIEWorld || !PIEWorld->HasBegunPlay())
		{
			return FinishOnTimeout(Now, TEXT("Shotgun systems PIE World did not begin play"));
		}

		ADroneNPCCharacter* ShotgunNPC = nullptr;
		int32 ShotgunNPCCount = 0;
		for (TActorIterator<ADroneNPCCharacter> It(PIEWorld); It; ++It)
		{
			if (It->ActorHasTag(ShooterTag))
			{
				ShotgunNPC = *It;
				++ShotgunNPCCount;
			}
		}

		ADronePrototypePawn* Drone = nullptr;
		for (TActorIterator<ADronePrototypePawn> It(PIEWorld); It; ++It)
		{
			Drone = *It;
			break;
		}

		ADroneNPCAIController* Controller = ShotgunNPC
			? Cast<ADroneNPCAIController>(ShotgunNPC->GetController())
			: nullptr;
		UDroneNPCWeaponComponent* Weapon = ShotgunNPC
			? ShotgunNPC->GetNPCWeaponComponent()
			: nullptr;
		LastNPC = ShotgunNPC;
		LastController = Controller;
		LastWeapon = Weapon;
		const bool bVolleyObserved = Weapon
			&& Weapon->GetShotgunVolleyAttemptCount() >= 1
			&& Weapon->GetShotgunProjectileSpawnCount() >= Weapon->GetShotgunPelletCount()
			&& Weapon->GetWeaponFiredEventCount() >= 1;
		if (Controller && Controller->HasDetectedDrone() && DetectionObservedAt == 0.0)
		{
			DetectionObservedAt = Now;
			Test->TestEqual(
				TEXT("Shotgun first-shot aim delay defaults to one second"),
				Controller->GetPersonalWeaponInitialAimDelaySeconds(),
				1.0f);
		}
		if (bVolleyObserved && Controller && !bInitialAimDelayVerified)
		{
			const float AimElapsed = Controller->GetPersonalWeaponInitialAimElapsedSeconds();
			Test->TestTrue(
				*FString::Printf(
					TEXT("First Shotgun Volley waits for the initial aim delay (elapsed %.3fs, required %.3fs)"),
					AimElapsed,
					Controller->GetPersonalWeaponInitialAimDelaySeconds()),
				AimElapsed + 0.02f >= Controller->GetPersonalWeaponInitialAimDelaySeconds());
			bInitialAimDelayVerified = true;
		}
		if ((!ShotgunNPC || !Drone || !Controller || !Weapon || !bVolleyObserved)
			&& Now - StartedAt <= 15.0)
		{
			return false;
		}
		if (bVolleyObserved && VolleyObservedAt == 0.0)
		{
			VolleyObservedAt = Now;
		}
		if (bVolleyObserved && Now - VolleyObservedAt < 0.08)
		{
			return false;
		}
		int32 LivePelletCount = 0;
		for (TActorIterator<ADroneNPCProjectile> It(PIEWorld); It; ++It)
		{
			LivePelletCount += It->GetOwner() == ShotgunNPC
				&& It->GetProjectileSource() == EDroneNPCProjectileSource::Shotgun ? 1 : 0;
		}
		Test->TestEqual(TEXT("All eight separately flying pellets remain alive after 80 ms"),
			LivePelletCount,
			Weapon ? Weapon->GetShotgunPelletCount() : 8);

		Test->TestEqual(TEXT("Shotgun systems PIE has exactly one dedicated Shotgun NPC"), ShotgunNPCCount, 1);
		Test->TestNotNull(TEXT("Shotgun systems PIE spawns the playable Drone"), Drone);
		Test->TestNotNull(TEXT("Shotgun NPC is possessed by the project AI Controller"), Controller);
		Test->TestNotNull(TEXT("Shotgun NPC owns the common Weapon Component"), Weapon);
		if (!ShotgunNPC || !Drone || !Controller || !Weapon)
		{
			return FinishOnTimeout(Now, TEXT("Shotgun systems PIE actors or components are missing"));
		}

		const UDroneNPCProfileComponent* Profile = ShotgunNPC->GetNPCProfileComponent();
		Test->TestNotNull(TEXT("Shotgun NPC owns its Profile Component"), Profile);
		Test->TestTrue(TEXT("Dedicated NPC profile is Hostile"), Profile && Profile->IsHostile());
		Test->TestTrue(
			TEXT("Dedicated NPC profile selects Shotgun"),
			Profile && Profile->GetProfile().WeaponType == EDroneNPCWeaponType::Shotgun);
		Test->TestTrue(TEXT("Controller reports the Shotgun role"), Controller->UsesShotgun());
		Test->TestFalse(TEXT("Shotgun NPC is not an MG operator"), Controller->CanUseMGTurret());
		Test->TestEqual(
			TEXT("Shotgun profile exposes a three-degree personal-weapon facing dead zone"),
			Profile ? Profile->GetProfile().PersonalWeaponFacingDeadZoneDegrees : -1.0f,
			3.0f);
		Test->TestEqual(
			TEXT("Shotgun profile exposes the personal-weapon facing turn speed"),
			Profile ? Profile->GetProfile().PersonalWeaponFacingTurnSpeedDegreesPerSecond : -1.0f,
			180.0f);
		Test->TestTrue(TEXT("Shotgun uses moving Projectile ballistics"), Weapon->UsesProjectileBallistics());
		Test->TestEqual(TEXT("Shotgun test keeps eight Pellets per Volley"), Weapon->GetShotgunPelletCount(), 8);
		Test->TestEqual(TEXT("Shotgun test keeps a twelve-degree cone half-angle"), Weapon->GetShotgunSpreadHalfAngleDegrees(), 12.0f);
		Test->TestEqual(TEXT("Shotgun test keeps 3500 cm/s projectile speed"), Weapon->GetShotgunProjectileSpeed(), 3500.0f);
		Test->TestEqual(TEXT("Shotgun test keeps an eight-shell magazine"), Weapon->GetMagazineCapacity(), 8);
		Test->TestTrue(TEXT("PIE observes at least one Shotgun Volley"), Weapon->GetShotgunVolleyAttemptCount() >= 1);
		Test->TestTrue(
			TEXT("One observed Volley spawns at least eight Pellet projectiles"),
			Weapon->GetShotgunProjectileSpawnCount() >= Weapon->GetShotgunPelletCount());
		Test->TestTrue(TEXT("Shotgun Volley consumes at least one shell"), Weapon->GetCurrentMagazineAmmo() < Weapon->GetMagazineCapacity());
		Test->TestTrue(TEXT("Shotgun Volley emits its common fire event"), Weapon->GetWeaponFiredEventCount() >= 1);
		Test->TestFalse(TEXT("Production shotgun hides cyan debug rays by default"), Weapon->IsShotgunDebugTraceEnabled());

		const TArray<FVector>& PelletEnds = Weapon->GetLastShotgunPelletTraceEnds();
		const TArray<FVector> BlueprintPelletEnds = Weapon->GetLastShotgunPelletEndpoints();
		Test->TestEqual(TEXT("Last Volley records all eight Pellet endpoints"), PelletEnds.Num(), 8);
		Test->TestEqual(TEXT("Blueprint endpoint getter exposes the same Pellet count"), BlueprintPelletEnds.Num(), PelletEnds.Num());

		FVector EyeLocation = FVector::ZeroVector;
		FRotator EyeRotation = FRotator::ZeroRotator;
		ShotgunNPC->GetActorEyesViewPoint(EyeLocation, EyeRotation);
		const FVector CenterDirection = (Weapon->GetCurrentAimPoint() - EyeLocation).GetSafeNormal();
		bool bEveryPelletInsideCone = !CenterDirection.IsNearlyZero() && PelletEnds.Num() == 8;
		bool bFoundSeparatedPellet = false;
		for (int32 Index = 0; Index < PelletEnds.Num(); ++Index)
		{
			const FVector PelletDirection = (PelletEnds[Index] - EyeLocation).GetSafeNormal();
			const float PelletAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
				FVector::DotProduct(CenterDirection, PelletDirection),
				-1.0f,
				1.0f)));
			bEveryPelletInsideCone &= PelletAngleDegrees <= 12.01f;
			if (Index > 0)
			{
				bFoundSeparatedPellet |= !PelletEnds[Index].Equals(PelletEnds[0], 1.0f);
			}
		}
		Test->TestTrue(TEXT("Every Projectile direction stays inside the twelve-degree cone"), bEveryPelletInsideCone);
		Test->TestTrue(TEXT("Shotgun Pellets use independently randomized directions"), bFoundSeparatedPellet);
		return true;
	}

private:
	bool FinishOnTimeout(const double Now, const TCHAR* Reason)
	{
		if (Now - StartedAt <= 15.0)
		{
			return false;
		}
		Test->AddError(FString::Printf(TEXT(
			"%s after 15 seconds (NPC=%s Controller=%s Weapon=%s Volleys=%d Projectiles=%d FireEvents=%d)"),
			Reason,
			*GetNameSafe(LastNPC.Get()),
			*GetNameSafe(LastController.Get()),
			*GetNameSafe(LastWeapon.Get()),
			LastWeapon.IsValid() ? LastWeapon->GetShotgunVolleyAttemptCount() : -1,
			LastWeapon.IsValid() ? LastWeapon->GetShotgunProjectileSpawnCount() : -1,
			LastWeapon.IsValid() ? LastWeapon->GetWeaponFiredEventCount() : -1));
		return true;
	}

	FAutomationTestBase* Test;
	double StartedAt = 0.0;
	double DetectionObservedAt = 0.0;
	double VolleyObservedAt = 0.0;
	bool bInitialAimDelayVerified = false;
	TWeakObjectPtr<ADroneNPCCharacter> LastNPC;
	TWeakObjectPtr<ADroneNPCAIController> LastController;
	TWeakObjectPtr<UDroneNPCWeaponComponent> LastWeapon;
};

/**
 * 개인화기 NPC가 이미 표적 정면을 본 상태에서 작은 좌우 위치 변화까지 몸으로 따라가면
 * 상체/고개가 화면상 계속 도리도리하는 것처럼 보인다. 조준선 안의 미세 변화는 몸 회전으로
 * 넘기지 않는 안정화 계약을 실제 Shotgun NPC와 Controller Tick 경로에서 검증한다.
 */
class FValidatePersonalWeaponFacingStabilityPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidatePersonalWeaponFacingStabilityPIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		if (!PIEWorld || !PIEWorld->HasBegunPlay())
		{
			return FinishOnTimeout(Now, TEXT("Shotgun facing stability PIE World is unavailable"));
		}

		ADroneNPCCharacter* ShotgunNPC = nullptr;
		for (TActorIterator<ADroneNPCCharacter> It(PIEWorld); It; ++It)
		{
			if (It->ActorHasTag(ShooterTag))
			{
				ShotgunNPC = *It;
				break;
			}
		}

		ADronePrototypePawn* Drone = nullptr;
		for (TActorIterator<ADronePrototypePawn> It(PIEWorld); It; ++It)
		{
			Drone = *It;
			break;
		}

		ADroneNPCAIController* Controller = ShotgunNPC
			? Cast<ADroneNPCAIController>(ShotgunNPC->GetController())
			: nullptr;
		if (!ShotgunNPC || !Drone || !Controller || !Controller->HasActiveDroneLookTarget())
		{
			return FinishOnTimeout(Now, TEXT("Shotgun NPC did not keep an active Drone target"));
		}

		if (!bAlignmentStarted)
		{
			bAlignmentStarted = true;
			AlignmentStartedAt = Now;
			const FVector NPCForward = ShotgunNPC->GetActorForwardVector().GetSafeNormal2D();
			TargetCenter = ShotgunNPC->GetActorLocation()
				+ NPCForward * 900.0f
				+ FVector::UpVector * 150.0f;
			Drone->SetActorLocation(TargetCenter, false, nullptr, ETeleportType::TeleportPhysics);
			return false;
		}

		if (!bSamplingStarted)
		{
			Drone->SetActorLocation(TargetCenter, false, nullptr, ETeleportType::TeleportPhysics);
			if (Now - AlignmentStartedAt < 0.35)
			{
				return false;
			}

			bSamplingStarted = true;
			SamplingStartedAt = Now;
			BaselineBodyYaw = ShotgunNPC->GetActorRotation().Yaw;
			if (const USkeletalMeshComponent* Mesh = ShotgunNPC->GetMesh();
				Mesh && Mesh->DoesSocketExist(TEXT("head")))
			{
				BaselineHeadYaw = Mesh->GetSocketRotation(TEXT("head")).Yaw;
			}
			const FRotator BaselineRotation(0.0f, BaselineBodyYaw, 0.0f);
			TargetCenter = ShotgunNPC->GetActorLocation()
				+ BaselineRotation.Vector() * 900.0f
				+ FVector::UpVector * 150.0f;
			TargetRight = FRotationMatrix(BaselineRotation).GetUnitAxis(EAxis::Y);
		}

		const double SamplingElapsed = Now - SamplingStartedAt;
		const int32 AlternationIndex = FMath::FloorToInt(SamplingElapsed / 0.10);
		const float Side = (AlternationIndex % 2 == 0) ? 1.0f : -1.0f;
		// 900cm 전방에서 30cm 횡이동은 약 1.9도다. 자연스러운 정면 조준 허용 범위다.
		Drone->SetActorLocation(
			TargetCenter + TargetRight * 30.0f * Side,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		MaxBodyYawExcursionDegrees = FMath::Max(
			MaxBodyYawExcursionDegrees,
			FMath::Abs(FMath::FindDeltaAngleDegrees(
				BaselineBodyYaw,
				ShotgunNPC->GetActorRotation().Yaw)));
		if (const USkeletalMeshComponent* Mesh = ShotgunNPC->GetMesh();
			Mesh && Mesh->DoesSocketExist(TEXT("head")))
		{
			const FRotator HeadRotation = Mesh->GetSocketRotation(TEXT("head"));
			const float RelativeHeadYaw = FMath::FindDeltaAngleDegrees(BaselineHeadYaw, HeadRotation.Yaw);
			if (!bHasHeadSample)
			{
				bHasHeadSample = true;
				MinHeadYaw = MaxHeadYaw = RelativeHeadYaw;
				MinHeadPitch = MaxHeadPitch = HeadRotation.Pitch;
			}
			else
			{
				MinHeadYaw = FMath::Min(MinHeadYaw, RelativeHeadYaw);
				MaxHeadYaw = FMath::Max(MaxHeadYaw, RelativeHeadYaw);
				MinHeadPitch = FMath::Min(MinHeadPitch, HeadRotation.Pitch);
				MaxHeadPitch = FMath::Max(MaxHeadPitch, HeadRotation.Pitch);
			}
		}

		if (SamplingElapsed < 1.0)
		{
			return false;
		}

		Test->TestTrue(
			TEXT("Personal-weapon body facing ignores sub-three-degree target jitter"),
			MaxBodyYawExcursionDegrees <= 0.75f);
		Test->TestTrue(TEXT("Personal-weapon head does not shake side-to-side while tracking"),
			bHasHeadSample && MaxHeadYaw - MinHeadYaw <= 4.0f);
		Test->TestTrue(TEXT("Personal-weapon fire presentation does not nod the head repeatedly"),
			bHasHeadSample && MaxHeadPitch - MinHeadPitch <= 8.0f);
		return true;
	}

private:
	bool FinishOnTimeout(const double Now, const TCHAR* Reason)
	{
		if (Now - StartedAt <= 5.0)
		{
			return false;
		}
		Test->AddError(FString::Printf(TEXT("%s after 5 seconds"), Reason));
		return true;
	}

	FAutomationTestBase* Test;
	double StartedAt = 0.0;
	double AlignmentStartedAt = 0.0;
	double SamplingStartedAt = 0.0;
	bool bAlignmentStarted = false;
	bool bSamplingStarted = false;
	float BaselineBodyYaw = 0.0f;
	float MaxBodyYawExcursionDegrees = 0.0f;
	float BaselineHeadYaw = 0.0f;
	float MinHeadYaw = 0.0f;
	float MaxHeadYaw = 0.0f;
	float MinHeadPitch = 0.0f;
	float MaxHeadPitch = 0.0f;
	bool bHasHeadSample = false;
	FVector TargetCenter = FVector::ZeroVector;
	FVector TargetRight = FVector::RightVector;
};

/** 사거리 밖 추적, 리시 초과 포기, 포기 뒤 순찰 유지까지 실제 NavMesh PIE에서 검증한다. */
class FValidatePersonalWeaponPursuitPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidatePersonalWeaponPursuitPIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
			PhaseStartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		ADroneNPCCharacter* ShotgunNPC = nullptr;
		ADronePrototypePawn* Drone = nullptr;
		if (PIEWorld)
		{
			for (TActorIterator<ADroneNPCCharacter> It(PIEWorld); It; ++It)
			{
				if (It->ActorHasTag(ShooterTag))
				{
					ShotgunNPC = *It;
					break;
				}
			}
			for (TActorIterator<ADronePrototypePawn> It(PIEWorld); It; ++It)
			{
				Drone = *It;
				break;
			}
		}
		ADroneNPCAIController* Controller = ShotgunNPC
			? Cast<ADroneNPCAIController>(ShotgunNPC->GetController())
			: nullptr;
		if (!PIEWorld || !ShotgunNPC || !Drone || !Controller)
		{
			return FinishWithError(Now, TEXT("Pursuit PIE actors are unavailable"));
		}
		const int32 CurrentResponseState = static_cast<int32>(Controller->GetResponseState());
		if (CurrentResponseState != LastSpinResponseState)
		{
			// 상태 전환 전후의 서로 다른 회전 목적을 하나의 연속 회전으로 합산하지 않는다.
			// 실제 빙글 회전은 같은 Pursue 상태 안에서 누적되는 회전으로 검출한다.
			bHasPreviousBodyYaw = false;
			ContinuousBodyYawTravelDegrees = 0.0f;
			LastMeaningfulBodyYawDirection = 0;
			LastMeaningfulBodyYawSampleAt = 0.0;
			LastSpinResponseState = CurrentResponseState;
		}
		const float CurrentBodyYaw = ShotgunNPC->GetActorRotation().Yaw;
		{
			const FVector ImmediateMoveOffset =
				Controller->GetImmediateMoveDestination() - ShotgunNPC->GetActorLocation();
			const FVector TargetOffset = Drone->GetActorLocation() - ShotgunNPC->GetActorLocation();
			const FVector HorizontalVelocity(
				ShotgunNPC->GetVelocity().X,
				ShotgunNPC->GetVelocity().Y,
				0.0f);
			SpinDiagnosticSamples.Add(FString::Printf(
				TEXT("t=%.2f p=%d s=%d body=%.1f vel=%.1f speed=%.1f next=%.1f/%.0f target=%.1f/%.0f moves=%d"),
				Now - StartedAt,
				static_cast<int32>(Phase),
				static_cast<int32>(Controller->GetResponseState()),
				CurrentBodyYaw,
				HorizontalVelocity.IsNearlyZero() ? 0.0f : HorizontalVelocity.Rotation().Yaw,
				HorizontalVelocity.Size(),
				ImmediateMoveOffset.IsNearlyZero() ? 0.0f : ImmediateMoveOffset.Rotation().Yaw,
				ImmediateMoveOffset.Size2D(),
				TargetOffset.IsNearlyZero() ? 0.0f : TargetOffset.Rotation().Yaw,
				TargetOffset.Size2D(),
				Controller->GetPersonalWeaponPursuitMoveRequestCount()));
			if (SpinDiagnosticSamples.Num() > 16)
			{
				SpinDiagnosticSamples.RemoveAt(0);
			}
		}
		if (bHasPreviousBodyYaw)
		{
			const float SignedYawDelta = FMath::FindDeltaAngleDegrees(PreviousBodyYaw, CurrentBodyYaw);
			const float AbsoluteYawDelta = FMath::Abs(SignedYawDelta);
			if (AbsoluteYawDelta > 0.5f)
			{
				const int32 TurnDirection = SignedYawDelta > 0.0f ? 1 : -1;
				const bool bContinuesSameTurn = TurnDirection == LastMeaningfulBodyYawDirection
					&& Now - LastMeaningfulBodyYawSampleAt <= 0.20;
				ContinuousBodyYawTravelDegrees = bContinuesSameTurn
					? ContinuousBodyYawTravelDegrees + AbsoluteYawDelta
					: AbsoluteYawDelta;
				MaxContinuousBodyYawTravelDegrees = FMath::Max(
					MaxContinuousBodyYawTravelDegrees,
					ContinuousBodyYawTravelDegrees);
				LastMeaningfulBodyYawDirection = TurnDirection;
				LastMeaningfulBodyYawSampleAt = Now;
			}
			if (ContinuousBodyYawTravelDegrees > 300.0f)
			{
				const FVector HorizontalVelocity(
					ShotgunNPC->GetVelocity().X,
					ShotgunNPC->GetVelocity().Y,
					0.0f);
				Test->AddError(FString::Printf(
					TEXT("Shotgun NPC continuously spins through a near-full turn during engagement (yaw travel %.1fdeg, state=%d, bodyYaw=%.1f, velocityYaw=%.1f, speed=%.1f). Samples: %s"),
					ContinuousBodyYawTravelDegrees,
					static_cast<int32>(Controller->GetResponseState()),
					CurrentBodyYaw,
					HorizontalVelocity.IsNearlyZero() ? 0.0f : HorizontalVelocity.Rotation().Yaw,
					HorizontalVelocity.Size(),
					*FString::Join(SpinDiagnosticSamples, TEXT(" | "))));
				return true;
			}
		}
		PreviousBodyYaw = CurrentBodyYaw;
		bHasPreviousBodyYaw = true;
		LastControllerState = static_cast<int32>(Controller->GetResponseState());
		LastDetected = Controller->HasDetectedDrone();
		LastPursuitCount = Controller->GetPersonalWeaponPursuitStartCount();
		LastDisengageCount = Controller->GetPersonalWeaponDisengageCount();

		switch (Phase)
		{
		case EPhase::WaitForDetection:
			if (!Controller->HasDetectedDrone())
			{
				return FinishWithError(Now, TEXT("Shotgun NPC did not detect the Drone before pursuit"));
			}
			CombatOrigin = Controller->HasPersonalWeaponCombatOrigin()
				? Controller->GetPersonalWeaponCombatOrigin()
				: ShotgunNPC->GetActorLocation();
			PursuitDirection = (Drone->GetActorLocation() - ShotgunNPC->GetActorLocation()).GetSafeNormal2D();
			if (PursuitDirection.IsNearlyZero())
			{
				PursuitDirection = ShotgunNPC->GetActorForwardVector().GetSafeNormal2D();
			}
			InitialPawnLocation = ShotgunNPC->GetActorLocation();
			PursuitCountBeforeBoundaryCheck = Controller->GetPersonalWeaponPursuitStartCount();
			Phase = EPhase::ConfirmRangeBoundaryStability;
			PhaseStartedAt = Now;
			return false;

		case EPhase::ConfirmRangeBoundaryStability:
			{
				const int32 AlternationIndex = FMath::FloorToInt((Now - PhaseStartedAt) / 0.10);
				const float BoundaryOffset = AlternationIndex % 2 == 0 ? -10.0f : 10.0f;
				const float DesiredDistance = Controller->GetPersonalWeaponRange() + BoundaryOffset;
				constexpr float TargetHeight = 120.0f;
				const float HorizontalDistance = FMath::Sqrt(FMath::Max(
					0.0f,
					FMath::Square(DesiredDistance) - FMath::Square(TargetHeight)));
				FVector BoundaryTarget = ShotgunNPC->GetActorLocation()
					+ PursuitDirection * HorizontalDistance;
				BoundaryTarget.Z = ShotgunNPC->GetActorLocation().Z + TargetHeight;
				Drone->SetActorLocation(BoundaryTarget, false, nullptr, ETeleportType::TeleportPhysics);
				const bool bIsPursuing = Controller->GetResponseState()
					== EDroneNPCAIResponseState::PursueDrone;
				bUnexpectedBoundaryPursuit |= bIsPursuing;
			}
			if (Now - PhaseStartedAt < 0.8)
			{
				return false;
			}
			Test->TestFalse(
				TEXT("Brief weapon-range boundary jitter does not start pursuit"),
				bUnexpectedBoundaryPursuit);
			Test->TestEqual(
				TEXT("Boundary jitter does not add a pursuit transition"),
				Controller->GetPersonalWeaponPursuitStartCount(),
				PursuitCountBeforeBoundaryCheck);
			{
				// 0.45초 안정 경로 확인 중 NPC가 사거리 안으로 들어오지 않을 여유를 둔다.
				// TestMap의 -X Navigation Floor 끝(-1,850cm) 안쪽은 유지한다.
				const float PursuitDistance = FMath::Min(
					Controller->GetPersonalWeaponRange() + 500.0f,
					Controller->GetPersonalWeaponCombatLeashRadius() * 0.75f);
				FVector PursuitTarget = CombatOrigin + PursuitDirection * PursuitDistance;
				PursuitTarget.Z = InitialPawnLocation.Z + 120.0f;
				Drone->SetActorLocation(PursuitTarget, false, nullptr, ETeleportType::TeleportPhysics);
			}
			InitialTargetDistance = FVector::Dist(ShotgunNPC->GetActorLocation(), Drone->GetActorLocation());
			Phase = EPhase::WaitForPursuit;
			PhaseStartedAt = Now;
			return false;

		case EPhase::WaitForPursuit:
			{
				const float PawnTravel = FVector::Dist2D(InitialPawnLocation, ShotgunNPC->GetActorLocation());
				const float CurrentDistance = FVector::Dist(ShotgunNPC->GetActorLocation(), Drone->GetActorLocation());
				MaxObservedPawnTravel = FMath::Max(MaxObservedPawnTravel, PawnTravel);
				MinObservedTargetDistance = FMath::Min(MinObservedTargetDistance, CurrentDistance);
				MaxObservedPawnSpeed = FMath::Max(MaxObservedPawnSpeed, ShotgunNPC->GetVelocity().Size2D());
				LastObservedPawnLocation = ShotgunNPC->GetActorLocation();
				LastObservedTargetLocation = Drone->GetActorLocation();
				LastObservedMoveStatus = static_cast<int32>(Controller->GetMoveStatus());
				if (Controller->GetResponseState() == EDroneNPCAIResponseState::PursueDrone
					&& PawnTravel >= 50.0f
					&& CurrentDistance <= InitialTargetDistance - 50.0f)
				{
					Test->TestTrue(TEXT("Out-of-range Shotgun NPC enters PursueDrone"), true);
					Test->TestTrue(TEXT("Pursuing NPC advances on the target"), true);
					const FVector MoveDirection = ShotgunNPC->GetVelocity().GetSafeNormal2D();
					const FVector PursuitGoalDirection =
						(Drone->GetActorLocation() - ShotgunNPC->GetActorLocation()).GetSafeNormal2D();
					const FVector BodyForward = ShotgunNPC->GetActorForwardVector().GetSafeNormal2D();
					const float ViewYaw = ShotgunNPC->GetActorRotation().Yaw
						+ Controller->GetSmoothedDroneLookRotation().Yaw;
					const FVector ViewForward = FRotator(0.0f, ViewYaw, 0.0f).Vector().GetSafeNormal2D();
					const float BodyMoveAlignment = FVector::DotProduct(BodyForward, PursuitGoalDirection);
					const float ViewMoveAlignment = FVector::DotProduct(ViewForward, PursuitGoalDirection);
					if (MoveDirection.IsNearlyZero()
						|| PursuitGoalDirection.IsNearlyZero()
						|| BodyMoveAlignment < 0.85f
						|| ViewMoveAlignment < 0.85f)
					{
						return FinishWithError(Now, TEXT("Pursuit body and gaze did not converge on the stable pursuit goal"));
					}
					Test->TestTrue(
						*FString::Printf(TEXT("Pursuit body faces the stable pursuit goal (dot %.3f)"), BodyMoveAlignment),
						true);
					Test->TestTrue(
						*FString::Printf(TEXT("Pursuit gaze follows the same stable pursuit goal (dot %.3f)"), ViewMoveAlignment),
						true);
					PursuitMoveRequestCountAtStableStart = Controller->GetPersonalWeaponPursuitMoveRequestCount();
					bPursuitMoveWasActiveAtStableStart = Controller->IsPersonalWeaponPursuitMoveActive();
					Phase = EPhase::ConfirmStablePursuit;
					PhaseStartedAt = Now;
					return false;
				}
				return FinishWithError(Now, TEXT("Shotgun NPC did not make pursuit progress"));
			}

		case EPhase::ConfirmStablePursuit:
			if (Controller->GetResponseState() != EDroneNPCAIResponseState::PursueDrone)
			{
				return FinishWithError(Now, TEXT("Shotgun NPC left pursuit before the stable-path check"));
			}
			if (Now - PhaseStartedAt < 0.45)
			{
				return false;
			}
			if (bPursuitMoveWasActiveAtStableStart)
			{
				Test->TestEqual(
					TEXT("A stationary pursuit target does not restart the active MoveTo path"),
					Controller->GetPersonalWeaponPursuitMoveRequestCount(),
					PursuitMoveRequestCountAtStableStart);
			}
			else
			{
				Test->AddInfo(TEXT("The initial pursuit MoveTo had already ended before the stability window; a later request is treated as recovery, not a duplicate."));
			}
			{
				const FVector MoveDirection = ShotgunNPC->GetVelocity().GetSafeNormal2D();
				const float InRangeDistance = FMath::Max(100.0f, Controller->GetPersonalWeaponRange() - 100.0f);
				constexpr float InRangeTargetHeight = 120.0f;
				const float InRangeHorizontalDistance = FMath::Sqrt(FMath::Max(
					0.0f,
					FMath::Square(InRangeDistance) - FMath::Square(InRangeTargetHeight)));
				FVector InRangeTarget = ShotgunNPC->GetActorLocation()
					+ MoveDirection * InRangeHorizontalDistance;
				InRangeTarget.Z = ShotgunNPC->GetActorLocation().Z + InRangeTargetHeight;
				Drone->SetActorLocation(InRangeTarget, false, nullptr, ETeleportType::TeleportPhysics);
				Phase = EPhase::ConfirmInRangeStop;
				PhaseStartedAt = Now;
				return false;
			}

		case EPhase::ConfirmInRangeStop:
			if (Controller->GetResponseState() != EDroneNPCAIResponseState::DroneDetected
				|| ShotgunNPC->GetVelocity().Size2D() > 30.0f)
			{
				return FinishWithError(Now, TEXT("Shotgun NPC kept pursuing after the target entered weapon range"));
			}
			Test->TestTrue(TEXT("Entering actual weapon range immediately stops pursuit and resumes fire"), true);
			DisengageCountBefore = Controller->GetPersonalWeaponDisengageCount();
			{
				FVector BeyondLeash = CombatOrigin + PursuitDirection
					* (Controller->GetPersonalWeaponCombatLeashRadius() + 350.0f);
				BeyondLeash.Z = InitialPawnLocation.Z + 120.0f;
				Drone->SetActorLocation(BeyondLeash, false, nullptr, ETeleportType::TeleportPhysics);
			}
			Phase = EPhase::WaitForDisengage;
			PhaseStartedAt = Now;
			return false;

		case EPhase::WaitForDisengage:
			if (Controller->GetResponseState() == EDroneNPCAIResponseState::Patrol
				&& !Controller->HasDetectedDrone()
				&& Controller->GetPersonalWeaponDisengageCount() == DisengageCountBefore + 1)
			{
				Phase = EPhase::ConfirmPatrolHold;
				PhaseStartedAt = Now;
				return false;
			}
			return FinishWithError(Now, TEXT("Leash exit did not disengage to Patrol"));

		case EPhase::ConfirmPatrolHold:
			if (Now - PhaseStartedAt < 0.75)
			{
				return false;
			}
			Test->TestTrue(TEXT("Disengaged NPC keeps Patrol while the target remains outside the leash"),
				Controller->GetResponseState() == EDroneNPCAIResponseState::Patrol
				&& !Controller->HasDetectedDrone());
			Test->TestEqual(TEXT("Outside-leash target does not trigger immediate disengage loops"),
				Controller->GetPersonalWeaponDisengageCount(), DisengageCountBefore + 1);
			Test->TestTrue(
				*FString::Printf(
					TEXT("Shotgun engagement does not contain a continuous near-full body spin (max yaw travel %.1fdeg)"),
					MaxContinuousBodyYawTravelDegrees),
				MaxContinuousBodyYawTravelDegrees <= 300.0f);
			return true;
		}
		return true;
	}

private:
	enum class EPhase : uint8
	{
		WaitForDetection,
		ConfirmRangeBoundaryStability,
		WaitForPursuit,
		ConfirmStablePursuit,
		ConfirmInRangeStop,
		WaitForDisengage,
		ConfirmPatrolHold
	};

	bool FinishWithError(const double Now, const TCHAR* Reason)
	{
		if (Now - PhaseStartedAt <= 6.0 && Now - StartedAt <= 15.0)
		{
			return false;
		}
		Test->AddError(FString::Printf(TEXT("%s (state=%d detected=%d pursuits=%d disengages=%d move=%d maxTravel=%.1f minDistance=%.1f maxSpeed=%.1f pawn=%s target=%s). Samples: %s"),
			Reason,
			LastControllerState,
			LastDetected ? 1 : 0,
			LastPursuitCount,
			LastDisengageCount,
			LastObservedMoveStatus,
			MaxObservedPawnTravel,
			MinObservedTargetDistance,
			MaxObservedPawnSpeed,
			*LastObservedPawnLocation.ToCompactString(),
			*LastObservedTargetLocation.ToCompactString(),
			*FString::Join(SpinDiagnosticSamples, TEXT(" | "))));
		return true;
	}

	FAutomationTestBase* Test;
	EPhase Phase = EPhase::WaitForDetection;
	double StartedAt = 0.0;
	double PhaseStartedAt = 0.0;
	FVector CombatOrigin = FVector::ZeroVector;
	FVector PursuitDirection = FVector::ForwardVector;
	FVector InitialPawnLocation = FVector::ZeroVector;
	float InitialTargetDistance = 0.0f;
	int32 DisengageCountBefore = 0;
	int32 PursuitCountBeforeBoundaryCheck = 0;
	int32 PursuitMoveRequestCountAtStableStart = 0;
	bool bPursuitMoveWasActiveAtStableStart = false;
	bool bUnexpectedBoundaryPursuit = false;
	bool bHasPreviousBodyYaw = false;
	float PreviousBodyYaw = 0.0f;
	float ContinuousBodyYawTravelDegrees = 0.0f;
	float MaxContinuousBodyYawTravelDegrees = 0.0f;
	int32 LastMeaningfulBodyYawDirection = 0;
	double LastMeaningfulBodyYawSampleAt = 0.0;
	TArray<FString> SpinDiagnosticSamples;
	float MaxObservedPawnTravel = 0.0f;
	float MinObservedTargetDistance = TNumericLimits<float>::Max();
	float MaxObservedPawnSpeed = 0.0f;
	FVector LastObservedPawnLocation = FVector::ZeroVector;
	FVector LastObservedTargetLocation = FVector::ZeroVector;
	int32 LastObservedMoveStatus = -1;
	int32 LastSpinResponseState = -1;
	int32 LastControllerState = -1;
	bool LastDetected = false;
	int32 LastPursuitCount = 0;
	int32 LastDisengageCount = 0;
};
} // namespace DroneShotgunSystemsTestMap

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneShotgunSystemsTestMapAssetTest,
	"Drone.AI.ShotgunSystemsTestMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneShotgunSystemsTestMapAssetTest::RunTest(const FString& Parameters)
{
	using namespace DroneShotgunSystemsTestMap;
	UClass* ShotgunNPCClass = LoadClass<ADroneNPCCharacter>(nullptr, ShotgunNPCClassPath);
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, GameModeClassPath);
	UClass* ShotgunPelletProjectileClass = LoadClass<ADroneNPCProjectile>(nullptr, ShotgunPelletProjectileClassPath);
	UWorld* World = LoadObject<UWorld>(nullptr, MapObjectPath);
	TestNotNull(TEXT("Dedicated Shotgun NPC Blueprint Class loads"), ShotgunNPCClass);
	TestNotNull(TEXT("Prototype GameMode Class loads"), GameModeClass);
	TestNotNull(TEXT("Shotgun Pellet projectile Blueprint Class loads"), ShotgunPelletProjectileClass);
	TestNotNull(TEXT("Shotgun systems test map loads"), World);
	if (!ShotgunNPCClass || !GameModeClass || !ShotgunPelletProjectileClass || !World)
	{
		return false;
	}

	TestTrue(TEXT("Shotgun systems test map keeps Prototype GameMode"), World->GetWorldSettings()->DefaultGameMode.Get() == GameModeClass);
	int32 ShotgunNPCCount = 0;
	int32 PlayerStartCount = 0;
	int32 NavigationFloorCount = 0;
	int32 NavBoundsCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		if (Actor->IsA(ShotgunNPCClass) && Actor->ActorHasTag(ShooterTag))
		{
			++ShotgunNPCCount;
		}
		PlayerStartCount += Actor->IsA<APlayerStart>() ? 1 : 0;
		NavigationFloorCount += Actor->IsA<ADroneNPCNavigationFloor>() ? 1 : 0;
		NavBoundsCount += Actor->GetClass()->GetPathName() == TEXT("/Script/NavigationSystem.NavMeshBoundsVolume") ? 1 : 0;
	}
	TestEqual(TEXT("Shotgun test map contains one dedicated Shotgun NPC"), ShotgunNPCCount, 1);
	TestEqual(TEXT("Shotgun test map contains one PlayerStart"), PlayerStartCount, 1);
	TestEqual(TEXT("Shotgun test map contains one project Navigation Floor"), NavigationFloorCount, 1);
	TestEqual(TEXT("Shotgun test map contains one NavMesh bounds volume"), NavBoundsCount, 1);

	const ADroneNPCCharacter* ShotgunDefaults = Cast<ADroneNPCCharacter>(ShotgunNPCClass->GetDefaultObject());
	const UDroneNPCProfileComponent* Profile = ShotgunDefaults ? ShotgunDefaults->GetNPCProfileComponent() : nullptr;
	const UDroneNPCWeaponComponent* Weapon = ShotgunDefaults ? ShotgunDefaults->GetNPCWeaponComponent() : nullptr;
	TestNotNull(TEXT("Shotgun Blueprint CDO owns Profile Component"), Profile);
	TestNotNull(TEXT("Shotgun Blueprint CDO owns Weapon Component"), Weapon);
	TestTrue(TEXT("Shotgun Blueprint profile is Hostile"), Profile && Profile->IsHostile());
	TestTrue(
		TEXT("Shotgun Blueprint profile selects Shotgun"),
		Profile && Profile->GetProfile().WeaponType == EDroneNPCWeaponType::Shotgun);
	TestTrue(TEXT("Shotgun Blueprint cannot claim an MG turret"), Profile && !Profile->GetProfile().bCanUseMGTurret);
	TestEqual(
		TEXT("Shotgun Blueprint profile keeps the three-degree facing dead zone"),
		Profile ? Profile->GetProfile().PersonalWeaponFacingDeadZoneDegrees : -1.0f,
		3.0f);
	TestEqual(
		TEXT("Shotgun Blueprint profile keeps the facing turn speed"),
		Profile ? Profile->GetProfile().PersonalWeaponFacingTurnSpeedDegreesPerSecond : -1.0f,
		180.0f);
	TestTrue(TEXT("Shotgun Blueprint defaults to projectile ballistics"), Weapon && Weapon->UsesProjectileBallistics());
	TestEqual(TEXT("Shotgun Blueprint keeps eight Pellets"), Weapon ? Weapon->GetShotgunPelletCount() : 0, 8);
	TestEqual(TEXT("Shotgun Blueprint keeps twelve-degree spread"), Weapon ? Weapon->GetShotgunSpreadHalfAngleDegrees() : 0.0f, 12.0f);
	TestFalse(TEXT("Shotgun Blueprint disables cyan debug rays"), Weapon && Weapon->IsShotgunDebugTraceEnabled());
	TestEqual(TEXT("Shotgun Blueprint keeps 3500 cm/s Pellet speed"), Weapon ? Weapon->GetShotgunProjectileSpeed() : 0.0f, 3500.0f);
	TestEqual(TEXT("Shotgun Pellet damage stays at the low Greybox value"), Weapon ? Weapon->GetShotgunDamagePerPellet() : 0.0f, 3.0f);
	TestTrue(
		TEXT("Shotgun Blueprint selects its dedicated Pellet projectile Class"),
		Weapon && Weapon->GetProjectileClass() == ShotgunPelletProjectileClass);

	ADroneNPCProjectile* ProjectileDefaults = Cast<ADroneNPCProjectile>(ShotgunPelletProjectileClass->GetDefaultObject());
	const UStaticMeshComponent* ProjectileVisual = ProjectileDefaults
		? Cast<UStaticMeshComponent>(ProjectileDefaults->GetDefaultSubobjectByName(TEXT("ProjectileVisual")))
		: nullptr;
	const UStaticMeshComponent* ProjectileTrailVisual = ProjectileDefaults
		? Cast<UStaticMeshComponent>(ProjectileDefaults->GetDefaultSubobjectByName(TEXT("ProjectileTrailVisual")))
		: nullptr;
	TestNotNull(TEXT("Projectile owns a replaceable core Visual component"), ProjectileVisual);
	TestNotNull(TEXT("Projectile owns a replaceable short Tracer component"), ProjectileTrailVisual);
	TestTrue(
		TEXT("Production Shotgun Pellet keeps a readable bead scale"),
		ProjectileVisual
			&& ProjectileVisual->GetRelativeScale3D().X >= 0.035f
			&& ProjectileVisual->GetRelativeScale3D().X <= 0.06f);
	TestTrue(
		TEXT("Production Shotgun Pellet keeps a short readable tracer"),
		ProjectileTrailVisual
			&& ProjectileTrailVisual->GetRelativeScale3D().X >= 0.18f
			&& ProjectileTrailVisual->GetRelativeScale3D().Y >= 0.01f);
	TestNotNull(
		TEXT("Production Shotgun Pellet bead uses a visible Material"),
		ProjectileVisual ? ProjectileVisual->GetMaterial(0) : nullptr);
	TestNotNull(
		TEXT("Production Shotgun Pellet tracer uses a visible Material"),
		ProjectileTrailVisual ? ProjectileTrailVisual->GetMaterial(0) : nullptr);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneShotgunSystemsTestMapPIETest,
	"Drone.AI.ShotgunSystemsTestMapPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneShotgunSystemsTestMapPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneShotgunSystemsTestMap;
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains,
		0);
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Shotgun systems PIE test requires an idle Editor"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapPackage));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateShotgunFirePIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FValidatePersonalWeaponFacingStabilityPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FValidatePersonalWeaponPursuitPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
