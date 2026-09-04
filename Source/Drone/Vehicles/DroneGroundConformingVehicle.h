#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneGroundConformingVehicle.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Chaos Vehicle을 사용하지 않는 Greybox용 4점 지면 추종 차량이다.
 *
 * 앞좌/앞우/뒤좌/뒤우 네 지점의 Visibility Trace로 지면 높이와 법선을 읽고
 * Actor의 높이, Pitch, Roll을 보간한다. 완전한 타이어·질량·마찰 물리가 아니라
 * 울퉁불퉁한 길을 따라가는 차량과 차량 탑재 자동포탑을 빠르게 검증하기 위한 기반이다.
 */
UCLASS(Blueprintable)
class ADroneGroundConformingVehicle : public AActor
{
	GENERATED_BODY()

public:
	ADroneGroundConformingVehicle();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	UBoxComponent* GetVehicleCollision() const { return VehicleCollision; }

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	UStaticMeshComponent* GetVehicleBodyMesh() const { return VehicleBodyMesh; }

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	USceneComponent* GetTurretMount() const { return TurretMount; }
	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	int32 GetLastGroundContactCount() const { return LastGroundContactCount; }

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	float GetRideHeight() const { return RideHeight; }

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	float GetMaximumGroundAngleDegrees() const { return MaximumGroundAngleDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	TArray<UStaticMeshComponent*> GetWheelMeshes() const;

	/** 실제 프레임 이동거리에서 계산한 차량 전진축 속도다. 후진은 음수다. */
	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	float GetCurrentForwardSpeedCentimetersPerSecond() const { return CurrentForwardSpeedCentimetersPerSecond; }

	/** 바퀴 반지름과 이동거리로 누적한 회전각이다. 전진은 증가하고 후진은 감소한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	float GetCurrentWheelRotationDegrees() const { return CurrentWheelRotationDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	float GetWheelVisualSpinDirectionMultiplier() const { return WheelVisualSpinDirectionMultiplier; }

	/** Throttle/Steering은 각각 -1~1이다. Auto Drive가 켜진 동안에는 수동값을 무시한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Vehicle|GroundConforming")
	void SetDriveInput(float Throttle, float Steering);

	/** 맵 Greybox에서 별도 Controller 없이 왕복 주행을 확인하는 시험용 토글이다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Vehicle|GroundConforming")
	void SetGreyboxAutoDriveEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Drone|Vehicle|GroundConforming")
	bool IsGreyboxAutoDriveEnabled() const { return bGreyboxAutoDriveEnabled; }

	/** 테스트·Construction 도구가 한 프레임을 기다리지 않고 현재 지면에 맞출 때 사용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Vehicle|GroundConforming")
	bool RefreshGroundConformNow(bool bSnapToGround = true);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> VehicleCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> VehicleBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> FrontLeftWheel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> FrontRightWheel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> RearLeftWheel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> RearRightWheel;

	/** 차량용 자동포탑은 이 Component 또는 이 Actor에 Attach하면 지면 추종 회전을 함께 따른다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> TurretMount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="10.0", ForceUnits="cm"))
	float HalfWheelbase = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="10.0", ForceUnits="cm"))
	float HalfTrackWidth = 78.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="0.0", ForceUnits="cm"))
	float TraceStartHeight = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="10.0", ForceUnits="cm"))
	float TraceLength = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="1.0", ForceUnits="cm"))
	float RideHeight = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="1.0", ForceUnits="cm"))
	float WheelRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="0.0", ClampMax="60.0", ForceUnits="deg"))
	float MaximumGroundAngleDegrees = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="0.0"))
	float HeightInterpolationSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Suspension", meta=(ClampMin="0.0"))
	float RotationInterpolationSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Drive", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MaximumDriveSpeed = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Drive", meta=(ClampMin="0.0", ForceUnits="deg/s"))
	float MaximumTurnRateDegreesPerSecond = 45.0f;

	/** Wheel Mesh의 로컬 회전축 부호다. 현재 Greybox Cylinder는 수동 화면 확인 결과 +1이 전진 구름 방향이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Drive", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float WheelVisualSpinDirectionMultiplier = 1.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Drone|Vehicle|Greybox")
	bool bGreyboxAutoDriveEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Greybox", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float GreyboxAutoDriveSpeed = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Vehicle|Greybox", meta=(ClampMin="100.0", ForceUnits="cm"))
	float GreyboxAutoDriveDistance = 1050.0f;

private:
	struct FGroundContact
	{
		bool bBlockingHit = false;
		FVector Point = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
	};

	bool UpdateGroundConforming(float DeltaSeconds, bool bSnapToGround);
	void UpdateWheelVisuals(const FGroundContact (&Contacts)[4]);
	void CaptureWheelVisualBaseRotations();
	void UpdateWheelRollingVisuals(const FVector& PreviousLocation, const FVector& CurrentLocation, float DeltaSeconds);
	void InitializeDriveReference();

	float DriveThrottle = 0.0f;
	float DriveSteering = 0.0f;
	float DesiredHeadingYaw = 0.0f;
	bool bDriveReferenceInitialized = false;
	FVector GreyboxAutoDriveOrigin = FVector::ZeroVector;
	FVector GreyboxAutoDriveForward = FVector::ForwardVector;
	float GreyboxAutoDriveDirection = 1.0f;
	FQuat WheelVisualBaseRotations[4] = {FQuat::Identity, FQuat::Identity, FQuat::Identity, FQuat::Identity};
	bool bWheelVisualBaseRotationsCaptured = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Vehicle|Debug")
	float CurrentForwardSpeedCentimetersPerSecond = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Vehicle|Debug")
	float CurrentWheelRotationDegrees = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Vehicle|Debug")
	int32 LastGroundContactCount = 0;
};
