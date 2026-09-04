#include "Prototype/DronePrototypePawn.h"

#include "Camera/CameraComponent.h"
#include "AI/DroneNPCAIController.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Drone.h"
#include "Engine/LocalPlayer.h"
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
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Telemetry/DroneTelemetryComponent.h"

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

	// HUD가 Pawn을 직접 계산하지 않도록 공용 데이터 공급 Component를 기본 부착한다.
	TelemetryComponent = CreateDefaultSubobject<UDroneTelemetryComponent>(TEXT("TelemetryComponent"));
	HealthComponent = CreateDefaultSubobject<UDroneHealthComponent>(TEXT("HealthComponent"));

	// AI Perception의 전역 Pawn 자동 등록 설정에 의존하지 않고 Sight 대상으로 명시한다.
	PerceptionStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuliSource"));
}

void ADronePrototypePawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisualTiltAttachments();
	EnsureHealthFeedbackBindings();
}

void ADronePrototypePawn::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisualTiltAttachments();
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
	UpdateVisualBank(DeltaSeconds);
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
	ResetDamageShakePresentation();
	if (HealthComponent)
	{
		HealthComponent->OnHealthChangedNative.RemoveAll(this);
		HealthComponent->OnDeath.RemoveDynamic(this, &ADronePrototypePawn::HandleDeath);
	}
	Super::EndPlay(EndPlayReason);
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
	}

	if (ToggleViewAction)
	{
		EnhancedInputComponent->BindAction(
			ToggleViewAction,
			ETriggerEvent::Started,
			this,
			&ADronePrototypePawn::ToggleViewFromInput);
	}

	if (!MoveAction || !AltitudeAction || !YawAction || !LookAction || !CameraPitchRateAction || !ToggleViewAction)
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

	// Y=전후, X=좌우이며 Actor의 현재 Yaw를 기준으로 이동한다.
	const FVector2D MovementValue = Value.Get<FVector2D>();
	SetVisualTiltInputGreybox(MovementValue.Y, MovementValue.X);
	AddMovementInput(GetActorForwardVector(), MovementValue.Y);
	AddMovementInput(GetActorRightVector(), MovementValue.X);
}

void ADronePrototypePawn::ResetMoveVisualInput(const FInputActionValue&)
{
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

void ADronePrototypePawn::SetFirstPersonViewEnabled(const bool bEnabled)
{
	bFirstPersonViewEnabled = bEnabled;
	ApplyCameraViewMode();
}

void ADronePrototypePawn::ToggleFirstPersonView()
{
	SetFirstPersonViewEnabled(!bFirstPersonViewEnabled);
}

void ADronePrototypePawn::ToggleViewFromInput(const FInputActionValue&)
{
	ToggleFirstPersonView();
}

void ADronePrototypePawn::ApplyCameraViewMode()
{
	if (!CameraBoom || !CollisionComponent || !VisualTiltPivot)
	{
		return;
	}

	const FAttachmentTransformRules KeepRelativeAttachment(EAttachmentRule::KeepRelative, false);
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

void ADronePrototypePawn::UpdateVisualBank(const float DeltaSeconds)
{
	if (!VisualTiltPivot)
	{
		return;
	}

	// 사용자 조작 기준: 전진은 기수를 아래로, 오른쪽 이동은 외형을 오른쪽으로 기울인다.
	const float TargetPitch = -VisualTiltForwardInput * MaximumVisualTiltPitchDegrees;
	const float TargetRoll = VisualBankLateralInput * MaximumVisualBankRollDegrees;
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

	// 기체 기울기와 무관하게 World Up 방향을 사용한다.
	AddMovementInput(FVector::UpVector, Value.Get<float>());
}

void ADronePrototypePawn::ChangeYaw(const FInputActionValue& Value)
{
	if (HealthComponent && HealthComponent->IsDead())
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

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float PitchDelta =
		Value.Get<float>() * PrototypeGamepadPitchRateDegreesPerSecond * World->GetDeltaSeconds();
	AdjustCameraPitch(PitchDelta);
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
