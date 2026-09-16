#include "Prototype/DronePrototypePawn.h"

#include "Camera/CameraComponent.h"
#include "AI/DroneNPCAIController.h"
#include "Abilities/DroneImpactDetonationComponent.h"
#include "Abilities/DronePayloadDropComponent.h"
#include "Abilities/DroneReconScanComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Drone.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Health/DroneHealthComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Mission/DroneDefinition.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Telemetry/DroneTelemetryComponent.h"
#include "Signal/DroneSignalComponent.h"
#include "Weather/DroneWeatherResponseComponent.h"

ADronePrototypePawn::ADronePrototypePawn()
{
	// 이동은 UFloatingPawnMovement가 처리하고 Pawn Tick은 외형 Roll 보간에만 사용한다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Collision Root와 Visual Mesh를 분리하면 나중에 Mesh 크기가 달라도 이동 구조를 유지할 수 있다.
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(45.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Pawn"));
	CollisionComponent->SetSimulatePhysics(false);
	CollisionComponent->SetCanEverAffectNavigation(false);
	SetRootComponent(CollisionComponent);

	VisualTiltPivot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualTiltPivot"));
	VisualTiltPivot->SetupAttachment(CollisionComponent);

	PayloadCarryAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("PayloadCarryAnchor"));
	PayloadCarryAnchor->SetupAttachment(VisualTiltPivot);
	PayloadCarryAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, -65.0f));

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshComponent"));
	VisualMeshComponent->SetupAttachment(VisualTiltPivot);
	VisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMeshComponent->SetSimulatePhysics(false);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CollisionComponent);
	CameraBoom->TargetArmLength = 500.0f;
	CameraBoom->bUsePawnControlRotation = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	PrototypeMovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("PrototypeMovementComponent"));
	PrototypeMovementComponent->SetUpdatedComponent(CollisionComponent);
	PrototypeMovementComponent->MaxSpeed = 1200.0f;
	PrototypeMovementComponent->Acceleration = 2400.0f;
	PrototypeMovementComponent->Deceleration = 3000.0f;
	PrototypeMovementComponent->TurningBoost = 8.0f;

	// 쉬운 조작은 기존 반응을 그대로 유지한다. 실제 조작형은 자동 제동과 선회 보조를 줄이고
	// Root 자세를 실제 이동 축에 반영한다. 이는 모터별 추력 물리가 아닌 1차 조작 검증값이다.
	ManualRealisticGreyboxTuning.AccelerationMultiplier = 0.85f;
	ManualRealisticGreyboxTuning.DecelerationMultiplier = 0.20f;
	ManualRealisticGreyboxTuning.TurningBoostMultiplier = 0.35f;
	ManualRealisticGreyboxTuning.YawRateMultiplier = 0.85f;
	ManualRealisticGreyboxTuning.bUseLocalAltitudeAxis = true;
	ManualRealisticGreyboxTuning.bTiltCollisionRoot = true;

	// Rate/Acro는 자세 제한·자동 수평 복귀를 사용하지 않는다. UFloatingPawnMovement 기반이라
	// 감속을 완전히 끄지는 않되 관성이 남도록 보조 감속과 TurningBoost를 크게 낮춘다.
	AcroRateRealisticGreyboxTuning.AccelerationMultiplier = 1.15f;
	AcroRateRealisticGreyboxTuning.DecelerationMultiplier = 0.08f;
	AcroRateRealisticGreyboxTuning.TurningBoostMultiplier = 0.0f;
	AcroRateRealisticGreyboxTuning.YawRateMultiplier = 1.0f;
	AcroRateRealisticGreyboxTuning.bUseLocalAltitudeAxis = true;
	AcroRateRealisticGreyboxTuning.bTiltCollisionRoot = true;

	StableHandlingTuning.MaxSpeedMultiplier = 0.80f;
	StableHandlingTuning.AccelerationMultiplier = 0.75f;
	StableHandlingTuning.YawRateMultiplier = 0.75f;
	StableHandlingTuning.AttitudeLimitMultiplier = 0.65f;

	AgileHandlingTuning.MaxSpeedMultiplier = 1.25f;
	AgileHandlingTuning.AccelerationMultiplier = 1.30f;
	AgileHandlingTuning.YawRateMultiplier = 1.35f;
	AgileHandlingTuning.AttitudeLimitMultiplier = 1.25f;

	// HUD가 Pawn을 직접 계산하지 않도록 공용 데이터 공급 Component를 기본 부착한다.
	TelemetryComponent = CreateDefaultSubobject<UDroneTelemetryComponent>(TEXT("TelemetryComponent"));
	HealthComponent = CreateDefaultSubobject<UDroneHealthComponent>(TEXT("HealthComponent"));
	SignalComponent = CreateDefaultSubobject<UDroneSignalComponent>(TEXT("SignalComponent"));
	WeatherResponseComponent = CreateDefaultSubobject<UDroneWeatherResponseComponent>(TEXT("WeatherResponseComponent"));
	ReconScanComponent = CreateDefaultSubobject<UDroneReconScanComponent>(TEXT("ReconScanComponent"));
	ImpactDetonationComponent = CreateDefaultSubobject<UDroneImpactDetonationComponent>(TEXT("ImpactDetonationComponent"));
	PayloadDropComponent = CreateDefaultSubobject<UDronePayloadDropComponent>(TEXT("PayloadDropComponent"));

	// AI Perception의 전역 Pawn 자동 등록 설정에 의존하지 않고 Sight 대상으로 명시한다.
	PerceptionStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuliSource"));
}

bool ADronePrototypePawn::ApplyDroneDefinition(const UDroneDefinition* Definition)
{
	FString ValidationError;
	if (!IsValid(Definition) || !Definition->ValidateDefinition(ValidationError))
	{
		UE_LOG(
			LogDrone,
			Warning,
			TEXT("Prototype pawn '%s' rejected Drone Definition: %s"),
			*GetNameSafe(this),
			ValidationError.IsEmpty() ? TEXT("Definition이 없습니다.") : *ValidationError);
		return false;
	}

	const FDroneFlightProfile& Profile = bOverrideDefinitionFlightProfileInBlueprint
		? BlueprintFlightProfileOverride
		: Definition->FlightProfile;
	FString ProfileValidationError;
	if (!Profile.ValidateProfile(ProfileValidationError))
	{
		UE_LOG(
			LogDrone,
			Warning,
			TEXT("Prototype pawn '%s' rejected its Blueprint Flight Profile override: %s"),
			*GetNameSafe(this),
			*ProfileValidationError);
		return false;
	}
	BaseMaxSpeedCentimetersPerSecond = Profile.MaxSpeedCentimetersPerSecond;
	BaseAccelerationCentimetersPerSecondSquared = Profile.AccelerationCentimetersPerSecondSquared;
	BaseDecelerationCentimetersPerSecondSquared = Profile.DecelerationCentimetersPerSecondSquared;
	BaseTurningBoost = Profile.TurningBoost;
	BaseYawRateDegreesPerSecond = Profile.YawRateDegreesPerSecond;
	BaseMaximumVisualBankRollDegrees = Profile.MaximumVisualBankRollDegrees;
	BaseMaximumVisualTiltPitchDegrees = Profile.MaximumVisualTiltPitchDegrees;
	ActiveAcroRateSettings = Profile.AcroRateSettings;
	CurrentControlMode = Profile.DefaultControlMode;
	CurrentHandlingPreset = Profile.DefaultHandlingPreset;
	AcroThrottleInput = 0.0f;
	CurrentAcroBodyRateDegreesPerSecond = FRotator::ZeroRotator;
	ApplyRuntimeFlightTuning();
	OnFlightControlSettingsChanged.Broadcast(CurrentControlMode, CurrentHandlingPreset);
	bStartInFirstPersonView = Profile.bStartInFirstPersonView;
	SetFirstPersonViewEnabled(Profile.bStartInFirstPersonView);
	HealthComponent->ConfigureMaxHealth(Profile.MaxHealth, true);
	// 기획된 기능이 아니라 현재 빌드에서 검증 완료된 기능만 활성화해 역할 중첩을 막는다.
	ReconScanComponent->ConfigureFeatureEnabled(
		Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ReconScan));
	ImpactDetonationComponent->ConfigureFeatureEnabled(
		Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::ImpactDetonation));
	PayloadDropComponent->ConfigureFeatureEnabled(
		Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::PayloadDrop));
	SignalComponent->ConfigureJammingImmunity(
		Definition->ImplementedCapabilities.Contains(EDroneGameplayCapability::JammingImmunity));
	AppliedDroneId = Definition->DroneId;

	UE_LOG(
		LogDrone,
		Display,
		TEXT("Applied Drone Definition '%s' to '%s' (speed %.0f, yaw %.0f, health %.0f, control %d, handling %d)."),
		*AppliedDroneId.ToString(),
		*GetNameSafe(this),
		Profile.MaxSpeedCentimetersPerSecond,
		Profile.YawRateDegreesPerSecond,
		Profile.MaxHealth,
		static_cast<uint8>(CurrentControlMode),
		static_cast<uint8>(CurrentHandlingPreset));
	return true;
}

bool ADronePrototypePawn::TriggerPrimaryRoleAbility()
{
	if ((HealthComponent && HealthComponent->IsDead()) || AppliedDroneId.IsNone())
	{
		return false;
	}

	// 현재 세 역할은 Data Asset Validation에서 상호 배타적으로 활성화된다.
	if (ReconScanComponent && ReconScanComponent->IsFeatureEnabled())
	{
		return ReconScanComponent->StartBestAvailableScan();
	}
	if (ImpactDetonationComponent && ImpactDetonationComponent->IsFeatureEnabled())
	{
		return ImpactDetonationComponent->ArmImpactDetonation();
	}
	if (PayloadDropComponent && PayloadDropComponent->IsFeatureEnabled())
	{
		return PayloadDropComponent->ActivatePrimaryPayloadAction();
	}
	return false;
}

bool ADronePrototypePawn::TriggerSecondaryRoleAbility()
{
	if ((HealthComponent && HealthComponent->IsDead()) || AppliedDroneId.IsNone())
	{
		return false;
	}

	if (ReconScanComponent && ReconScanComponent->IsFeatureEnabled())
	{
		const bool bWasScanning = ReconScanComponent->IsScanning();
		ReconScanComponent->CancelScan();
		return bWasScanning;
	}
	if (ImpactDetonationComponent && ImpactDetonationComponent->IsFeatureEnabled())
	{
		const bool bWasArmed = ImpactDetonationComponent->IsArmed();
		ImpactDetonationComponent->DisarmImpactDetonation();
		return bWasArmed && !ImpactDetonationComponent->IsArmed();
	}
	if (PayloadDropComponent && PayloadDropComponent->IsFeatureEnabled())
	{
		const bool bRequestedEnabled = !IsDropCameraViewEnabled();
		PayloadDropComponent->SetDropViewEnabled(bRequestedEnabled);
		return IsDropCameraViewEnabled() == bRequestedEnabled;
	}
	return false;
}

void ADronePrototypePawn::SetControlMode(const EDroneControlMode NewControlMode)
{
	const bool bChanged = CurrentControlMode != NewControlMode;
	if (bChanged)
	{
		// 서로 의미가 다른 Action 축의 이전 프레임 값을 새 모드로 넘기지 않는다.
		VisualTiltForwardInput = 0.0f;
		VisualBankLateralInput = 0.0f;
		AcroYawInput = 0.0f;
		AcroThrottleInput = 0.0f;
		CurrentAcroBodyRateDegreesPerSecond = FRotator::ZeroRotator;
	}
	CurrentControlMode = NewControlMode;
	ApplyRuntimeFlightTuning();
	if (bChanged)
	{
		OnFlightControlSettingsChanged.Broadcast(CurrentControlMode, CurrentHandlingPreset);
	}
}

void ADronePrototypePawn::ToggleControlMode()
{
	switch (CurrentControlMode)
	{
	case EDroneControlMode::AssistedEasy:
		SetControlMode(EDroneControlMode::ManualRealisticGreybox);
		break;
	case EDroneControlMode::ManualRealisticGreybox:
		SetControlMode(EDroneControlMode::AcroRateRealisticGreybox);
		break;
	case EDroneControlMode::AcroRateRealisticGreybox:
	default:
		SetControlMode(EDroneControlMode::AssistedEasy);
		break;
	}
}

void ADronePrototypePawn::SetHandlingPreset(const EDroneHandlingPreset NewHandlingPreset)
{
	const bool bChanged = CurrentHandlingPreset != NewHandlingPreset;
	CurrentHandlingPreset = NewHandlingPreset;
	ApplyRuntimeFlightTuning();
	if (bChanged)
	{
		OnFlightControlSettingsChanged.Broadcast(CurrentControlMode, CurrentHandlingPreset);
	}
}

void ADronePrototypePawn::CycleHandlingPreset()
{
	switch (CurrentHandlingPreset)
	{
	case EDroneHandlingPreset::Stable:
		SetHandlingPreset(EDroneHandlingPreset::Balanced);
		break;
	case EDroneHandlingPreset::Balanced:
		SetHandlingPreset(EDroneHandlingPreset::Agile);
		break;
	case EDroneHandlingPreset::Agile:
	default:
		SetHandlingPreset(EDroneHandlingPreset::Stable);
		break;
	}
}

void ADronePrototypePawn::ApplyRuntimeFlightTuning()
{
	if (!PrototypeMovementComponent)
	{
		return;
	}

	const FDroneControlModeTuning& ControlTuning = ResolveCurrentControlModeTuning();

	const FDroneHandlingPresetTuning* HandlingTuning = &BalancedHandlingTuning;
	switch (CurrentHandlingPreset)
	{
	case EDroneHandlingPreset::Stable:
		HandlingTuning = &StableHandlingTuning;
		break;
	case EDroneHandlingPreset::Agile:
		HandlingTuning = &AgileHandlingTuning;
		break;
	case EDroneHandlingPreset::Balanced:
	default:
		break;
	}

	// 항상 Data Asset의 원본값에서 다시 계산하므로 모드를 반복 전환해도 배율이 누적되지 않는다.
	PrototypeMovementComponent->MaxSpeed =
		BaseMaxSpeedCentimetersPerSecond * HandlingTuning->MaxSpeedMultiplier
		* (SignalComponent ? SignalComponent->GetSnapshot().ControlResponseMultiplier : 1.0f);
	PrototypeMovementComponent->Acceleration =
		BaseAccelerationCentimetersPerSecondSquared
		* ControlTuning.AccelerationMultiplier
		* HandlingTuning->AccelerationMultiplier
		* (SignalComponent ? SignalComponent->GetSnapshot().ControlResponseMultiplier : 1.0f);
	// Acro는 아래의 속도 비례 항력만 사용한다. FloatingPawnMovement의 정속 감속까지 겹치면
	// 저속과 고속에서 감쇠 체감이 뒤틀리므로 해당 모드에서는 자동 감속을 끈다.
	PrototypeMovementComponent->Deceleration =
		CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox
			? 0.0f
			: BaseDecelerationCentimetersPerSecondSquared * ControlTuning.DecelerationMultiplier;
	PrototypeMovementComponent->TurningBoost =
		BaseTurningBoost * ControlTuning.TurningBoostMultiplier;
	PrototypeYawRateDegreesPerSecond =
		BaseYawRateDegreesPerSecond
		* ControlTuning.YawRateMultiplier
		* HandlingTuning->YawRateMultiplier;
	MaximumVisualBankRollDegrees =
		BaseMaximumVisualBankRollDegrees * HandlingTuning->AttitudeLimitMultiplier;
	MaximumVisualTiltPitchDegrees =
		BaseMaximumVisualTiltPitchDegrees * HandlingTuning->AttitudeLimitMultiplier;
}

const FDroneControlModeTuning& ADronePrototypePawn::ResolveCurrentControlModeTuning() const
{
	switch (CurrentControlMode)
	{
	case EDroneControlMode::ManualRealisticGreybox:
		return ManualRealisticGreyboxTuning;
	case EDroneControlMode::AcroRateRealisticGreybox:
		return AcroRateRealisticGreyboxTuning;
	case EDroneControlMode::AssistedEasy:
	default:
		return AssistedEasyTuning;
	}
}

void ADronePrototypePawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisualTiltAttachments();
	RefreshRotorVisualComponents();
	EnsureHealthFeedbackBindings();
}

void ADronePrototypePawn::BeginPlay()
{
	Super::BeginPlay();
	if (SignalComponent)
	{
		SignalComponent->OnSignalSnapshotChanged.AddUniqueDynamic(
			this, &ADronePrototypePawn::HandleSignalSnapshotChanged);
		ApplyRuntimeFlightTuning();
	}
	RefreshVisualTiltAttachments();
	RefreshRotorVisualComponents();
	EnsureHealthFeedbackBindings();
	bFirstPersonViewEnabled = bStartInFirstPersonView;
	ApplyCameraViewMode();

	// Actor가 유효한 World에 들어온 뒤 등록해야 Perception System이 실제 Source를 받을 수 있다.
	PerceptionStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	PerceptionStimuliSource->RegisterWithPerceptionSystem();
}

void ADronePrototypePawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	RefreshRotorVisualComponents();
	EnsureHealthFeedbackBindings();
}

void ADronePrototypePawn::EnsureHealthFeedbackBindings()
{
	if (HealthComponent)
	{
		HealthComponent->OnHealthChangedNative.RemoveAll(this);
		HealthComponent->OnHealthChangedNative.AddUObject(this, &ADronePrototypePawn::HandleHealthChanged);
		HealthComponent->OnDeath.AddUniqueDynamic(this, &ADronePrototypePawn::HandleDeath);
	}
}

void ADronePrototypePawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateDamageShake(DeltaSeconds);
	UpdateControlAttitude(DeltaSeconds);
	UpdateAcroFlightPhysics(DeltaSeconds);
	LimitAcroVerticalSpeed();
	UpdateVisualBank(DeltaSeconds);
	UpdateRotorVisuals(DeltaSeconds);
}

void ADronePrototypePawn::PawnClientRestart()
{
	Super::PawnClientRestart();
	// 로컬 클라이언트가 Possess/재시작될 때 IMC 적용을 시도한다. Subsystem이 없으면 안전하게 건너뛴다.
	ApplyPrototypeMappingContext();
}

void ADronePrototypePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	UE_LOG(LogDrone, Display, TEXT("Prototype pawn '%s' possessed by '%s'."), *GetNameSafe(this), *GetNameSafe(NewController));
}

void ADronePrototypePawn::UnPossessed()
{
	// 이전 Pawn의 입력이 새 Pawn과 겹치지 않도록 소유한 IMC부터 제거한다.
	RemovePrototypeMappingContext();
	Super::UnPossessed();
}

UPawnMovementComponent* ADronePrototypePawn::GetMovementComponent() const
{
	return PrototypeMovementComponent;
}

void ADronePrototypePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemovePrototypeMappingContext();
	if (SignalComponent)
	{
		SignalComponent->OnSignalSnapshotChanged.RemoveDynamic(
			this, &ADronePrototypePawn::HandleSignalSnapshotChanged);
	}
	ResetDamageShakePresentation();
	if (HealthComponent)
	{
		HealthComponent->OnHealthChangedNative.RemoveAll(this);
		HealthComponent->OnDeath.RemoveDynamic(this, &ADronePrototypePawn::HandleDeath);
	}
	Super::EndPlay(EndPlayReason);
}

void ADronePrototypePawn::HandleSignalSnapshotChanged(FDroneSignalSnapshot /*NewSnapshot*/)
{
	// 신호 변화 때 원본 Flight Profile에서 재계산한다. 반복 진입·이탈 시 감속 배율이 누적되지 않는다.
	ApplyRuntimeFlightTuning();
}

void ADronePrototypePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogDrone, Error, TEXT("Prototype pawn '%s' requires an Enhanced Input component."), *GetNameSafe(this));
		return;
	}

	// Asset이 연결된 Action만 Bind해 native CDO도 안전하게 생성·테스트할 수 있게 한다.
	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADronePrototypePawn::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADronePrototypePawn::ResetMoveVisualInput);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ADronePrototypePawn::ResetMoveVisualInput);
	}

	if (AltitudeAction)
	{
		EnhancedInputComponent->BindAction(AltitudeAction, ETriggerEvent::Triggered, this, &ADronePrototypePawn::ChangeAltitude);
	}

	if (YawAction)
	{
		EnhancedInputComponent->BindAction(YawAction, ETriggerEvent::Triggered, this, &ADronePrototypePawn::ChangeYaw);
		EnhancedInputComponent->BindAction(YawAction, ETriggerEvent::Completed, this, &ADronePrototypePawn::ResetYawInput);
		EnhancedInputComponent->BindAction(YawAction, ETriggerEvent::Canceled, this, &ADronePrototypePawn::ResetYawInput);
	}

	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADronePrototypePawn::Look);
	}

	if (CameraPitchRateAction)
	{
		EnhancedInputComponent->BindAction(
			CameraPitchRateAction,
			ETriggerEvent::Triggered,
			this,
			&ADronePrototypePawn::ChangeCameraPitch);
		EnhancedInputComponent->BindAction(
			CameraPitchRateAction,
			ETriggerEvent::Completed,
			this,
			&ADronePrototypePawn::ResetCameraPitchInput);
		EnhancedInputComponent->BindAction(
			CameraPitchRateAction,
			ETriggerEvent::Canceled,
			this,
			&ADronePrototypePawn::ResetCameraPitchInput);
	}

	if (AcroPitchAction)
	{
		EnhancedInputComponent->BindAction(AcroPitchAction, ETriggerEvent::Triggered, this, &ADronePrototypePawn::ChangeAcroPitch);
		EnhancedInputComponent->BindAction(AcroPitchAction, ETriggerEvent::Completed, this, &ADronePrototypePawn::ResetAcroPitchInput);
		EnhancedInputComponent->BindAction(AcroPitchAction, ETriggerEvent::Canceled, this, &ADronePrototypePawn::ResetAcroPitchInput);
	}

	if (AcroRollAction)
	{
		EnhancedInputComponent->BindAction(AcroRollAction, ETriggerEvent::Triggered, this, &ADronePrototypePawn::ChangeAcroRoll);
		EnhancedInputComponent->BindAction(AcroRollAction, ETriggerEvent::Completed, this, &ADronePrototypePawn::ResetAcroRollInput);
		EnhancedInputComponent->BindAction(AcroRollAction, ETriggerEvent::Canceled, this, &ADronePrototypePawn::ResetAcroRollInput);
	}

	if (AcroYawAction)
	{
		EnhancedInputComponent->BindAction(AcroYawAction, ETriggerEvent::Triggered, this, &ADronePrototypePawn::ChangeAcroYaw);
		EnhancedInputComponent->BindAction(AcroYawAction, ETriggerEvent::Completed, this, &ADronePrototypePawn::ResetAcroYawInput);
		EnhancedInputComponent->BindAction(AcroYawAction, ETriggerEvent::Canceled, this, &ADronePrototypePawn::ResetAcroYawInput);
	}

	if (AcroThrottleAction)
	{
		EnhancedInputComponent->BindAction(AcroThrottleAction, ETriggerEvent::Triggered, this, &ADronePrototypePawn::ChangeAcroThrottle);
		EnhancedInputComponent->BindAction(AcroThrottleAction, ETriggerEvent::Completed, this, &ADronePrototypePawn::ResetAcroThrottleInput);
		EnhancedInputComponent->BindAction(AcroThrottleAction, ETriggerEvent::Canceled, this, &ADronePrototypePawn::ResetAcroThrottleInput);
	}

	if (ToggleViewAction)
	{
		EnhancedInputComponent->BindAction(
			ToggleViewAction,
			ETriggerEvent::Started,
			this,
			&ADronePrototypePawn::ToggleViewFromInput);
	}

	if (PrimaryRoleAbilityAction)
	{
		EnhancedInputComponent->BindAction(
			PrimaryRoleAbilityAction,
			ETriggerEvent::Started,
			this,
			&ADronePrototypePawn::TriggerPrimaryRoleAbilityFromInput);
	}

	if (SecondaryRoleAbilityAction)
	{
		EnhancedInputComponent->BindAction(
			SecondaryRoleAbilityAction,
			ETriggerEvent::Started,
			this,
			&ADronePrototypePawn::TriggerSecondaryRoleAbilityFromInput);
	}

	if (!MoveAction
		|| !AltitudeAction
		|| !YawAction
		|| !LookAction
		|| !CameraPitchRateAction
		|| !AcroPitchAction
		|| !AcroRollAction
		|| !AcroYawAction
		|| !AcroThrottleAction
		|| !ToggleViewAction
		|| !PrimaryRoleAbilityAction
		|| !SecondaryRoleAbilityAction)
	{
		UE_LOG(LogDrone, Display, TEXT("Prototype pawn '%s' does not have all prototype Input Actions assigned yet."), *GetNameSafe(this));
	}
}

void ADronePrototypePawn::ApplyPrototypeMappingContext()
{
	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}

	if (!PrototypeMappingContext)
	{
		UE_LOG(LogDrone, Display, TEXT("Prototype pawn '%s' has no prototype Input Mapping Context assigned yet."), *GetNameSafe(this));
		return;
	}

	// Enhanced Input Mapping은 화면과 입력 장치를 가진 로컬 Controller에만 적용한다.
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (bPrototypeMappingContextAdded && AppliedInputSubsystem.Get() == Subsystem
				&& AppliedMappingContext.Get() == PrototypeMappingContext
				&& Subsystem->HasMappingContext(PrototypeMappingContext))
			{
				return;
			}

			// 다른 Local Player나 다른 IMC에서 넘어온 경우 이전 소유 기록을 먼저 정리한다.
			RemovePrototypeMappingContext();

			if (Subsystem->HasMappingContext(PrototypeMappingContext))
			{
				UE_LOG(LogDrone, Display, TEXT("Prototype mapping context for '%s' is already owned by another setup path."), *GetNameSafe(this));
				return;
			}

			Subsystem->AddMappingContext(PrototypeMappingContext, PrototypeMappingPriority);
			if (Subsystem->HasMappingContext(PrototypeMappingContext))
			{
				AppliedInputSubsystem = Subsystem;
				AppliedMappingContext = PrototypeMappingContext;
				bPrototypeMappingContextAdded = true;
			}
			else
			{
				UE_LOG(LogDrone, Warning, TEXT("Prototype mapping context for '%s' could not be registered."), *GetNameSafe(this));
			}
		}
	}
}

void ADronePrototypePawn::RemovePrototypeMappingContext()
{
	if (!bPrototypeMappingContextAdded)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = AppliedInputSubsystem.Get();
	UInputMappingContext* MappingContext = AppliedMappingContext.Get();
	if (Subsystem && MappingContext && Subsystem->HasMappingContext(MappingContext))
	{
		Subsystem->RemoveMappingContext(MappingContext);
	}

	AppliedInputSubsystem.Reset();
	AppliedMappingContext.Reset();
	bPrototypeMappingContextAdded = false;
}

void ADronePrototypePawn::Move(const FInputActionValue& Value)
{
	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}

	const FVector2D MovementValue = Value.Get<FVector2D>();
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		// Acro는 전용 1D Action을 사용한다. 공용 Move를 재해석하면 키보드 W/S와 Space/Ctrl이
		// 모두 Throttle이 되고 Pitch가 사라지므로 여기서는 공용 입력을 소비하지 않는다.
		return;
	}

	// 쉬운/제한 자세에서는 Y=전후, X=좌우이며 Actor 축을 기준으로 직접 이동한다.
	SetVisualTiltInputGreybox(MovementValue.Y, MovementValue.X);
	AddMovementInput(GetActorForwardVector(), MovementValue.Y);
	AddMovementInput(GetActorRightVector(), MovementValue.X);
}

void ADronePrototypePawn::ResetMoveVisualInput(const FInputActionValue&)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		return;
	}
	SetVisualTiltInputGreybox(0.0f, 0.0f);
}

void ADronePrototypePawn::SetVisualBankInputGreybox(const float NormalizedLateralInput)
{
	VisualBankLateralInput = FMath::Clamp(NormalizedLateralInput, -1.0f, 1.0f);
}

void ADronePrototypePawn::SetVisualTiltInputGreybox(
	const float NormalizedForwardInput,
	const float NormalizedLateralInput)
{
	VisualTiltForwardInput = FMath::Clamp(NormalizedForwardInput, -1.0f, 1.0f);
	VisualBankLateralInput = FMath::Clamp(NormalizedLateralInput, -1.0f, 1.0f);
}

void ADronePrototypePawn::SetAcroRateInputGreybox(
	const float PitchInput,
	const float RollInput,
	const float YawInput)
{
	VisualTiltForwardInput = FMath::Clamp(PitchInput, -1.0f, 1.0f);
	VisualBankLateralInput = FMath::Clamp(RollInput, -1.0f, 1.0f);
	AcroYawInput = FMath::Clamp(YawInput, -1.0f, 1.0f);
}

void ADronePrototypePawn::SetAcroThrottleInputGreybox(const float ThrottleInput)
{
	AcroThrottleInput = FMath::Clamp(ThrottleInput, -1.0f, 1.0f);
}

float ADronePrototypePawn::GetCurrentAcroThrottleNormalized() const
{
	const float HoverThrottle = FMath::Clamp(
		ActiveAcroRateSettings.HoverThrottleNormalized,
		0.05f,
		0.95f);
	return AcroThrottleInput >= 0.0f
		? FMath::Lerp(HoverThrottle, 1.0f, AcroThrottleInput)
		: FMath::Lerp(HoverThrottle, 0.0f, -AcroThrottleInput);
}

float ADronePrototypePawn::CalculateActualRateDegreesPerSecond(
	const float NormalizedStickInput,
	const float CenterSensitivityDegreesPerSecond,
	const float MaximumRateDegreesPerSecond,
	const float Expo)
{
	const float Stick = FMath::Clamp(NormalizedStickInput, -1.0f, 1.0f);
	const float AbsoluteStick = FMath::Abs(Stick);
	const float SafeCenterSensitivity = FMath::Max(0.0f, CenterSensitivityDegreesPerSecond);
	const float SafeMaximumRate = FMath::Max(SafeCenterSensitivity, MaximumRateDegreesPerSecond);
	const float SafeExpo = FMath::Clamp(Expo, 0.0f, 1.0f);
	const float ExpoCurve = AbsoluteStick
		* (FMath::Pow(AbsoluteStick, 5.0f) * SafeExpo + AbsoluteStick * (1.0f - SafeExpo));
	const float AdditionalRate = SafeMaximumRate - SafeCenterSensitivity;
	return Stick * SafeCenterSensitivity + FMath::Sign(Stick) * AdditionalRate * ExpoCurve;
}

FRotator ADronePrototypePawn::GetCurrentAcroBodyRateSetpointDegreesPerSecond() const
{
	const float PitchRate = -CalculateActualRateDegreesPerSecond(
		VisualTiltForwardInput,
		ActiveAcroRateSettings.PitchRollCenterSensitivityDegreesPerSecond,
		ActiveAcroRateSettings.MaximumPitchRateDegreesPerSecond,
		ActiveAcroRateSettings.PitchRollExpo);
	const float RollRate = CalculateActualRateDegreesPerSecond(
		VisualBankLateralInput,
		ActiveAcroRateSettings.PitchRollCenterSensitivityDegreesPerSecond,
		ActiveAcroRateSettings.MaximumRollRateDegreesPerSecond,
		ActiveAcroRateSettings.PitchRollExpo);
	const float YawRate = CalculateActualRateDegreesPerSecond(
		AcroYawInput,
		ActiveAcroRateSettings.YawCenterSensitivityDegreesPerSecond,
		ActiveAcroRateSettings.MaximumYawRateDegreesPerSecond,
		ActiveAcroRateSettings.YawExpo);
	return FRotator(PitchRate, YawRate, RollRate);
}

void ADronePrototypePawn::SetFirstPersonViewEnabled(const bool bEnabled)
{
	bFirstPersonViewEnabled = bEnabled;
	if (bDropCameraViewEnabled)
	{
		bFirstPersonViewBeforeDropCamera = bEnabled;
	}
	ApplyCameraViewMode();
}

void ADronePrototypePawn::ToggleFirstPersonView()
{
	SetFirstPersonViewEnabled(!bFirstPersonViewEnabled);
}

void ADronePrototypePawn::SetDropCameraViewEnabled(const bool bEnabled)
{
	if (bEnabled == bDropCameraViewEnabled)
	{
		return;
	}
	if (bEnabled)
	{
		bFirstPersonViewBeforeDropCamera = bFirstPersonViewEnabled;
		CameraBoomRotationBeforeDropCamera = CameraBoom
			? CameraBoom->GetRelativeRotation()
			: FRotator::ZeroRotator;
		bDropCameraViewEnabled = true;
		ApplyCameraViewMode();
		return;
	}

	bDropCameraViewEnabled = false;
	bFirstPersonViewEnabled = bFirstPersonViewBeforeDropCamera;
	ApplyCameraViewMode();
	if (CameraBoom)
	{
		CameraBoom->SetRelativeRotation(CameraBoomRotationBeforeDropCamera);
	}
}

void ADronePrototypePawn::ToggleViewFromInput(const FInputActionValue&)
{
	ToggleFirstPersonView();
}

void ADronePrototypePawn::TriggerPrimaryRoleAbilityFromInput(const FInputActionValue&)
{
	TriggerPrimaryRoleAbility();
}

void ADronePrototypePawn::TriggerSecondaryRoleAbilityFromInput(const FInputActionValue&)
{
	TriggerSecondaryRoleAbility();
}

void ADronePrototypePawn::ApplyCameraViewMode()
{
	if (!CameraBoom || !CollisionComponent || !VisualTiltPivot)
	{
		return;
	}

	const FAttachmentTransformRules KeepRelativeAttachment(EAttachmentRule::KeepRelative, false);
	if (bDropCameraViewEnabled)
	{
		CameraBoom->AttachToComponent(CollisionComponent, KeepRelativeAttachment);
		CameraBoom->SetRelativeLocation(DropViewCameraBoomOffset);
		CameraBoom->SetRelativeRotation(FRotator(DropViewCameraPitchDegrees, 0.0f, 0.0f));
		CameraBoom->TargetArmLength = DropViewCameraArmLength;
		return;
	}
	if (bFirstPersonViewEnabled)
	{
		CameraBoom->AttachToComponent(VisualTiltPivot, KeepRelativeAttachment);
		CameraBoom->SetRelativeLocation(FirstPersonCameraBoomOffset);
		CameraBoom->TargetArmLength = 0.0f;
	}
	else
	{
		CameraBoom->AttachToComponent(CollisionComponent, KeepRelativeAttachment);
		CameraBoom->SetRelativeLocation(ThirdPersonCameraBoomOffset);
		CameraBoom->TargetArmLength = ThirdPersonCameraArmLength;
	}
}

void ADronePrototypePawn::RefreshVisualTiltAttachments()
{
	if (!VisualTiltPivot || !CollisionComponent)
	{
		return;
	}

	const FAttachmentTransformRules KeepRelativeAttachment(EAttachmentRule::KeepRelative, false);
	if (VisualTiltPivot->GetAttachParent() != CollisionComponent)
	{
		VisualTiltPivot->AttachToComponent(CollisionComponent, KeepRelativeAttachment);
	}
	if (VisualMeshComponent && VisualMeshComponent->GetAttachParent() != VisualTiltPivot)
	{
		VisualMeshComponent->AttachToComponent(VisualTiltPivot, KeepRelativeAttachment);
	}

	// 프로젝트 FPV BP의 Rotor처럼 Collision Root에 직접 추가된 외형 Mesh도 모두 같은
	// Pivot을 따르게 한다. CameraBoom 아래 Preview Mesh와 별도 계층 Mesh는 건드리지 않는다.
	TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents(this);
	for (UStaticMeshComponent* MeshComponent : StaticMeshComponents)
	{
		if (!MeshComponent
			|| MeshComponent == VisualMeshComponent
			|| MeshComponent->GetAttachParent() != CollisionComponent
			|| MeshComponent->ComponentHasTag(TEXT("DroneNoVisualBank")))
		{
			continue;
		}
		MeshComponent->AttachToComponent(VisualTiltPivot, KeepRelativeAttachment);
	}
}

void ADronePrototypePawn::RefreshRotorVisualComponents()
{
	RotorVisualComponents.Reset();
	TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents(this);
	for (UStaticMeshComponent* MeshComponent : StaticMeshComponents)
	{
		if (!MeshComponent || MeshComponent == VisualMeshComponent)
		{
			continue;
		}

		const bool bHasRotorTag = !RotorVisualComponentTag.IsNone()
			&& MeshComponent->ComponentHasTag(RotorVisualComponentTag);
		const bool bUsesLegacyRotorName = MeshComponent->GetName().Contains(TEXT("Rotor"));
		if (bHasRotorTag || bUsesLegacyRotorName)
		{
			RotorVisualComponents.Add(MeshComponent);
		}
	}

	// Component 생성 순서와 무관하게 A/B/C/D/5/6의 교차 방향을 항상 동일하게 만든다.
	RotorVisualComponents.Sort([](
		const TWeakObjectPtr<UStaticMeshComponent>& Left,
		const TWeakObjectPtr<UStaticMeshComponent>& Right)
	{
		return GetNameSafe(Left.Get()) < GetNameSafe(Right.Get());
	});
}

void ADronePrototypePawn::UpdateRotorVisuals(const float DeltaSeconds)
{
	if (!bRotorVisualSpinEnabled
		|| RotorVisualComponents.IsEmpty()
		|| RotorVisualSpinDegreesPerSecond <= 0.0f
		|| DeltaSeconds <= 0.0f
		|| (HealthComponent && HealthComponent->IsDead()))
	{
		return;
	}

	const FVector SpinAxis = RotorVisualLocalSpinAxis.GetSafeNormal();
	if (SpinAxis.IsNearlyZero())
	{
		return;
	}

	const float StepRadians = FMath::DegreesToRadians(RotorVisualSpinDegreesPerSecond * DeltaSeconds);
	for (int32 RotorIndex = 0; RotorIndex < RotorVisualComponents.Num(); ++RotorIndex)
	{
		if (UStaticMeshComponent* RotorComponent = RotorVisualComponents[RotorIndex].Get())
		{
			// 제공 Drone Pack의 일부 Rotor Mesh는 Asset Pivot이 기체 원점에 있고
			// 실제 날개 Geometry는 원점에서 멀리 떨어져 있다. 단순 LocalRotation은
			// 이 경우 날개를 기체 주위로 공전시키므로 Mesh Bounds 중심을 고정한 채 회전한다.
			const UStaticMesh* RotorMesh = RotorComponent->GetStaticMesh();
			const FVector RotorLocalCenter = RotorMesh
				? RotorMesh->GetBoundingBox().GetCenter()
				: FVector::ZeroVector;
			const FVector CenterBeforeRotation = RotorComponent->GetRelativeTransform().TransformPosition(RotorLocalCenter);
			const float Direction = bAlternateRotorVisualSpinDirections && (RotorIndex % 2) == 1
				? -1.0f
				: 1.0f;
			RotorComponent->AddLocalRotation(FQuat(SpinAxis, StepRadians * Direction));
			const FVector CenterAfterRotation = RotorComponent->GetRelativeTransform().TransformPosition(RotorLocalCenter);
			RotorComponent->SetRelativeLocation(
				RotorComponent->GetRelativeLocation() + CenterBeforeRotation - CenterAfterRotation);
		}
	}
}

void ADronePrototypePawn::UpdateControlAttitude(const float DeltaSeconds)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		// Rate/Acro의 핵심 계약: 각 Stick은 Body 각속도이며 Stick 중앙은 현재 자세를 유지한다.
		// 모터/PID 전체 시뮬레이션 대신 응답 시간을 둬 목표 Rate가 즉시 순간이동하지 않게 한다.
		const FRotator BodyRateSetpoint = GetCurrentAcroBodyRateSetpointDegreesPerSecond();
		const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
		const float ResponseAlpha = 1.0f - FMath::Exp(
			-SafeDeltaSeconds / FMath::Max(
				0.001f,
				ActiveAcroRateSettings.BodyRateResponseTimeSeconds));
		CurrentAcroBodyRateDegreesPerSecond.Pitch = FMath::Lerp(
			CurrentAcroBodyRateDegreesPerSecond.Pitch,
			BodyRateSetpoint.Pitch,
			ResponseAlpha);
		CurrentAcroBodyRateDegreesPerSecond.Yaw = FMath::Lerp(
			CurrentAcroBodyRateDegreesPerSecond.Yaw,
			BodyRateSetpoint.Yaw,
			ResponseAlpha);
		CurrentAcroBodyRateDegreesPerSecond.Roll = FMath::Lerp(
			CurrentAcroBodyRateDegreesPerSecond.Roll,
			BodyRateSetpoint.Roll,
			ResponseAlpha);

		// Local Rotation을 누적하므로 제한 자세 모드와 달리 90도 이상의 Roll/Loop가 가능하다.
		AddActorLocalRotation(FRotator(
			CurrentAcroBodyRateDegreesPerSecond.Pitch * SafeDeltaSeconds,
			CurrentAcroBodyRateDegreesPerSecond.Yaw * SafeDeltaSeconds,
			CurrentAcroBodyRateDegreesPerSecond.Roll * SafeDeltaSeconds));
		return;
	}

	const FDroneControlModeTuning& ControlTuning = ResolveCurrentControlModeTuning();
	const bool bUseRootAttitude = ControlTuning.bTiltCollisionRoot;

	// 쉬운 조작에서는 Root를 자동으로 수평 복귀시킨다. 실제 조작형에서는 이동 입력이
	// Root Pitch/Roll이 되어 다음 프레임의 Forward/Right/Up 이동 축까지 바꾼다.
	const float TargetPitch = bUseRootAttitude
		? -VisualTiltForwardInput * MaximumVisualTiltPitchDegrees
		: 0.0f;
	const float TargetRoll = bUseRootAttitude
		? VisualBankLateralInput * MaximumVisualBankRollDegrees
		: 0.0f;
	const bool bHasAttitudeInput = !FMath::IsNearlyZero(VisualTiltForwardInput)
		|| !FMath::IsNearlyZero(VisualBankLateralInput);
	const float InterpolationSpeed = bHasAttitudeInput
		? VisualBankInterpolationSpeed
		: VisualBankReturnSpeed;

	FRotator ActorRotation = GetActorRotation();
	ActorRotation.Pitch = FMath::FInterpTo(
		ActorRotation.Pitch,
		TargetPitch,
		FMath::Max(0.0f, DeltaSeconds),
		InterpolationSpeed);
	ActorRotation.Roll = FMath::FInterpTo(
		ActorRotation.Roll,
		TargetRoll,
		FMath::Max(0.0f, DeltaSeconds),
		InterpolationSpeed);
	SetActorRotation(ActorRotation);
}

void ADronePrototypePawn::UpdateAcroFlightPhysics(const float DeltaSeconds)
{
	if (CurrentControlMode != EDroneControlMode::AcroRateRealisticGreybox
		|| !PrototypeMovementComponent
		|| (HealthComponent && HealthComponent->IsDead()))
	{
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	if (SafeDeltaSeconds <= 0.0f)
	{
		return;
	}

	const float GravityAcceleration = FMath::Max(
		1.0f,
		ActiveAcroRateSettings.GravityAccelerationCentimetersPerSecondSquared);
	const float HoverThrottle = FMath::Clamp(
		ActiveAcroRateSettings.HoverThrottleNormalized,
		0.05f,
		0.95f);
	const float MaximumThrustAcceleration = GravityAcceleration / HoverThrottle;
	const float ThrustAcceleration = GetCurrentAcroThrottleNormalized() * MaximumThrustAcceleration;
	const FVector NetAcceleration =
		GetActorUpVector() * ThrustAcceleration
		+ FVector::DownVector * GravityAcceleration;

	// 속도 비례 항력은 프레임 시간에 안정적인 지수 감쇠로 적용한다.
	FVector Velocity = PrototypeMovementComponent->Velocity;
	const float DragPerSecond = FMath::Max(0.0f, ActiveAcroRateSettings.LinearDragPerSecond);
	Velocity *= FMath::Exp(-DragPerSecond * SafeDeltaSeconds);
	Velocity += NetAcceleration * SafeDeltaSeconds;
	PrototypeMovementComponent->Velocity = Velocity;
}

void ADronePrototypePawn::LimitAcroVerticalSpeed()
{
	if (CurrentControlMode != EDroneControlMode::AcroRateRealisticGreybox || !PrototypeMovementComponent)
	{
		return;
	}

	// Flight Profile MaxSpeed는 전체 속도, 공개 FPV 기준 상승/하강은 World Z 성분을 제한한다.
	FVector Velocity = PrototypeMovementComponent->Velocity;
	Velocity = Velocity.GetClampedToMaxSize(FMath::Max(0.0f, PrototypeMovementComponent->MaxSpeed));
	Velocity.Z = FMath::Clamp(
		Velocity.Z,
		-ActiveAcroRateSettings.MaximumWorldVerticalSpeedCentimetersPerSecond,
		ActiveAcroRateSettings.MaximumWorldVerticalSpeedCentimetersPerSecond);
	PrototypeMovementComponent->Velocity = Velocity;
}

void ADronePrototypePawn::UpdateVisualBank(const float DeltaSeconds)
{
	if (!VisualTiltPivot)
	{
		return;
	}

	const FDroneControlModeTuning& ControlTuning = ResolveCurrentControlModeTuning();
	// 실제 조작형은 Root가 이미 기울어지므로 외형 Pivot에는 중복 기울기를 적용하지 않는다.
	const float TargetPitch = ControlTuning.bTiltCollisionRoot
		? 0.0f
		: -VisualTiltForwardInput * MaximumVisualTiltPitchDegrees;
	const float TargetRoll = ControlTuning.bTiltCollisionRoot
		? 0.0f
		: VisualBankLateralInput * MaximumVisualBankRollDegrees;
	const float PitchInterpolationSpeed = FMath::IsNearlyZero(VisualTiltForwardInput)
		? VisualBankReturnSpeed
		: VisualBankInterpolationSpeed;
	const float RollInterpolationSpeed = FMath::IsNearlyZero(VisualBankLateralInput)
		? VisualBankReturnSpeed
		: VisualBankInterpolationSpeed;
	CurrentVisualTiltPitchDegrees = FMath::FInterpTo(
		CurrentVisualTiltPitchDegrees,
		TargetPitch,
		FMath::Max(0.0f, DeltaSeconds),
		PitchInterpolationSpeed);
	CurrentVisualBankRollDegrees = FMath::FInterpTo(
		CurrentVisualBankRollDegrees,
		TargetRoll,
		FMath::Max(0.0f, DeltaSeconds),
		RollInterpolationSpeed);
	FRotator CombinedRotation = CurrentDamageShakeVisualRotation;
	CombinedRotation.Pitch += CurrentVisualTiltPitchDegrees;
	CombinedRotation.Roll += CurrentVisualBankRollDegrees;
	VisualTiltPivot->SetRelativeRotation(CombinedRotation);
}

void ADronePrototypePawn::TriggerDamageShakeGreybox(const float AppliedDamage)
{
	if (!bDamageShakeEnabled || AppliedDamage <= 0.0f || DamageShakeDurationSeconds <= 0.0f)
	{
		return;
	}

	const float DamageScale = FMath::Clamp(
		AppliedDamage / FMath::Max(1.0f, DamageForMaximumShake),
		MinimumDamageShakeScale,
		1.0f);
	CurrentDamageShakeStrength = FMath::Max(CurrentDamageShakeStrength, DamageScale);
	DamageShakeTimeRemainingSeconds = DamageShakeDurationSeconds;
	DamageShakePhaseRadians = FMath::Fmod(DamageShakePhaseRadians + 1.6180339f, UE_TWO_PI);
	++DamageShakeEventCount;

	if (FollowCamera && !bCameraAdditiveBaseCaptured)
	{
		FollowCamera->GetAdditiveOffset(CameraAdditiveBaseTransform, CameraAdditiveBaseFOV);
		bCameraAdditiveBaseCaptured = true;
	}
}

void ADronePrototypePawn::UpdateDamageShake(const float DeltaSeconds)
{
	if (DamageShakeTimeRemainingSeconds <= 0.0f)
	{
		return;
	}

	const float Duration = FMath::Max(0.01f, DamageShakeDurationSeconds);
	const float RemainingRatio = FMath::Clamp(DamageShakeTimeRemainingSeconds / Duration, 0.0f, 1.0f);
	const float Envelope = RemainingRatio * RemainingRatio * CurrentDamageShakeStrength;
	const float ElapsedSeconds = Duration - DamageShakeTimeRemainingSeconds;
	const float Oscillation = ElapsedSeconds * DamageShakeOscillationsPerSecond * UE_TWO_PI
		+ DamageShakePhaseRadians;

	CurrentDamageShakeVisualRotation = FRotator(
		FMath::Sin(Oscillation * 0.83f) * DamageShakeVisualRotationDegrees * 0.65f * Envelope,
		FMath::Sin(Oscillation * 1.17f + 0.7f) * DamageShakeVisualRotationDegrees * 0.35f * Envelope,
		FMath::Cos(Oscillation) * DamageShakeVisualRotationDegrees * Envelope);

	const FVector CameraLocationOffset(
		FMath::Sin(Oscillation * 1.31f),
		FMath::Cos(Oscillation * 0.91f),
		FMath::Sin(Oscillation * 1.53f + 0.4f));
	const FRotator CameraRotationOffset(
		FMath::Sin(Oscillation * 0.77f) * DamageShakeCameraRotationDegrees * Envelope,
		FMath::Cos(Oscillation * 1.09f) * DamageShakeCameraRotationDegrees * 0.6f * Envelope,
		FMath::Sin(Oscillation * 1.43f) * DamageShakeCameraRotationDegrees * 0.5f * Envelope);
	ApplyDamageShakeCameraOffset(
		CameraLocationOffset * DamageShakeCameraLocationCentimeters * Envelope,
		CameraRotationOffset);

	DamageShakeTimeRemainingSeconds = FMath::Max(
		0.0f,
		DamageShakeTimeRemainingSeconds - FMath::Max(0.0f, DeltaSeconds));
	if (DamageShakeTimeRemainingSeconds <= 0.0f)
	{
		ResetDamageShakePresentation();
	}
}

void ADronePrototypePawn::ApplyDamageShakeCameraOffset(
	const FVector& LocationOffset,
	const FRotator& RotationOffset)
{
	if (!FollowCamera)
	{
		return;
	}

	FollowCamera->ClearAdditiveOffset();
	if (bCameraAdditiveBaseCaptured)
	{
		FollowCamera->AddAdditiveOffset(CameraAdditiveBaseTransform, CameraAdditiveBaseFOV);
	}
	FollowCamera->AddAdditiveOffset(FTransform(RotationOffset, LocationOffset), 0.0f);
}

void ADronePrototypePawn::ResetDamageShakePresentation()
{
	DamageShakeTimeRemainingSeconds = 0.0f;
	CurrentDamageShakeStrength = 0.0f;
	CurrentDamageShakeVisualRotation = FRotator::ZeroRotator;
	if (FollowCamera && bCameraAdditiveBaseCaptured)
	{
		FollowCamera->ClearAdditiveOffset();
		FollowCamera->AddAdditiveOffset(CameraAdditiveBaseTransform, CameraAdditiveBaseFOV);
	}
	bCameraAdditiveBaseCaptured = false;
	CameraAdditiveBaseTransform = FTransform::Identity;
	CameraAdditiveBaseFOV = 0.0f;
}

void ADronePrototypePawn::HandleHealthChanged(
	const float,
	const float,
	const float,
	const float AppliedDamage)
{
	TriggerDamageShakeGreybox(AppliedDamage);
}

void ADronePrototypePawn::ChangeAltitude(const FInputActionValue& Value)
{
	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		return;
	}

	const FDroneControlModeTuning& ControlTuning = ResolveCurrentControlModeTuning();
	// 쉬운 조작은 World Up, 실제 조작형은 현재 기체의 Local Up을 사용한다.
	const FVector AltitudeAxis = ControlTuning.bUseLocalAltitudeAxis
		? GetActorUpVector()
		: FVector::UpVector;
	AddMovementInput(AltitudeAxis, Value.Get<float>());
}

void ADronePrototypePawn::ChangeYaw(const FInputActionValue& Value)
{
	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}

	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 키·Gamepad 축은 프레임률에 무관하도록 초당 회전량에 DeltaSeconds를 곱한다.
	const float YawDelta = Value.Get<float>() * PrototypeYawRateDegreesPerSecond * World->GetDeltaSeconds();
	AddActorLocalRotation(FRotator(0.0f, YawDelta, 0.0f));
}

void ADronePrototypePawn::ResetYawInput(const FInputActionValue&)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		return;
	}
}

void ADronePrototypePawn::Look(const FInputActionValue& Value)
{
	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}

	// Mouse X는 Drone Yaw, Mouse Y는 SpringArm Pitch만 변경한다.
	const FVector2D LookValue = Value.Get<FVector2D>();
	AddActorLocalRotation(FRotator(0.0f, LookValue.X * PrototypeMouseYawDegreesPerInput, 0.0f));
	AdjustCameraPitch(LookValue.Y * PrototypeMousePitchDegreesPerInput);
}

void ADronePrototypePawn::ChangeCameraPitch(const FInputActionValue& Value)
{
	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}

	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float PitchDelta =
		Value.Get<float>() * PrototypeGamepadPitchRateDegreesPerSecond * World->GetDeltaSeconds();
	AdjustCameraPitch(PitchDelta);
}

void ADronePrototypePawn::ResetCameraPitchInput(const FInputActionValue&)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		return;
	}
}

void ADronePrototypePawn::ChangeAcroPitch(const FInputActionValue& Value)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox
		&& (!HealthComponent || !HealthComponent->IsDead()))
	{
		VisualTiltForwardInput = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f);
	}
}

void ADronePrototypePawn::ResetAcroPitchInput(const FInputActionValue&)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		VisualTiltForwardInput = 0.0f;
	}
}

void ADronePrototypePawn::ChangeAcroRoll(const FInputActionValue& Value)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox
		&& (!HealthComponent || !HealthComponent->IsDead()))
	{
		VisualBankLateralInput = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f);
	}
}

void ADronePrototypePawn::ResetAcroRollInput(const FInputActionValue&)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		VisualBankLateralInput = 0.0f;
	}
}

void ADronePrototypePawn::ChangeAcroYaw(const FInputActionValue& Value)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox
		&& (!HealthComponent || !HealthComponent->IsDead()))
	{
		AcroYawInput = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f);
	}
}

void ADronePrototypePawn::ResetAcroYawInput(const FInputActionValue&)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		AcroYawInput = 0.0f;
	}
}

void ADronePrototypePawn::ChangeAcroThrottle(const FInputActionValue& Value)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox
		&& (!HealthComponent || !HealthComponent->IsDead()))
	{
		SetAcroThrottleInputGreybox(Value.Get<float>());
	}
}

void ADronePrototypePawn::ResetAcroThrottleInput(const FInputActionValue&)
{
	if (CurrentControlMode == EDroneControlMode::AcroRateRealisticGreybox)
	{
		SetAcroThrottleInputGreybox(0.0f);
	}
}

void ADronePrototypePawn::AdjustCameraPitch(const float PitchDeltaDegrees)
{
	if (!CameraBoom || FMath::IsNearlyZero(PitchDeltaDegrees))
	{
		return;
	}

	FRotator BoomRotation = CameraBoom->GetRelativeRotation();
	BoomRotation.Pitch = FMath::Clamp(
		BoomRotation.Pitch + PitchDeltaDegrees,
		PrototypeMinimumCameraPitchDegrees,
		PrototypeMaximumCameraPitchDegrees);
	BoomRotation.Yaw = 0.0f;
	BoomRotation.Roll = 0.0f;
	CameraBoom->SetRelativeRotation(BoomRotation);
}

void ADronePrototypePawn::HandleDeath(
	AActor* DeadActor,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	if (DeadActor != this)
	{
		return;
	}
	SetVisualTiltInputGreybox(0.0f, 0.0f);
	AcroYawInput = 0.0f;
	AcroThrottleInput = 0.0f;
	CurrentAcroBodyRateDegreesPerSecond = FRotator::ZeroRotator;
	ReconScanComponent->CancelScan();
	ImpactDetonationComponent->DisarmImpactDetonation();
	PayloadDropComponent->SetDropViewEnabled(false);

	// 회색상자 사망 규칙: 기체는 현 위치에 남기고 조종·이동·충돌만 중지한다.
	// 이후 GameMode가 이 Event를 받아 임무 실패 화면이나 Respawn을 결정할 수 있다.
	RemovePrototypeMappingContext();
	PrototypeMovementComponent->StopMovementImmediately();
	PrototypeMovementComponent->Deactivate();
	SetActorEnableCollision(false);
	PerceptionStimuliSource->UnregisterFromPerceptionSystem();
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		DisableInput(PlayerController);
	}

	for (TActorIterator<ADroneNPCAIController> It(GetWorld()); It; ++It)
	{
		It->HandleDetectedDroneDestroyed(this);
	}
	++DroneDestroyedEventCount;
	OnDroneDestroyed.Broadcast(this);
}
