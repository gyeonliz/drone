#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Mission/DroneDefinition.h"
#include "Prototype/DroneFlightControlTypes.h"
#include "DronePrototypePawn.generated.h"

class AController;
class UCameraComponent;
class UDroneTelemetryComponent;
class UDroneHealthComponent;
class UDroneImpactDetonationComponent;
class UDronePayloadDropComponent;
class UDroneReconScanComponent;
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneFlightControlSettingsChangedSignature,
	EDroneControlMode, ControlMode,
	EDroneHandlingPreset, HandlingPreset);

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

	UFUNCTION(BlueprintPure, Category="Drone|Drop|Pickup")
	USceneComponent* GetPayloadCarryAnchor() const { return PayloadCarryAnchor; }
	UStaticMeshComponent* GetVisualMeshComponent() const { return VisualMeshComponent; }
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	UFloatingPawnMovement* GetPrototypeMovementComponent() const { return PrototypeMovementComponent; }
	UDroneTelemetryComponent* GetTelemetryComponent() const { return TelemetryComponent; }
	UDroneHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UDroneReconScanComponent* GetReconScanComponent() const { return ReconScanComponent; }
	UDroneImpactDetonationComponent* GetImpactDetonationComponent() const { return ImpactDetonationComponent; }
	UDronePayloadDropComponent* GetPayloadDropComponent() const { return PayloadDropComponent; }
	UAIPerceptionStimuliSourceComponent* GetPerceptionStimuliSource() const { return PerceptionStimuliSource; }

	/** FLOW-05가 Spawn한 Pawn에 선택 Definition의 실제 비행 수치를 한 번에 적용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flight|Profile")
	bool ApplyDroneDefinition(const UDroneDefinition* Definition);

	UFUNCTION(BlueprintPure, Category="Drone|Flight|Profile")
	FName GetAppliedDroneId() const { return AppliedDroneId; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|Profile")
	float GetPrototypeYawRateDegreesPerSecond() const { return PrototypeYawRateDegreesPerSecond; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|Profile")
	bool UsesBlueprintFlightProfileOverride() const { return bOverrideDefinitionFlightProfileInBlueprint; }

	/**
	 * 현재 Definition의 역할에 맞는 공통 1차 기능을 실행한다.
	 * Recon=가장 가까운 유효 대상 Scan, FPV=충돌 자폭 Arm, Drop=적재 중 투하/빈 상태 근처 화물 적재다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|Role Ability")
	bool TriggerPrimaryRoleAbility();

	/** Recon=Scan 취소, FPV=Disarm, Drop=상단 Camera 전환을 실행한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Role Ability")
	bool TriggerSecondaryRoleAbility();

	/** 쉬운 조작과 실제 조작형 Greybox를 플레이 중에도 바꾼다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flight|Control")
	void SetControlMode(EDroneControlMode NewControlMode);

	UFUNCTION(BlueprintCallable, Category="Drone|Flight|Control")
	void ToggleControlMode();

	UFUNCTION(BlueprintPure, Category="Drone|Flight|Control")
	EDroneControlMode GetControlMode() const { return CurrentControlMode; }

	/** 안정/균형/고기동은 기체 종류가 아니라 독립된 반응성 프리셋이다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flight|Control")
	void SetHandlingPreset(EDroneHandlingPreset NewHandlingPreset);

	UFUNCTION(BlueprintCallable, Category="Drone|Flight|Control")
	void CycleHandlingPreset();

	UFUNCTION(BlueprintPure, Category="Drone|Flight|Control")
	EDroneHandlingPreset GetHandlingPreset() const { return CurrentHandlingPreset; }

	/** FLOW-05 선택 UI나 설정 Widget은 이 Event로 현재 조작 설정 표시를 갱신한다. */
	UPROPERTY(BlueprintAssignable, Category="Drone|Flight|Control")
	FDroneFlightControlSettingsChangedSignature OnFlightControlSettingsChanged;

	UFUNCTION(BlueprintPure, Category="Drone|Flight|VisualBank")
	float GetCurrentVisualBankRollDegrees() const { return CurrentVisualBankRollDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|VisualBank")
	float GetCurrentVisualTiltPitchDegrees() const { return CurrentVisualTiltPitchDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|VisualBank")
	float GetMaximumVisualBankRollDegrees() const { return MaximumVisualBankRollDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|VisualBank")
	float GetMaximumVisualTiltPitchDegrees() const { return MaximumVisualTiltPitchDegrees; }

	/** DroneRotor Tag 또는 Rotor 이름을 가진 외형 Mesh를 다시 수집한다. BP Component를 바꾼 뒤 호출할 수 있다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Flight|RotorVisual")
	void RefreshRotorVisualComponents();

	UFUNCTION(BlueprintPure, Category="Drone|Flight|RotorVisual")
	int32 GetRotorVisualComponentCount() const { return RotorVisualComponents.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Flight|RotorVisual")
	float GetRotorVisualSpinDegreesPerSecond() const { return RotorVisualSpinDegreesPerSecond; }

	UFUNCTION(BlueprintPure, Category="Drone|Camera")
	bool IsFirstPersonViewEnabled() const { return bFirstPersonViewEnabled; }

	/** 3인칭 추적 카메라와 이동 Pitch·Roll을 함께 따르는 1인칭 카메라를 전환한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Camera")
	void SetFirstPersonViewEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Drone|Camera")
	void ToggleFirstPersonView();

	/** 드랍 조준용 상단 Camera다. Payload Component가 사용하며 기존 1/3인칭 상태를 복원한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Camera")
	void SetDropCameraViewEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Drone|Camera")
	bool IsDropCameraViewEnabled() const { return bDropCameraViewEnabled; }

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

	/** 맵 배치 화물을 실제 Actor 상태로 기체 아래에 붙이는 Anchor다. 파생 BP에서 위치를 조정할 수 있다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> PayloadCarryAnchor;

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

	/** Definition의 ImplementedCapabilities에 ReconScan이 있을 때만 활성화된다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDroneReconScanComponent> ReconScanComponent;

	/** Definition의 ImplementedCapabilities에 ImpactDetonation이 있을 때만 활성화된다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDroneImpactDetonationComponent> ImpactDetonationComponent;

	/** Definition의 ImplementedCapabilities에 PayloadDrop이 있을 때만 활성화된다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDronePayloadDropComponent> PayloadDropComponent;

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

	/** 임시 Greybox 기본키는 Left Mouse Button이다. 최종 입력 방식은 아직 미정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> PrimaryRoleAbilityAction;

	/** 임시 Greybox 기본키는 Right Mouse Button이다. 최종 입력 방식은 아직 미정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> SecondaryRoleAbilityAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input", meta=(AllowPrivateAccess="true"))
	int32 PrototypeMappingPriority = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Movement", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float PrototypeYawRateDegreesPerSecond = 90.0f;

	/**
	 * false면 선택 화면의 DA_Drone_* FlightProfile을 사용한다.
	 * true면 이 Pawn Blueprint의 아래 Profile로 속도·가속·Yaw·기울기·체력·시작 시점을 모두 덮어쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|FlightProfileOverride", meta=(AllowPrivateAccess="true"))
	bool bOverrideDefinitionFlightProfileInBlueprint = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|FlightProfileOverride",
		meta=(EditCondition="bOverrideDefinitionFlightProfileInBlueprint", ShowOnlyInnerProperties, AllowPrivateAccess="true"))
	FDroneFlightProfile BlueprintFlightProfileOverride;

	/** 쉬운 조작은 즉시 수평 이동하고 World Up 고도를 사용한다. 파생 BP에서 배율을 조정할 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|ControlModes", meta=(AllowPrivateAccess="true"))
	FDroneControlModeTuning AssistedEasyTuning;

	/** 실제 조작형은 감속 보조가 적고 기체 Root 기울기 및 Local Up을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|ControlModes", meta=(AllowPrivateAccess="true"))
	FDroneControlModeTuning ManualRealisticGreyboxTuning;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|HandlingPresets", meta=(AllowPrivateAccess="true"))
	FDroneHandlingPresetTuning StableHandlingTuning;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|HandlingPresets", meta=(AllowPrivateAccess="true"))
	FDroneHandlingPresetTuning BalancedHandlingTuning;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|HandlingPresets", meta=(AllowPrivateAccess="true"))
	FDroneHandlingPresetTuning AgileHandlingTuning;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera|DropView", meta=(AllowPrivateAccess="true"))
	FVector DropViewCameraBoomOffset = FVector(0.0f, 0.0f, 250.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera|DropView", meta=(ClampMin="0.0", ForceUnits="cm", AllowPrivateAccess="true"))
	float DropViewCameraArmLength = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera|DropView", meta=(ClampMin="-89.0", ClampMax="-1.0", ForceUnits="deg", AllowPrivateAccess="true"))
	float DropViewCameraPitchDegrees = -75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Camera", meta=(AllowPrivateAccess="true"))
	bool bStartInFirstPersonView = false;

	/** 좌우 입력 1.0에서 허용할 최대 Roll이다. 쉬운 조작은 외형에만, 실제 조작형은 Root에 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|VisualBank", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg", AllowPrivateAccess="true"))
	float MaximumVisualBankRollDegrees = 18.0f;

	/** 전후 입력 1.0에서 허용할 최대 Pitch다. 전진은 기수 아래, 후진은 기수 위 방향이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|VisualBank", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg", AllowPrivateAccess="true"))
	float MaximumVisualTiltPitchDegrees = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|VisualBank", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float VisualBankInterpolationSpeed = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|VisualBank", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float VisualBankReturnSpeed = 5.0f;

	/** Rotor 회전은 외형 Component만 움직이며 Pawn 이동·Collision에는 영향을 주지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|RotorVisual", meta=(AllowPrivateAccess="true"))
	bool bRotorVisualSpinEnabled = true;

	/** 현재 Greybox 회전 속도다. 각 역할 Drone BP에서 모델과 화면 체감에 맞춰 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|RotorVisual", meta=(ClampMin="0.0", ForceUnits="deg/s", AllowPrivateAccess="true"))
	float RotorVisualSpinDegreesPerSecond = 1440.0f;

	/** Mesh의 로컬 회전축. 현재 Drone Pack Rotor는 로컬 Z축을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|RotorVisual", meta=(AllowPrivateAccess="true"))
	FVector RotorVisualLocalSpinAxis = FVector::UpVector;

	/** 인접 Rotor를 반대 방향으로 돌려 Quad/Hexa Rotor의 기본 시각 방향을 만든다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|RotorVisual", meta=(AllowPrivateAccess="true"))
	bool bAlternateRotorVisualSpinDirections = true;

	/** 이 Component Tag를 가진 Static Mesh를 Rotor로 수집한다. 기존 BP는 이름에 Rotor가 있어도 자동 인식한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|RotorVisual", meta=(AllowPrivateAccess="true"))
	FName RotorVisualComponentTag = TEXT("DroneRotor");

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
	void TriggerPrimaryRoleAbilityFromInput(const FInputActionValue& Value);
	void TriggerSecondaryRoleAbilityFromInput(const FInputActionValue& Value);
	void AdjustCameraPitch(float PitchDeltaDegrees);
	void ApplyCameraViewMode();
	void RefreshVisualTiltAttachments();
	void ApplyRuntimeFlightTuning();
	void UpdateControlAttitude(float DeltaSeconds);
	void UpdateVisualBank(float DeltaSeconds);
	void UpdateRotorVisuals(float DeltaSeconds);
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
	TArray<TWeakObjectPtr<UStaticMeshComponent>> RotorVisualComponents;
	bool bFirstPersonViewEnabled = false;
	bool bDropCameraViewEnabled = false;
	bool bFirstPersonViewBeforeDropCamera = false;
	FRotator CameraBoomRotationBeforeDropCamera = FRotator::ZeroRotator;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Flight|Control")
	EDroneControlMode CurrentControlMode = EDroneControlMode::AssistedEasy;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Flight|Control")
	EDroneHandlingPreset CurrentHandlingPreset = EDroneHandlingPreset::Balanced;

	/** Data Asset의 원본값이다. 모드/프리셋 전환 시 누적 곱셈하지 않고 이 값에서 다시 계산한다. */
	float BaseMaxSpeedCentimetersPerSecond = 1200.0f;
	float BaseAccelerationCentimetersPerSecondSquared = 2400.0f;
	float BaseDecelerationCentimetersPerSecondSquared = 3000.0f;
	float BaseTurningBoost = 8.0f;
	float BaseYawRateDegreesPerSecond = 90.0f;
	float BaseMaximumVisualBankRollDegrees = 18.0f;
	float BaseMaximumVisualTiltPitchDegrees = 14.0f;

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

	/** NAME_None이면 아직 FLOW-05 Definition이 적용되지 않은 기본 Prototype 상태다. */
	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Flight|Profile")
	FName AppliedDroneId = NAME_None;
};
