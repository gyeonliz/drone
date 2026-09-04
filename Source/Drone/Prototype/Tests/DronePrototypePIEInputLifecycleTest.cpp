#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Prototype/DronePrototypePawn.h"
#include "Prototype/DronePrototypePlayerController.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformTime.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Math/RotationMatrix.h"
#include "Misc/AutomationTest.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Telemetry/DroneTelemetryComponent.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFlightHUDWidget.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"

namespace DronePrototypePIEInputLifecycle
{
// 중앙 Maps 폴더의 실제 Prototype Asset을 고정해 Legacy 템플릿 자산으로 되돌아가는 회귀를 막는다.
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/Lvl_DronePrototype");
constexpr const TCHAR* PawnClassPath = TEXT("/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration.BP_DroneFPVIntegration_C");
constexpr const TCHAR* ControllerClassPath = TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypePlayerController.BP_DronePrototypePlayerController_C");
constexpr const TCHAR* FlightHUDClassPath = TEXT("/Game/Drone/Prototype/UI/WBP_DroneFlightHUD.WBP_DroneFlightHUD_C");
constexpr const TCHAR* ContextPath = TEXT("/Game/Drone/Prototype/Input/IMC_DronePrototype.IMC_DronePrototype");
constexpr const TCHAR* MovePath = TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_Move.IA_DronePrototype_Move");
constexpr const TCHAR* AltitudePath = TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_Altitude.IA_DronePrototype_Altitude");
constexpr const TCHAR* YawPath = TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_Yaw.IA_DronePrototype_Yaw");
constexpr const TCHAR* LookPath = TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_Look.IA_DronePrototype_Look");
constexpr const TCHAR* CameraPitchRatePath = TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_CameraPitchRate.IA_DronePrototype_CameraPitchRate");
constexpr const TCHAR* ToggleViewPath = TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_ToggleView.IA_DronePrototype_ToggleView");
constexpr int32 ExpectedMappingCount = 16;
constexpr double EffectSampleSeconds = 0.2;
constexpr float TranslationTolerance = 0.01f;
constexpr float RotationTolerance = 0.001f;
constexpr float MaximumOpposingInputStrengthRatio = 1.5f;
constexpr float MinimumCycleStrengthRatio = 0.5f;
constexpr float MaximumCycleStrengthRatio = 1.5f;

bool HasPIEWorld()
{
	if (!GEngine)
	{
		return false;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
		{
			return true;
		}
	}

	return false;
}

FRequestPlaySessionParams MakePlayParams()
{
	// 각 회차를 새 1인 Standalone PIE로 만들어 이전 회차의 UI/Input 상태 재사용을 막는다.
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(1);
	Settings->bLaunchSeparateServer = false;
	Settings->AddToRoot(); // Removed by FStartPIEForAutomationCommand in UE 5.8.

	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.EditorPlaySettings = Settings;
	Params.bAllowOnlineSubsystem = false;
	return Params;
}

class FNestedStartPIEForAutomationCommand final : public FStartPIEForAutomationCommand
{
public:
	explicit FNestedStartPIEForAutomationCommand(FRequestPlaySessionParams InRequestParams)
		: FStartPIEForAutomationCommand(MoveTemp(InRequestParams))
	{
	}

	virtual bool Update() override
	{
		// Nested latent commands do not pass through FAutomationTestFramework::InternalUpdate().
		if (StartTime == 0.0)
		{
			StartTime = FPlatformTime::Seconds();
		}

		return FStartPIEForAutomationCommand::Update();
	}
};

class FLazyStartPIEForAutomationCommand final : public IAutomationLatentCommand
{
public:
	explicit FLazyStartPIEForAutomationCommand(FRequestPlaySessionParams InRequestParams)
		: RequestParams(MoveTemp(InRequestParams))
	{
	}

	virtual ~FLazyStartPIEForAutomationCommand() override
	{
		if (!StartCommand)
		{
			if (ULevelEditorPlaySettings* Settings = RequestParams.EditorPlaySettings.Get())
			{
				Settings->RemoveFromRoot();
			}
		}
	}

	virtual bool Update() override
	{
		if (!StartCommand)
		{
			StartCommand = MakeUnique<FNestedStartPIEForAutomationCommand>(MoveTemp(RequestParams));
		}

		return StartCommand->Update();
	}

private:
	FRequestPlaySessionParams RequestParams;
	TUniquePtr<FNestedStartPIEForAutomationCommand> StartCommand;
};

enum class EProbeEffect : uint8
{
	Forward,
	Right,
	Up,
	ActorYaw,
	CameraPitch
};

struct FProbe
{
	FKey Key;
	const TCHAR* Label;
	EProbeEffect Effect;
	float ExpectedSign;
	bool bAnalog;
};

enum class ECombinationExpectation : uint8
{
	ForwardAndUp,
	RightAndActorYaw,
	OpposeForward,
	OpposeRight
};

struct FCombinationProbe
{
	FKey FirstKey;
	FKey SecondKey;
	const TCHAR* Label;
	ECombinationExpectation Expectation;
};

enum class EAcquireResult : uint8
{
	Wait,
	Ready,
	Fatal
};

struct FEffectStrengthHistory
{
	bool RecordOrCompare(
		const int32 Cycle,
		const TMap<FName, float>& CurrentStrengths,
		FString& OutError)
	{
		if (CurrentStrengths.IsEmpty())
		{
			OutError = TEXT("no input-effect strength samples were recorded");
			return false;
		}

		if (Cycle == 1)
		{
			BaselineStrengths = CurrentStrengths;
			return true;
		}

		if (BaselineStrengths.Num() != CurrentStrengths.Num())
		{
			OutError = FString::Printf(
				TEXT("cycle strength sample count is %d, baseline has %d"),
				CurrentStrengths.Num(),
				BaselineStrengths.Num());
			return false;
		}

		for (const TPair<FName, float>& Baseline : BaselineStrengths)
		{
			const float* Current = CurrentStrengths.Find(Baseline.Key);
			if (!Current)
			{
				OutError = FString::Printf(TEXT("cycle is missing strength sample %s"), *Baseline.Key.ToString());
				return false;
			}

			if (!FMath::IsFinite(Baseline.Value) || !FMath::IsFinite(*Current)
				|| Baseline.Value <= UE_SMALL_NUMBER || *Current <= UE_SMALL_NUMBER)
			{
				OutError = FString::Printf(
					TEXT("strength sample %s is invalid (baseline %.6f, current %.6f)"),
					*Baseline.Key.ToString(),
					Baseline.Value,
					*Current);
				return false;
			}

			const float Ratio = *Current / Baseline.Value;
			if (Ratio < MinimumCycleStrengthRatio || Ratio > MaximumCycleStrengthRatio)
			{
				OutError = FString::Printf(
					TEXT("%s strength changed across PIE cycles (baseline %.6f, current %.6f, ratio %.3f; expected %.2f-%.2f)"),
					*Baseline.Key.ToString(),
					Baseline.Value,
					*Current,
					Ratio,
					MinimumCycleStrengthRatio,
					MaximumCycleStrengthRatio);
				return false;
			}
		}

		return true;
	}

private:
	TMap<FName, float> BaselineStrengths;
};

struct FPIECycleLifecycleState
{
	void Capture(
		UEnhancedInputLocalPlayerSubsystem* InSubsystem,
		UInputMappingContext* InMappingContext,
		ADronePrototypePlayerController* InController,
		UDroneFlightHUDWidget* InFlightHUD,
		UDroneTelemetryComponent* InTelemetry)
	{
		Subsystem = InSubsystem;
		MappingContext = InMappingContext;
		Controller = InController;
		FlightHUD = InFlightHUD;
		Telemetry = InTelemetry;
	}

	bool IsMappingContextStillApplied() const
	{
		UEnhancedInputLocalPlayerSubsystem* CurrentSubsystem = Subsystem.Get();
		UInputMappingContext* CurrentMappingContext = MappingContext.Get();
		return CurrentSubsystem && CurrentMappingContext
			&& CurrentSubsystem->HasMappingContext(CurrentMappingContext);
	}

	bool IsHUDLifecycleStillBound(FString& OutReason) const
	{
		UDroneFlightHUDWidget* CurrentHUD = FlightHUD.Get();
		UDroneTelemetryComponent* CurrentTelemetry = Telemetry.Get();
		ADronePrototypePlayerController* CurrentController = Controller.Get();

		if (CurrentHUD && (CurrentHUD->IsInViewport() || CurrentHUD->HasTelemetrySource()))
		{
			OutReason = TEXT("Flight HUD retained its viewport or telemetry source after PIE teardown");
			return true;
		}

		if (CurrentHUD && CurrentTelemetry
			&& CurrentTelemetry->OnTelemetryUpdated.Contains(
				CurrentHUD,
				FName(TEXT("HandleTelemetryUpdated"))))
		{
			OutReason = TEXT("Flight HUD telemetry delegate remained bound after PIE teardown");
			return true;
		}

		if (CurrentController
			&& CurrentController->OnPossessedPawnChanged.Contains(
				CurrentController,
				FName(TEXT("HandlePossessedPawnChanged"))))
		{
			OutReason = TEXT("Prototype PlayerController possession delegate remained bound after PIE teardown");
			return true;
		}

		return false;
	}

private:
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> Subsystem;
	TWeakObjectPtr<UInputMappingContext> MappingContext;
	TWeakObjectPtr<ADronePrototypePlayerController> Controller;
	TWeakObjectPtr<UDroneFlightHUDWidget> FlightHUD;
	TWeakObjectPtr<UDroneTelemetryComponent> Telemetry;
};

class FValidatePIEInputCommand final : public IAutomationLatentCommand
{
public:
	FValidatePIEInputCommand(
		FAutomationTestBase* InTest,
		const int32 InCycle,
		const TSharedRef<FEffectStrengthHistory>& InStrengthHistory,
		const TSharedRef<FPIECycleLifecycleState>& InLifecycleState)
		: Test(InTest)
		, Cycle(InCycle)
		, StrengthHistory(InStrengthHistory)
		, LifecycleState(InLifecycleState)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (!bActivated)
		{
			bActivated = true;
			ActivatedAt = Now;
		}

		if (Now - ActivatedAt > 60.0)
		{
			return Fail(TEXT("input validation exceeded 60 seconds"));
		}

		if (Phase == EPhase::Acquire)
		{
			FString Reason;
			const EAcquireResult Result = TryAcquire(Reason);
			if (Result == EAcquireResult::Fatal)
			{
				return Fail(Reason);
			}

			if (Result == EAcquireResult::Wait)
			{
				if (Now - ActivatedAt > 10.0)
				{
					return Fail(FString::Printf(TEXT("setup did not become ready: %s"), *Reason));
				}
				return false;
			}

			if (!bHUDPossessionCycleCompleted)
			{
				Phase = EPhase::HUDUnpossess;
				return false;
			}

			BuildProbes();
			BuildCombinationProbes();
			Phase = EPhase::Reset;
			return false;
		}

		ADronePrototypePawn* CurrentPawn = Pawn.Get();
		ADronePrototypePlayerController* CurrentController = Controller.Get();
		UEnhancedPlayerInput* CurrentInput = PlayerInput.Get();
		UDroneFlightHUDWidget* CurrentFlightHUD = FlightHUD.Get();
		UDroneTelemetryComponent* CurrentTelemetry = Telemetry.Get();
		if (!CurrentPawn || !CurrentController || !CurrentInput || !CurrentFlightHUD || !CurrentTelemetry)
		{
			return Fail(TEXT("PIE input or Flight HUD objects became invalid during lifecycle probes"));
		}

		if (Phase == EPhase::HUDUnpossess)
		{
			CurrentController->UnPossess();
			if (CurrentController->GetPawn() != nullptr
				|| CurrentFlightHUD != CurrentController->GetFlightHUDWidget()
				|| CurrentFlightHUD->HasTelemetrySource()
				|| CurrentFlightHUD->GetVisibility() != ESlateVisibility::Collapsed
				|| CurrentTelemetry->OnTelemetryUpdated.Contains(
					CurrentFlightHUD,
					FName(TEXT("HandleTelemetryUpdated"))))
			{
				return Fail(TEXT("UnPossess did not reuse and hide the Flight HUD or release telemetry"));
			}

			Phase = EPhase::HUDRepossess;
			return false;
		}

		if (Phase == EPhase::HUDRepossess)
		{
			CurrentController->Possess(CurrentPawn);
			if (CurrentController->GetPawn() != CurrentPawn
				|| CurrentFlightHUD != CurrentController->GetFlightHUDWidget()
				|| CurrentFlightHUD->GetTelemetrySource() != CurrentTelemetry
				|| CurrentFlightHUD->GetVisibility() != ESlateVisibility::HitTestInvisible
				|| !CurrentTelemetry->OnTelemetryUpdated.Contains(
					CurrentFlightHUD,
					FName(TEXT("HandleTelemetryUpdated"))))
			{
				return Fail(TEXT("Re-Possess did not reuse and reconnect the Flight HUD"));
			}

			bHUDPossessionCycleCompleted = true;
			Phase = EPhase::Acquire;
			return false;
		}

		if (ProbeIndex >= Probes.Num() && CombinationIndex >= CombinationProbes.Num())
		{
			CurrentInput->FlushPressedKeys();
			FString StrengthError;
			if (!StrengthHistory->RecordOrCompare(Cycle, CycleStrengths, StrengthError))
			{
				return Fail(StrengthError);
			}

			Test->AddInfo(FString::Printf(
				TEXT("[fresh PIE %d/3] HUD viewport/possession lifecycle, keyboard, mouse, gamepad, combination, opposing-input, and strength probes passed"),
				Cycle));
			return true;
		}

		if (ProbeIndex >= Probes.Num() && Phase == EPhase::Reset)
		{
			Phase = EPhase::CombinationReset;
		}

		switch (Phase)
		{
		case EPhase::Reset:
		{
			const FProbe& Probe = Probes[ProbeIndex];
			if (UFloatingPawnMovement* Movement = CurrentPawn->GetPrototypeMovementComponent())
			{
				Movement->StopMovementImmediately();
			}
			CurrentInput->FlushPressedKeys();
			CurrentPawn->ConsumeMovementInputVector();
			if (!CurrentPawn->SetActorTransform(InitialPawnTransform, false, nullptr, ETeleportType::TeleportPhysics))
			{
				return Fail(FString::Printf(TEXT("%s: could not reset Pawn transform"), Probe.Label));
			}
			CurrentController->SetControlRotation(InitialControlRotation);
			if (USpringArmComponent* CameraBoom = CurrentPawn->GetCameraBoom())
			{
				CameraBoom->SetRelativeRotation(InitialCameraBoomRotation);
			}
			FramesToSettle = 1;
			Phase = EPhase::Settle;
			return false;
		}

		case EPhase::Settle:
		{
			const FProbe& Probe = Probes[ProbeIndex];
			if (FramesToSettle-- > 0)
			{
				return false;
			}
			BaselineLocation = CurrentPawn->GetActorLocation();
			BaselineActorRotation = CurrentPawn->GetActorRotation();
			BaselineControlRotation = CurrentController->GetControlRotation();
			BaselineCameraBoomRotation = CurrentPawn->GetCameraBoom()->GetRelativeRotation();
			ProbeStartedWorldSeconds = CurrentPawn->GetWorld()->GetTimeSeconds();
			AnalogDispatchCount = 0;
			SendProbe(Probe, *CurrentInput);
			if (Probe.bAnalog)
			{
				++AnalogDispatchCount;
			}
			ProbeStartedAt = Now;
			Phase = EPhase::Observe;
			return false;
		}

		case EPhase::Observe:
		{
			const FProbe& Probe = Probes[ProbeIndex];
			const float SignedEffect = MeasureSignedEffect(Probe, *CurrentPawn, *CurrentController);
			const float Tolerance =
				(Probe.Effect == EProbeEffect::Forward || Probe.Effect == EProbeEffect::Right || Probe.Effect == EProbeEffect::Up)
				? TranslationTolerance
				: RotationTolerance;

			if (SignedEffect < -Tolerance)
			{
				ReleaseProbe(Probe, *CurrentInput);
				return Fail(FString::Printf(TEXT("%s moved in the opposite direction (signed effect %.6f)"), Probe.Label, SignedEffect));
			}

			const double ProbeElapsedWorldSeconds =
				CurrentPawn->GetWorld()->GetTimeSeconds() - ProbeStartedWorldSeconds;
			if (ProbeElapsedWorldSeconds < EffectSampleSeconds)
			{
				if (Now - ProbeStartedAt > 2.0)
				{
					ReleaseProbe(Probe, *CurrentInput);
					return Fail(FString::Printf(TEXT("%s input sample did not advance PIE time"), Probe.Label));
				}

				if (Probe.bAnalog)
				{
					SendProbe(Probe, *CurrentInput); // Mouse delta is a one-frame input.
					++AnalogDispatchCount;
				}
				return false;
			}

			ReleaseProbe(Probe, *CurrentInput);
			if (SignedEffect <= Tolerance)
			{
				return Fail(FString::Printf(TEXT("%s produced no observable callback effect"), Probe.Label));
			}

			FString IsolationError;
			if (!CheckRotationIsolation(Probe, *CurrentPawn, *CurrentController, IsolationError))
			{
				return Fail(IsolationError);
			}

			const float NormalizedStrength = NormalizeEffectStrength(
				Probe.Effect,
				SignedEffect,
				ProbeElapsedWorldSeconds,
				AnalogDispatchCount);
			CycleStrengths.Add(FName(Probe.Label), NormalizedStrength);
			++ProbeIndex;
			Phase = EPhase::Reset;
			return false;
		}

		case EPhase::CombinationReset:
		{
			FString ResetError;
			if (!ResetForCombination(*CurrentPawn, *CurrentController, *CurrentInput, ResetError))
			{
				return Fail(ResetError);
			}
			FramesToSettle = 1;
			Phase = EPhase::CombinationSettle;
			return false;
		}

		case EPhase::CombinationSettle:
			if (FramesToSettle-- > 0)
			{
				return false;
			}
			BeginCombination(
				CombinationProbes[CombinationIndex],
				*CurrentPawn,
				*CurrentController,
				*CurrentInput,
				Now);
			Phase = EPhase::CombinationObserve;
			return false;

		case EPhase::CombinationObserve:
			return UpdateCombination(
				CombinationProbes[CombinationIndex],
				*CurrentPawn,
				*CurrentController,
				*CurrentInput,
				Now);

		default:
			return Fail(TEXT("invalid latent input phase"));
		}
	}

private:
	enum class EPhase : uint8
	{
		Acquire,
		HUDUnpossess,
		HUDRepossess,
		Reset,
		Settle,
		Observe,
		CombinationReset,
		CombinationSettle,
		CombinationObserve
	};

	EAcquireResult TryAcquire(FString& OutReason)
	{
		UWorld* WorldObject = AutomationCommon::GetAnyGameWorld();
		if (!WorldObject || WorldObject->WorldType != EWorldType::PIE)
		{
			OutReason = TEXT("PIE world is unavailable");
			return EAcquireResult::Wait;
		}

		if (!WorldObject->GetMapName().EndsWith(TEXT("Lvl_DronePrototype")))
		{
			OutReason = FString::Printf(TEXT("wrong PIE map: %s"), *WorldObject->GetMapName());
			return EAcquireResult::Fatal;
		}

		int32 PawnCount = 0;
		ADronePrototypePawn* FoundPawn = nullptr;
		for (TActorIterator<ADronePrototypePawn> It(WorldObject); It; ++It)
		{
			FoundPawn = *It;
			++PawnCount;
		}

		if (PawnCount == 0)
		{
			OutReason = TEXT("Prototype Pawn has not spawned yet");
			return EAcquireResult::Wait;
		}

		if (PawnCount != 1)
		{
			OutReason = FString::Printf(TEXT("expected one Prototype Pawn, found %d"), PawnCount);
			return EAcquireResult::Fatal;
		}

		ADronePrototypePlayerController* FoundController =
			Cast<ADronePrototypePlayerController>(WorldObject->GetFirstPlayerController());
		if (!FoundController || !FoundController->IsLocalController())
		{
			OutReason = TEXT("local Prototype PlayerController is unavailable");
			return EAcquireResult::Wait;
		}

		if (FoundController->GetPawn() != FoundPawn || FoundPawn->GetController() != FoundController)
		{
			OutReason = TEXT("Prototype Pawn is not possessed by the local PlayerController");
			return EAcquireResult::Fatal;
		}

		UClass* ExpectedControllerClass = LoadClass<ADronePrototypePlayerController>(nullptr, ControllerClassPath);
		if (!ExpectedControllerClass || FoundController->GetClass() != ExpectedControllerClass)
		{
			OutReason = FString::Printf(
				TEXT("PlayerController class is %s, expected BP_DronePrototypePlayerController_C"),
				*GetNameSafe(FoundController->GetClass()));
			return EAcquireResult::Fatal;
		}

		// PlayerController가 만든 단 하나의 HUD가 현재 Drone Telemetry에 연결됐는지 확인한다.
		UDroneFlightHUDWidget* FoundFlightHUD = FoundController->GetFlightHUDWidget();
		if (!FoundFlightHUD || !FoundFlightHUD->IsInViewport())
		{
			OutReason = TEXT("Flight HUD Widget is not on the local Player screen yet");
			return EAcquireResult::Wait;
		}

		UClass* ExpectedFlightHUDClass = LoadClass<UDroneFlightHUDWidget>(nullptr, FlightHUDClassPath);
		if (!ExpectedFlightHUDClass
			|| FoundFlightHUD->GetClass() != ExpectedFlightHUDClass
			|| FoundFlightHUD->IsUsingNativeFallbackLayout())
		{
			OutReason = FString::Printf(
				TEXT("Flight HUD class is %s or its WBP Designer contract fell back to native layout"),
				*GetNameSafe(FoundFlightHUD->GetClass()));
			return EAcquireResult::Fatal;
		}

		if (FoundFlightHUD->GetTelemetrySource() != FoundPawn->GetTelemetryComponent()
			|| FoundFlightHUD->GetOwningPlayer() != FoundController
			|| FoundFlightHUD->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			OutReason = TEXT("Flight HUD is not visibly connected to the possessed Drone telemetry");
			return EAcquireResult::Fatal;
		}

		UDroneTelemetryComponent* FoundTelemetry = FoundPawn->GetTelemetryComponent();
		if (!FoundTelemetry
			|| !FoundTelemetry->OnTelemetryUpdated.Contains(
				FoundFlightHUD,
				FName(TEXT("HandleTelemetryUpdated")))
			|| !FoundController->OnPossessedPawnChanged.Contains(
				FoundController,
				FName(TEXT("HandlePossessedPawnChanged"))))
		{
			OutReason = TEXT("Flight HUD or possession lifecycle delegate is not bound");
			return EAcquireResult::Fatal;
		}

		// 새 PIE마다 이전 Widget이 남지 않고 World/Viewport에 정확히 하나만 있어야 한다.
		int32 WorldFlightHUDCount = 0;
		int32 VisibleFlightHUDCount = 0;
		for (TObjectIterator<UDroneFlightHUDWidget> It; It; ++It)
		{
			UDroneFlightHUDWidget* Candidate = *It;
			if (!Candidate || Candidate->IsTemplate())
			{
				continue;
			}

			if (Candidate->GetWorld() == WorldObject)
			{
				++WorldFlightHUDCount;
			}
			if (Candidate->IsInViewport())
			{
				++VisibleFlightHUDCount;
			}
		}

		if (WorldFlightHUDCount != 1 || VisibleFlightHUDCount != 1)
		{
			OutReason = FString::Printf(
				TEXT("expected one PIE Flight HUD and one visible Flight HUD, found %d and %d"),
				WorldFlightHUDCount,
				VisibleFlightHUDCount);
			return EAcquireResult::Fatal;
		}

		FoundTelemetry->RefreshTelemetry();
		const FDroneTelemetrySnapshot LatestSnapshot = FoundTelemetry->GetLatestSnapshot();
		const FDroneTelemetrySnapshot DisplayedSnapshot = FoundFlightHUD->GetDisplayedSnapshot();
		if (!FMath::IsNearlyEqual(DisplayedSnapshot.SpeedKilometersPerHour, LatestSnapshot.SpeedKilometersPerHour)
			|| !FMath::IsNearlyEqual(DisplayedSnapshot.AltitudeMeters, LatestSnapshot.AltitudeMeters)
			|| !FMath::IsNearlyEqual(DisplayedSnapshot.VerticalSpeedMetersPerSecond, LatestSnapshot.VerticalSpeedMetersPerSecond)
			|| !FMath::IsNearlyEqual(DisplayedSnapshot.HeadingDegrees, LatestSnapshot.HeadingDegrees))
		{
			OutReason = TEXT("Flight HUD did not receive the latest telemetry event Snapshot");
			return EAcquireResult::Fatal;
		}

		UClass* ExpectedPawnClass = LoadClass<ADronePrototypePawn>(nullptr, PawnClassPath);
		if (!ExpectedPawnClass || FoundPawn->GetClass() != ExpectedPawnClass)
		{
			OutReason = FString::Printf(
				TEXT("spawned Pawn class is %s, expected BP_DroneFPVIntegration_C"),
				*GetNameSafe(FoundPawn->GetClass()));
			return EAcquireResult::Fatal;
		}

		ULocalPlayer* LocalPlayer = FoundController->GetLocalPlayer();
		UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
		UEnhancedPlayerInput* FoundInput = Subsystem ? Subsystem->GetPlayerInput() : nullptr;
		UEnhancedInputComponent* InputComponent = Cast<UEnhancedInputComponent>(FoundPawn->InputComponent);
		if (!Subsystem || !FoundInput || !InputComponent)
		{
			OutReason = TEXT("Enhanced Input subsystem/player input/component is not ready");
			return EAcquireResult::Wait;
		}

		UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, ContextPath);
		UInputAction* Move = LoadObject<UInputAction>(nullptr, MovePath);
		UInputAction* Altitude = LoadObject<UInputAction>(nullptr, AltitudePath);
		UInputAction* Yaw = LoadObject<UInputAction>(nullptr, YawPath);
		UInputAction* Look = LoadObject<UInputAction>(nullptr, LookPath);
		UInputAction* CameraPitchRate = LoadObject<UInputAction>(nullptr, CameraPitchRatePath);
		UInputAction* ToggleView = LoadObject<UInputAction>(nullptr, ToggleViewPath);
		if (!IMC || !Move || !Altitude || !Yaw || !Look || !CameraPitchRate || !ToggleView)
		{
			OutReason = TEXT("one or more Prototype input assets failed to load");
			return EAcquireResult::Fatal;
		}

		int32 Priority = INDEX_NONE;
		if (!Subsystem->HasMappingContext(IMC, Priority))
		{
			OutReason = TEXT("IMC_DronePrototype is not applied yet");
			return EAcquireResult::Wait;
		}

		if (Priority != 1)
		{
			OutReason = FString::Printf(TEXT("IMC_DronePrototype priority is %d, expected 1"), Priority);
			return EAcquireResult::Fatal;
		}

		const TArray<FEnhancedActionKeyMapping>& SourceMappings = IMC->GetMappings();
		if (SourceMappings.Num() != ExpectedMappingCount)
		{
			OutReason = FString::Printf(
				TEXT("IMC source has %d mappings, expected %d"),
				SourceMappings.Num(),
				ExpectedMappingCount);
			return EAcquireResult::Fatal;
		}

		const TConstArrayView<const FEnhancedActionKeyMapping> EffectiveMappings = FoundInput->GetEnhancedActionMappingsView();
		int32 ToggleViewPMappingCount = 0;
		for (const FEnhancedActionKeyMapping& Source : SourceMappings)
		{
			if (Source.Action == ToggleView && Source.Key == EKeys::P)
			{
				++ToggleViewPMappingCount;
			}
		}
		if (ToggleViewPMappingCount != 1)
		{
			OutReason = FString::Printf(TEXT("ToggleView/P mapping appears %d times, expected exactly one"), ToggleViewPMappingCount);
			return EAcquireResult::Fatal;
		}

		const TArray<const UInputAction*> ExpectedActions{Move, Altitude, Yaw, Look, CameraPitchRate, ToggleView};
		int32 PrototypeEffectiveCount = 0;
		for (const FEnhancedActionKeyMapping& Effective : EffectiveMappings)
		{
			if (ExpectedActions.Contains(Effective.Action.Get()))
			{
				++PrototypeEffectiveCount;
			}
		}

		if (PrototypeEffectiveCount < ExpectedMappingCount)
		{
			OutReason = FString::Printf(
				TEXT("effective Prototype mappings are still rebuilding (%d/%d)"),
				PrototypeEffectiveCount,
				ExpectedMappingCount);
			return EAcquireResult::Wait;
		}

		if (PrototypeEffectiveCount != ExpectedMappingCount || EffectiveMappings.Num() != ExpectedMappingCount)
		{
			OutReason = FString::Printf(
				TEXT("effective mappings are Prototype=%d, total=%d; expected %d and %d"),
				PrototypeEffectiveCount,
				EffectiveMappings.Num(),
				ExpectedMappingCount,
				ExpectedMappingCount);
			return EAcquireResult::Fatal;
		}

		for (const FEnhancedActionKeyMapping& Source : SourceMappings)
		{
			int32 MatchCount = 0;
			for (const FEnhancedActionKeyMapping& Effective : EffectiveMappings)
			{
				if (Effective.Action == Source.Action && Effective.Key == Source.Key)
				{
					++MatchCount;
				}
			}

			if (MatchCount != 1)
			{
				OutReason = FString::Printf(
					TEXT("effective mapping %s/%s appears %d times"),
					*GetNameSafe(Source.Action.Get()),
					*Source.Key.ToString(),
					MatchCount);
				return MatchCount == 0 ? EAcquireResult::Wait : EAcquireResult::Fatal;
			}
		}

		for (const UInputAction* Action : ExpectedActions)
		{
			int32 ActionBindingCount = 0;
			int32 TriggeredBindingCount = 0;
			int32 StartedBindingCount = 0;
			int32 CompletedBindingCount = 0;
			int32 CanceledBindingCount = 0;
			bool bAllBindingsOwnedByPawn = true;
			for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : InputComponent->GetActionEventBindings())
			{
				if (Binding && Binding->GetAction() == Action)
				{
					++ActionBindingCount;
					bAllBindingsOwnedByPawn &= Binding->GetUObject() == FoundPawn;
					switch (Binding->GetTriggerEvent())
					{
					case ETriggerEvent::Started:
						++StartedBindingCount;
						break;
					case ETriggerEvent::Triggered:
						++TriggeredBindingCount;
						break;
					case ETriggerEvent::Completed:
						++CompletedBindingCount;
						break;
					case ETriggerEvent::Canceled:
						++CanceledBindingCount;
						break;
					default:
						break;
					}
				}
			}

			if (ActionBindingCount == 0)
			{
				OutReason = FString::Printf(TEXT("binding for %s is not ready"), *GetNameSafe(Action));
				return EAcquireResult::Wait;
			}

			const bool bIsMoveAction = Action == Move;
			const bool bIsToggleViewAction = Action == ToggleView;
			const int32 ExpectedActionBindingCount = bIsMoveAction ? 3 : 1;
			const int32 ExpectedTriggeredBindingCount = bIsToggleViewAction ? 0 : 1;
			const int32 ExpectedStartedBindingCount = bIsToggleViewAction ? 1 : 0;
			const bool bMoveReleaseBindingsAreValid = !bIsMoveAction
				|| (CompletedBindingCount == 1 && CanceledBindingCount == 1);
			if (ActionBindingCount != ExpectedActionBindingCount
				|| TriggeredBindingCount != ExpectedTriggeredBindingCount
				|| StartedBindingCount != ExpectedStartedBindingCount
				|| !bMoveReleaseBindingsAreValid
				|| !bAllBindingsOwnedByPawn)
			{
				OutReason = FString::Printf(
					TEXT("%s bindings are total=%d Started=%d Triggered=%d Completed=%d Canceled=%d; expected Pawn-owned %d/%d/%d/%d/%d"),
					*GetNameSafe(Action),
					ActionBindingCount,
					StartedBindingCount,
					TriggeredBindingCount,
					CompletedBindingCount,
					CanceledBindingCount,
					ExpectedActionBindingCount,
					ExpectedStartedBindingCount,
					ExpectedTriggeredBindingCount,
					bIsMoveAction ? 1 : 0,
					bIsMoveAction ? 1 : 0);
				return EAcquireResult::Fatal;
			}
		}

		World = WorldObject;
		Pawn = FoundPawn;
		Controller = FoundController;
		FlightHUD = FoundFlightHUD;
		Telemetry = FoundTelemetry;
		PlayerInput = FoundInput;
		LifecycleState->Capture(Subsystem, IMC, FoundController, FoundFlightHUD, FoundTelemetry);
		InitialPawnTransform = FoundPawn->GetActorTransform();
		InitialControlRotation = FoundController->GetControlRotation();
		InitialCameraBoomRotation = FoundPawn->GetCameraBoom()->GetRelativeRotation();
		return EAcquireResult::Ready;
	}

	void BuildProbes()
	{
		Probes = {
			{EKeys::W, TEXT("W / forward"), EProbeEffect::Forward, +1.0f, false},
			{EKeys::S, TEXT("S / backward"), EProbeEffect::Forward, -1.0f, false},
			{EKeys::A, TEXT("A / left"), EProbeEffect::Right, -1.0f, false},
			{EKeys::D, TEXT("D / right"), EProbeEffect::Right, +1.0f, false},
			{EKeys::SpaceBar, TEXT("SpaceBar / up"), EProbeEffect::Up, +1.0f, false},
			{EKeys::LeftControl, TEXT("LeftControl / down"), EProbeEffect::Up, -1.0f, false},
			{EKeys::E, TEXT("E / actor yaw positive"), EProbeEffect::ActorYaw, +1.0f, false},
			{EKeys::Q, TEXT("Q / actor yaw negative"), EProbeEffect::ActorYaw, -1.0f, false},
			{EKeys::MouseX, TEXT("MouseX / actor yaw"), EProbeEffect::ActorYaw, +1.0f, true},
			{EKeys::MouseY, TEXT("MouseY / camera pitch"), EProbeEffect::CameraPitch, +1.0f, true},
			{EKeys::Gamepad_LeftY, TEXT("Gamepad LeftY / forward"), EProbeEffect::Forward, +1.0f, true},
			{EKeys::Gamepad_LeftX, TEXT("Gamepad LeftX / right"), EProbeEffect::Right, +1.0f, true},
			{EKeys::Gamepad_RightTriggerAxis, TEXT("Gamepad RT / up"), EProbeEffect::Up, +1.0f, true},
			{EKeys::Gamepad_LeftTriggerAxis, TEXT("Gamepad LT / down"), EProbeEffect::Up, -1.0f, true},
			{EKeys::Gamepad_RightX, TEXT("Gamepad RightX / actor yaw"), EProbeEffect::ActorYaw, +1.0f, true},
			{EKeys::Gamepad_RightY, TEXT("Gamepad RightY / camera pitch"), EProbeEffect::CameraPitch, +1.0f, true},
		};
	}

	void BuildCombinationProbes()
	{
		CombinationProbes = {
			{EKeys::W, EKeys::SpaceBar, TEXT("W+Space / forward and up"), ECombinationExpectation::ForwardAndUp},
			{EKeys::D, EKeys::E, TEXT("D+E / right and actor yaw"), ECombinationExpectation::RightAndActorYaw},
			{EKeys::W, EKeys::S, TEXT("W+S / opposing forward input"), ECombinationExpectation::OpposeForward},
			{EKeys::A, EKeys::D, TEXT("A+D / opposing right input"), ECombinationExpectation::OpposeRight},
		};
	}

	static void SendProbe(const FProbe& Probe, UEnhancedPlayerInput& Input)
	{
		Input.InputKey(FInputKeyEventArgs::CreateSimulated(
			Probe.Key,
			Probe.bAnalog ? IE_Axis : IE_Pressed,
			1.0f,
			Probe.bAnalog ? 1 : -1));
	}

	static void ReleaseProbe(const FProbe& Probe, UEnhancedPlayerInput& Input)
	{
		if (Probe.bAnalog)
		{
			Input.InputKey(FInputKeyEventArgs::CreateSimulated(Probe.Key, IE_Axis, 0.0f, 1));
		}
		else
		{
			Input.InputKey(FInputKeyEventArgs::CreateSimulated(Probe.Key, IE_Released, 0.0f));
		}
	}

	static float NormalizeEffectStrength(
		const EProbeEffect Effect,
		const float SignedEffect,
		const double ElapsedWorldSeconds,
		const int32 /*AnalogSampleCount*/)
	{
		const float Elapsed = FMath::Max(static_cast<float>(ElapsedWorldSeconds), UE_SMALL_NUMBER);
		switch (Effect)
		{
		case EProbeEffect::Forward:
		case EProbeEffect::Right:
		case EProbeEffect::Up:
			return SignedEffect / FMath::Square(Elapsed);

		case EProbeEffect::ActorYaw:
			return SignedEffect / Elapsed;

		case EProbeEffect::CameraPitch:
			return SignedEffect / Elapsed;
		}

		return 0.0f;
	}

	bool ResetForCombination(
		ADronePrototypePawn& CurrentPawn,
		APlayerController& CurrentController,
		UEnhancedPlayerInput& CurrentInput,
		FString& OutError)
	{
		if (UFloatingPawnMovement* Movement = CurrentPawn.GetPrototypeMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
		CurrentInput.FlushPressedKeys();
		CurrentPawn.ConsumeMovementInputVector();
		if (!CurrentPawn.SetActorTransform(InitialPawnTransform, false, nullptr, ETeleportType::TeleportPhysics))
		{
			OutError = FString::Printf(
				TEXT("%s: could not reset Pawn transform"),
				CombinationProbes[CombinationIndex].Label);
			return false;
		}
		CurrentController.SetControlRotation(InitialControlRotation);
		if (USpringArmComponent* CameraBoom = CurrentPawn.GetCameraBoom())
		{
			CameraBoom->SetRelativeRotation(InitialCameraBoomRotation);
		}
		return true;
	}

	void BeginCombination(
		const FCombinationProbe& Combination,
		ADronePrototypePawn& CurrentPawn,
		APlayerController& CurrentController,
		UEnhancedPlayerInput& CurrentInput,
		const double Now)
	{
		BaselineLocation = CurrentPawn.GetActorLocation();
		BaselineActorRotation = CurrentPawn.GetActorRotation();
		BaselineControlRotation = CurrentController.GetControlRotation();
		BaselineCameraBoomRotation = CurrentPawn.GetCameraBoom()->GetRelativeRotation();
		CombinationStartedWorldSeconds = CurrentPawn.GetWorld()->GetTimeSeconds();
		CombinationStartedAt = Now;
		CurrentInput.InputKey(FInputKeyEventArgs::CreateSimulated(Combination.FirstKey, IE_Pressed, 1.0f));
		CurrentInput.InputKey(FInputKeyEventArgs::CreateSimulated(Combination.SecondKey, IE_Pressed, 1.0f));
	}

	bool UpdateCombination(
		const FCombinationProbe& Combination,
		ADronePrototypePawn& CurrentPawn,
		APlayerController& CurrentController,
		UEnhancedPlayerInput& CurrentInput,
		const double Now)
	{
		const double ElapsedWorldSeconds =
			CurrentPawn.GetWorld()->GetTimeSeconds() - CombinationStartedWorldSeconds;
		if (ElapsedWorldSeconds < EffectSampleSeconds)
		{
			if (Now - CombinationStartedAt > 2.0)
			{
				ReleaseCombination(Combination, CurrentInput);
				return Fail(FString::Printf(TEXT("%s sample did not advance PIE time"), Combination.Label));
			}
			return false;
		}

		ReleaseCombination(Combination, CurrentInput);

		const FVector Translation = CurrentPawn.GetActorLocation() - BaselineLocation;
		const float Forward = FVector::DotProduct(Translation, BaselineActorRotation.Vector());
		const float Right = FVector::DotProduct(
			Translation,
			FRotationMatrix(BaselineActorRotation).GetScaledAxis(EAxis::Y));
		const float Up = Translation.Z;
		const float ActorYaw =
			FMath::FindDeltaAngleDegrees(BaselineActorRotation.Yaw, CurrentPawn.GetActorRotation().Yaw);
		const FRotator ControlDelta =
			(CurrentController.GetControlRotation() - BaselineControlRotation).GetNormalized();

		switch (Combination.Expectation)
		{
		case ECombinationExpectation::ForwardAndUp:
			if (Forward <= TranslationTolerance || Up <= TranslationTolerance)
			{
				return Fail(FString::Printf(
					TEXT("%s did not produce both effects (forward %.6f, up %.6f)"),
					Combination.Label,
					Forward,
					Up));
			}
			if (!CurrentPawn.GetActorRotation().Equals(BaselineActorRotation, 0.05f)
				|| !ControlDelta.IsNearlyZero(0.05f))
			{
				return Fail(FString::Printf(TEXT("%s changed an unrelated rotation"), Combination.Label));
			}
			CycleStrengths.Add(
				FName(TEXT("W+Space / forward")),
				NormalizeEffectStrength(EProbeEffect::Forward, Forward, ElapsedWorldSeconds, 0));
			CycleStrengths.Add(
				FName(TEXT("W+Space / up")),
				NormalizeEffectStrength(EProbeEffect::Up, Up, ElapsedWorldSeconds, 0));
			break;

		case ECombinationExpectation::RightAndActorYaw:
			if (Right <= TranslationTolerance || ActorYaw <= RotationTolerance)
			{
				return Fail(FString::Printf(
					TEXT("%s did not produce both effects (right %.6f, actor yaw %.6f)"),
					Combination.Label,
					Right,
					ActorYaw));
			}
			if (!ControlDelta.IsNearlyZero(0.05f))
			{
				return Fail(FString::Printf(
					TEXT("%s changed control rotation by %s"),
					Combination.Label,
					*ControlDelta.ToCompactString()));
			}
			CycleStrengths.Add(
				FName(TEXT("D+E / right")),
				NormalizeEffectStrength(EProbeEffect::Right, Right, ElapsedWorldSeconds, 0));
			CycleStrengths.Add(
				FName(TEXT("D+E / actor yaw")),
				NormalizeEffectStrength(EProbeEffect::ActorYaw, ActorYaw, ElapsedWorldSeconds, 0));
			break;

		case ECombinationExpectation::OpposeForward:
		case ECombinationExpectation::OpposeRight:
		{
			const bool bForwardAxis = Combination.Expectation == ECombinationExpectation::OpposeForward;
			const float OpposingAxis = bForwardAxis
				? Forward
				: Right;
			const float UnrelatedHorizontalAxis = bForwardAxis ? Right : Forward;
			const FName FirstSingleInput = bForwardAxis ? FName(TEXT("W / forward")) : FName(TEXT("A / left"));
			const FName SecondSingleInput = bForwardAxis ? FName(TEXT("S / backward")) : FName(TEXT("D / right"));
			const float* FirstSingleStrength = CycleStrengths.Find(FirstSingleInput);
			const float* SecondSingleStrength = CycleStrengths.Find(SecondSingleInput);
			if (!FirstSingleStrength || !SecondSingleStrength
				|| *FirstSingleStrength <= UE_SMALL_NUMBER || *SecondSingleStrength <= UE_SMALL_NUMBER)
			{
				return Fail(FString::Printf(TEXT("%s could not compare its opposing input strength"), Combination.Label));
			}

			const float OpposingStrength = NormalizeEffectStrength(
				bForwardAxis ? EProbeEffect::Forward : EProbeEffect::Right,
				FMath::Abs(OpposingAxis),
				ElapsedWorldSeconds,
				0);
			const float MaximumSingleStrength = FMath::Max(*FirstSingleStrength, *SecondSingleStrength);
			const float StrengthRatio = OpposingStrength / MaximumSingleStrength;
			if (!FMath::IsFinite(StrengthRatio)
				|| StrengthRatio > MaximumOpposingInputStrengthRatio
				|| FMath::Abs(UnrelatedHorizontalAxis) > TranslationTolerance
				|| FMath::Abs(Up) > TranslationTolerance
				|| FMath::Abs(ActorYaw) > 0.05f
				|| !ControlDelta.IsNearlyZero(0.05f))
			{
				return Fail(FString::Printf(
					TEXT("%s produced an invalid opposing-input effect (axis %.6f, ratio %.3f, translation %s, actor yaw %.6f, control %s)"),
					Combination.Label,
					OpposingAxis,
					StrengthRatio,
					*Translation.ToCompactString(),
					ActorYaw,
					*ControlDelta.ToCompactString()));
			}
			break;
		}
		}

		++CombinationIndex;
		Phase = EPhase::CombinationReset;
		return false;
	}

	static void ReleaseCombination(
		const FCombinationProbe& Combination,
		UEnhancedPlayerInput& CurrentInput)
	{
		CurrentInput.InputKey(FInputKeyEventArgs::CreateSimulated(Combination.FirstKey, IE_Released, 0.0f));
		CurrentInput.InputKey(FInputKeyEventArgs::CreateSimulated(Combination.SecondKey, IE_Released, 0.0f));
	}

	float MeasureSignedEffect(
		const FProbe& Probe,
		const ADronePrototypePawn& CurrentPawn,
		const APlayerController& CurrentController) const
	{
		float Raw = 0.0f;
		switch (Probe.Effect)
		{
		case EProbeEffect::Forward:
			Raw = FVector::DotProduct(CurrentPawn.GetActorLocation() - BaselineLocation, BaselineActorRotation.Vector());
			break;
		case EProbeEffect::Right:
			Raw = FVector::DotProduct(
				CurrentPawn.GetActorLocation() - BaselineLocation,
				FRotationMatrix(BaselineActorRotation).GetScaledAxis(EAxis::Y));
			break;
		case EProbeEffect::Up:
			Raw = CurrentPawn.GetActorLocation().Z - BaselineLocation.Z;
			break;
		case EProbeEffect::ActorYaw:
			Raw = FMath::FindDeltaAngleDegrees(BaselineActorRotation.Yaw, CurrentPawn.GetActorRotation().Yaw);
			break;
		case EProbeEffect::CameraPitch:
			Raw = FMath::FindDeltaAngleDegrees(
				BaselineCameraBoomRotation.Pitch,
				CurrentPawn.GetCameraBoom()->GetRelativeRotation().Pitch);
			break;
		}

		return Raw * Probe.ExpectedSign;
	}

	bool CheckRotationIsolation(
		const FProbe& Probe,
		const ADronePrototypePawn& CurrentPawn,
		const APlayerController& CurrentController,
		FString& OutError) const
	{
		constexpr float IsolationRotationTolerance = 0.05f;
		if (Probe.Effect == EProbeEffect::ActorYaw)
		{
			const float ControlDelta = FMath::Abs(
				FMath::FindDeltaAngleDegrees(BaselineControlRotation.Yaw, CurrentController.GetControlRotation().Yaw));
			if (ControlDelta > IsolationRotationTolerance)
			{
				OutError = FString::Printf(
					TEXT("%s changed control yaw by %.4f; expected actor yaw only"),
					Probe.Label,
					ControlDelta);
				return false;
			}
		}

		if (Probe.Effect == EProbeEffect::CameraPitch)
		{
			const FRotator ActorDelta = (CurrentPawn.GetActorRotation() - BaselineActorRotation).GetNormalized();
			const FRotator ControlDelta = (CurrentController.GetControlRotation() - BaselineControlRotation).GetNormalized();
			if (!ActorDelta.IsNearlyZero(IsolationRotationTolerance)
				|| !ControlDelta.IsNearlyZero(IsolationRotationTolerance))
			{
				OutError = FString::Printf(
					TEXT("%s changed actor/control rotation (%s / %s); expected camera boom pitch only"),
					Probe.Label,
					*ActorDelta.ToCompactString(),
					*ControlDelta.ToCompactString());
				return false;
			}
		}

		return true;
	}

	bool Fail(const FString& Message)
	{
		if (UEnhancedPlayerInput* CurrentInput = PlayerInput.Get())
		{
			CurrentInput->FlushPressedKeys();
		}
		Test->AddError(FString::Printf(TEXT("[fresh PIE %d/3] %s"), Cycle, *Message));
		return true; // Continue to queued EndPIE and teardown commands.
	}

	FAutomationTestBase* Test;
	int32 Cycle;
	TSharedRef<FEffectStrengthHistory> StrengthHistory;
	TSharedRef<FPIECycleLifecycleState> LifecycleState;
	bool bActivated = false;
	double ActivatedAt = 0.0;
	double ProbeStartedAt = 0.0;
	double ProbeStartedWorldSeconds = 0.0;
	double CombinationStartedAt = 0.0;
	double CombinationStartedWorldSeconds = 0.0;
	EPhase Phase = EPhase::Acquire;
	int32 FramesToSettle = 0;
	int32 ProbeIndex = 0;
	int32 CombinationIndex = 0;
	int32 AnalogDispatchCount = 0;
	TArray<FProbe> Probes;
	TArray<FCombinationProbe> CombinationProbes;
	TMap<FName, float> CycleStrengths;
	bool bHUDPossessionCycleCompleted = false;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ADronePrototypePawn> Pawn;
	TWeakObjectPtr<ADronePrototypePlayerController> Controller;
	TWeakObjectPtr<UDroneFlightHUDWidget> FlightHUD;
	TWeakObjectPtr<UDroneTelemetryComponent> Telemetry;
	TWeakObjectPtr<UEnhancedPlayerInput> PlayerInput;
	FTransform InitialPawnTransform;
	FRotator InitialControlRotation;
	FRotator InitialCameraBoomRotation;
	FVector BaselineLocation = FVector::ZeroVector;
	FRotator BaselineActorRotation = FRotator::ZeroRotator;
	FRotator BaselineControlRotation = FRotator::ZeroRotator;
	FRotator BaselineCameraBoomRotation = FRotator::ZeroRotator;
};

class FWaitForPIETeardownCommand final : public IAutomationLatentCommand
{
public:
	FWaitForPIETeardownCommand(
		FAutomationTestBase* InTest,
		const int32 InCycle,
		const TSharedRef<FPIECycleLifecycleState>& InLifecycleState)
		: Test(InTest)
		, Cycle(InCycle)
		, LifecycleState(InLifecycleState)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (!bActivated)
		{
			bActivated = true;
			ActivatedAt = Now;
		}

		const bool bSessionActive = GEditor && GEditor->IsPlaySessionInProgress();
		const bool bPlayWorldAlive = GEditor && GEditor->PlayWorld != nullptr;
		if (!bSessionActive && !bPlayWorldAlive && !HasPIEWorld())
		{
			if (LifecycleState->IsMappingContextStillApplied())
			{
				Test->AddError(FString::Printf(
					TEXT("[fresh PIE %d/3] IMC_DronePrototype remained applied after PIE teardown"),
					Cycle));
			}

			FString HUDLifecycleReason;
			if (LifecycleState->IsHUDLifecycleStillBound(HUDLifecycleReason))
			{
				Test->AddError(FString::Printf(
					TEXT("[fresh PIE %d/3] %s"),
					Cycle,
					*HUDLifecycleReason));
			}
			return true;
		}

		if (!bForcedCleanup && Now - ActivatedAt > 10.0)
		{
			bForcedCleanup = true;
			Test->AddError(FString::Printf(
				TEXT("[fresh PIE %d/3] PIE teardown timed out; forcing EndPlayMap"),
				Cycle));
			if (GEditor)
			{
				GEditor->EndPlayMap();
			}
		}

		if (Now - ActivatedAt > 30.0)
		{
			Test->AddError(FString::Printf(
				TEXT("[fresh PIE %d/3] PIE world survived forced teardown; stopping this lifecycle test"),
				Cycle));
			return true;
		}

		return false;
	}

private:
	FAutomationTestBase* Test;
	int32 Cycle;
	TSharedRef<FPIECycleLifecycleState> LifecycleState;
	bool bActivated = false;
	bool bForcedCleanup = false;
	double ActivatedAt = 0.0;
};
} // namespace DronePrototypePIEInputLifecycle

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePrototypePIEInputLifecycleTest,
	"Drone.Prototype.PIEInputLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDronePrototypePIEInputLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace DronePrototypePIEInputLifecycle;

	if (!GEditor)
	{
		AddError(TEXT("GEditor is unavailable"));
		return false;
	}

	if (GEditor->IsPlaySessionInProgress() || HasPIEWorld())
	{
		AddError(TEXT("Test requires no pre-existing PIE session"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapPackage));
		return false;
	}

	const TSharedRef<FEffectStrengthHistory> StrengthHistory = MakeShared<FEffectStrengthHistory>();
	for (int32 Cycle = 1; Cycle <= 3; ++Cycle)
	{
		const TSharedRef<FPIECycleLifecycleState> LifecycleState = MakeShared<FPIECycleLifecycleState>();
		FRequestPlaySessionParams Params = MakePlayParams();
		ADD_LATENT_AUTOMATION_COMMAND(FLazyStartPIEForAutomationCommand(MoveTemp(Params)));
		ADD_LATENT_AUTOMATION_COMMAND(FValidatePIEInputCommand(this, Cycle, StrengthHistory, LifecycleState));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIETeardownCommand(this, Cycle, LifecycleState));
	}

	return true;
}

#endif
