#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DronePrototypePawn.generated.h"

class AController;
class UCameraComponent;
class UDroneTelemetryComponent;
class UDroneHealthComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UFloatingPawnMovement;
class UInputAction;
class UInputMappingContext;
class USphereComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UAIPerceptionStimuliSourceComponent;
struct FInputActionValue;

class ADronePrototypePawn;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDroneDestroyedSignature, ADronePrototypePawn*, DestroyedDrone);

/**
 * 기존 Third Person Character와 분리한 Drone 조종 Prototype Pawn.
 *
 * 이 클래스는 입력·이동·카메라와 Telemetry Component 소유까지만 담당한다.
 * HUD 생성은 PlayerController, 화면 외형은 WBP가 담당하므로 Pawn 교체 때 UI가 사라지지 않는다.
 * 현재 수치와 입력 Asset은 최종 비행 물리·감도·네트워크 규칙이 아니다.
 */
UCLASS(Blueprintable)
class ADronePrototypePawn : public APawn
{
	GENERATED_BODY()

public:
	ADronePrototypePawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;

	USphereComponent* GetCollisionComponent() const { return CollisionComponent; }
	USceneComponent* GetVisualTiltPivot() const { return VisualTiltPivot; }
	UStaticMeshComponent* GetVisualMeshComponent() const { return VisualMeshComponent; }
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	UFloatingPawnMovement* GetPrototypeMovementComponent() const { return PrototypeMovementComponent; }
	UDroneTelemetryComponent* GetTelemetryComponent() const { return TelemetryComponent; }
	UDroneHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UAIPerceptionStimuliSourceComponent* GetPerceptionStimuliSource() const { return PerceptionStimuliSource; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|VisualBank")
	float GetCurrentVisualBankRollDegrees() const { return CurrentVisualBankRollDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|VisualBank")
	float GetCurrentVisualTiltPitchDegrees() const { return CurrentVisualTiltPitchDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|VisualBank")
	float GetMaximumVisualBankRollDegrees() const { return MaximumVisualBankRollDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|VisualBank")
	float GetMaximumVisualTiltPitchDegrees() const { return MaximumVisualTiltPitchDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Camera")
	bool IsFirstPersonViewEnabled() const { return bFirstPersonViewEnabled; }

	/** 3인칭 추적 카메라와 이동 Pitch·Roll을 함께 따르는 1인칭 카메라를 전환한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Camera")
	void SetFirstPersonViewEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Drone|Camera")
	void ToggleFirstPersonView();

	/** 기존 Blueprint 호환용 좌우 입력 함수다. 전후 입력 상태는 유지한다. -1=좌, +1=우. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flight|VisualBank")
	void SetVisualBankInputGreybox(float NormalizedLateralInput);

	/** 이동 외의 AI/연출 경로도 같은 2축 외형 기울기를 사용한다. 전후·좌우 입력은 각각 -1~+1이다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flight|VisualBank")
	void SetVisualTiltInputGreybox(float NormalizedForwardInput, float NormalizedLateralInput);

	/** Health Damage와 같은 경로로 외형·카메라 피격 흔들림을 시작한다. 피해량은 강도 계산에만 사용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Feedback|DamageShake")
	void TriggerDamageShakeGreybox(float AppliedDamage);

	UFUNCTION(BlueprintPure, Category="Drone|Feedback|DamageShake")
	bool IsDamageShakeActive() const { return DamageShakeTimeRemainingSeconds > 0.0f; }

	UFUNCTION(BlueprintPure, Category="Drone|Feedback|DamageShake")
	float GetCurrentDamageShakeStrength() const { return CurrentDamageShakeStrength; }

	UFUNCTION(BlueprintPure, Category="Drone|Feedback|DamageShake|Debug")
	int32 GetDamageShakeEventCount() const { return DamageShakeEventCount; }

	UFUNCTION(BlueprintPure, Category="Drone|Health|Debug")
	int32 GetDroneDestroyedEventCount() const { return DroneDestroyedEventCount; }

	/** Mission/GameMode는 이 Event를 받아 실패 화면·재시작 규칙을 추가한다. */
	UPROPERTY(BlueprintAssignable, Category="Drone|Health")
	FDroneDestroyedSignature OnDroneDestroyed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 충돌과 이동의 기준 Root. Visual Mesh와 분리해 구매 에셋 교체 영향을 줄인다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> CollisionComponent;

	/** 충돌·카메라는 수평으로 두고 Drone 본체와 Rotor 외형만 Pitch·Roll시키는 Pivot이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> VisualTiltPivot;

	/** 현재 Engine 기본 도형을 표시하는 외형 전용 Component. Collision은 사용하지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UFloatingPawnMovement> PrototypeMovementComponent;

	/** HUD와 Tutorial 기록기에 기본 0.1초 주기 및 명시적 즉시 갱신 Snapshot을 공급한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDroneTelemetryComponent> TelemetryComponent;

	/** 드론 기본 체력 100과 파괴/실패 판정용 사망 Event를 제공한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDroneHealthComponent> HealthComponent;

	/** Enemy AI Sight가 드론을 명시적인 감지 대상으로 등록하는 Component다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAIPerceptionStimuliSourceComponent> PerceptionStimuliSource;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputMappingContext> PrototypeMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> AltitudeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> YawAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> CameraPitchRateAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> ToggleViewAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	int32 PrototypeMappingPriority = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Movement", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float PrototypeYawRateDegreesPerSecond = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float PrototypeMouseYawDegreesPerInput = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float PrototypeMousePitchDegreesPerInput = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float PrototypeGamepadPitchRateDegreesPerSecond = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(ClampMin="-89.0", ClampMax="89.0", AllowPrivateAccess="true"))
	float PrototypeMinimumCameraPitchDegrees = -70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(ClampMin="-89.0", ClampMax="89.0", AllowPrivateAccess="true"))
	float PrototypeMaximumCameraPitchDegrees = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(ClampMin="0.0", ForceUnits="cm", AllowPrivateAccess="true"))
	float ThirdPersonCameraArmLength = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(AllowPrivateAccess="true"))
	FVector ThirdPersonCameraBoomOffset = FVector::ZeroVector;

	/** FPV에서 Drone 외형 앞쪽에 둘 CameraBoom 위치다. 최종 Mesh에 맞춰 파생 BP에서 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(AllowPrivateAccess="true"))
	FVector FirstPersonCameraBoomOffset = FVector(70.0f, 0.0f, 12.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(AllowPrivateAccess="true"))
	bool bStartInFirstPersonView = false;

	/** 좌우 입력 1.0에서 외형에 적용할 최대 Roll이다. 이동·충돌·카메라 회전에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|VisualBank", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg", AllowPrivateAccess="true"))
	float MaximumVisualBankRollDegrees = 18.0f;

	/** 전후 입력 1.0에서 외형에 적용할 최대 Pitch다. 전진은 기수 아래, 후진은 기수 위 방향이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|VisualBank", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg", AllowPrivateAccess="true"))
	float MaximumVisualTiltPitchDegrees = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|VisualBank", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float VisualBankInterpolationSpeed = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|VisualBank", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float VisualBankReturnSpeed = 5.0f;

	/** 피격 흔들림은 외형과 Camera View에만 적용하고 Actor 이동·Collision에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|DamageShake", meta=(AllowPrivateAccess="true"))
	bool bDamageShakeEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|DamageShake", meta=(ClampMin="0.05", ClampMax="2.0", ForceUnits="s", AllowPrivateAccess="true"))
	float DamageShakeDurationSeconds = 0.30f;

	/** 이 피해량 이상은 최대 흔들림 강도로 제한한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|DamageShake", meta=(ClampMin="1.0", AllowPrivateAccess="true"))
	float DamageForMaximumShake = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|DamageShake", meta=(ClampMin="0.0", ClampMax="1.0", AllowPrivateAccess="true"))
	float MinimumDamageShakeScale = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|DamageShake", meta=(ClampMin="0.0", ClampMax="20.0", ForceUnits="deg", AllowPrivateAccess="true"))
	float DamageShakeVisualRotationDegrees = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|DamageShake", meta=(ClampMin="0.0", ClampMax="50.0", ForceUnits="cm", AllowPrivateAccess="true"))
	float DamageShakeCameraLocationCentimeters = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|DamageShake", meta=(ClampMin="0.0", ClampMax="10.0", ForceUnits="deg", AllowPrivateAccess="true"))
	float DamageShakeCameraRotationDegrees = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|DamageShake", meta=(ClampMin="1.0", ClampMax="60.0", AllowPrivateAccess="true"))
	float DamageShakeOscillationsPerSecond = 18.0f;

private:
	/** 이 Pawn이 실제로 추가한 IMC만 나중에 제거하기 위해 소유 기록을 보관한다. */
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AppliedInputSubsystem;
	TWeakObjectPtr<UInputMappingContext> AppliedMappingContext;
	bool bPrototypeMappingContextAdded = false;

	/** 로컬 Player에 Prototype IMC를 정확히 한 번 추가한다. */
	void ApplyPrototypeMappingContext();

	/** 다른 시스템의 IMC는 건드리지 않고 이 Pawn이 추가한 Mapping만 제거한다. */
	void RemovePrototypeMappingContext();
	void EnsureHealthFeedbackBindings();

	void Move(const FInputActionValue& Value);
	void ResetMoveVisualInput(const FInputActionValue& Value);
	void ChangeAltitude(const FInputActionValue& Value);
	void ChangeYaw(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void ChangeCameraPitch(const FInputActionValue& Value);
	void ToggleViewFromInput(const FInputActionValue& Value);
	void AdjustCameraPitch(float PitchDeltaDegrees);
	void ApplyCameraViewMode();
	void RefreshVisualTiltAttachments();
	void UpdateVisualBank(float DeltaSeconds);
	void UpdateDamageShake(float DeltaSeconds);
	void ApplyDamageShakeCameraOffset(const FVector& LocationOffset, const FRotator& RotationOffset);
	void ResetDamageShakePresentation();

	void HandleHealthChanged(float PreviousHealth, float CurrentHealth, float MaxHealth, float AppliedDamage);

	UFUNCTION()
	void HandleDeath(AActor* DeadActor, AController* InstigatorController, AActor* DamageCauser);

	UPROPERTY(Transient)
	int32 DroneDestroyedEventCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Flight|VisualBank")
	float CurrentVisualBankRollDegrees = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Flight|VisualBank")
	float CurrentVisualTiltPitchDegrees = 0.0f;

	float VisualBankLateralInput = 0.0f;
	float VisualTiltForwardInput = 0.0f;
	bool bFirstPersonViewEnabled = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Feedback|DamageShake")
	float CurrentDamageShakeStrength = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Feedback|DamageShake")
	FRotator CurrentDamageShakeVisualRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	int32 DamageShakeEventCount = 0;

	float DamageShakeTimeRemainingSeconds = 0.0f;
	float DamageShakePhaseRadians = 0.0f;
	FTransform CameraAdditiveBaseTransform = FTransform::Identity;
	float CameraAdditiveBaseFOV = 0.0f;
	bool bCameraAdditiveBaseCaptured = false;
};
