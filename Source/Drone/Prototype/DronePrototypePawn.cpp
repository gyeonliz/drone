#include "Prototype/DronePrototypePawn.h"

#include "Camera/CameraComponent.h"
#include "AI/DroneNPCAIController.h"
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
	// 이동은 UFloatingPawnMovement가 처리하므로 Pawn 자체 Tick은 필요하지 않다.
	PrimaryActorTick.bCanEverTick = false;

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

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshComponent"));
	VisualMeshComponent->SetupAttachment(CollisionComponent);
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

void ADronePrototypePawn::BeginPlay()
{
	Super::BeginPlay();

	// Actor가 유효한 World에 들어온 뒤 등록해야 Perception System이 실제 Source를 받을 수 있다.
	PerceptionStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	PerceptionStimuliSource->RegisterWithPerceptionSystem();
	HealthComponent->OnDeath.AddDynamic(this, &ADronePrototypePawn::HandleDeath);
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

	if (!MoveAction || !AltitudeAction || !YawAction || !LookAction || !CameraPitchRateAction)
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
	AddMovementInput(GetActorForwardVector(), MovementValue.Y);
	AddMovementInput(GetActorRightVector(), MovementValue.X);
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
