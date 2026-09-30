#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneCollisionResponseComponent.generated.h"

class AActor;
class ADronePrototypePawn;
class UPrimitiveComponent;

/**
 * 비행 Drone의 공통 Blocking 충돌 반발과 그물 얽힘 상태를 처리한다.
 * 수평에 가까운 벽/기둥/구조물은 법선 반대 방향으로 분리하고, 그물은 반발 대신
 * 감속·조종 저하·자세 교란·하강을 적용한다. Ground UGV에는 두 규칙을 적용하지 않는다.
 */
UCLASS(ClassGroup=(Drone), meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneCollisionResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneCollisionResponseComponent();

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response")
	bool IsCollisionResponseEnabled() const { return bCollisionResponseEnabled; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response")
	int32 GetResolvedImpactCount() const { return ResolvedImpactCount; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response")
	FVector GetLastResolvedVelocity() const { return LastResolvedVelocity; }

	/** 접촉 피드백은 조종 Root/피격 Camera Shake와 분리된 부드러운 외형 기울기다. */
	UFUNCTION(BlueprintPure, Category="Drone|Physics|Contact Feedback")
	FRotator GetContactVisualRotation() const { return CurrentContactVisualRotation; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response")
	float GetMinimumWallContactSpeed() const { return MinimumImpactSpeedCentimetersPerSecond; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response|Rotor Probes")
	bool UsesRotorContactProbes() const { return bUseRotorContactProbes; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net Entanglement")
	bool IsNetEntangled() const { return NetEntanglementRemainingSeconds > 0.0f; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net Entanglement")
	bool IsNetCaptured() const { return bNetCaptured; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net Entanglement")
	float GetNetEntanglementSeverity() const { return NetEntanglementSeverity; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net Entanglement")
	float GetNetEntanglementRemainingSeconds() const { return NetEntanglementRemainingSeconds; }

	/** 이동·Yaw·Acro 추력에 곱하는 현재 조종 가능 비율이다. 정상 상태는 1이다. */
	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net Entanglement")
	float GetFlightControlEffectivenessMultiplier() const;

	UFUNCTION(BlueprintCallable, Category="Drone|Physics|Collision Response|Greybox")
	void ConfigureCollisionResponse(
		bool bEnabled,
		float NewRestitution,
		float NewMinimumImpactSpeed,
		float NewMinimumSeparationSpeed,
		float NewMaximumResponseSpeed);

	/** 자동화와 런타임 Hit가 공유하는 순수 반사 계산이다. */
	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response")
	static FVector ComputeReflectedVelocity(
		FVector IncomingVelocity,
		FVector ImpactNormal,
		float Restitution,
		float MinimumSeparationSpeed,
		float MaximumResponseSpeed);

	/** 약한 접촉은 작게, 강한 충돌은 크게 표면 밖으로 분리하는 거리다. */
	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response")
	static float ComputeSpeedScaledSeparationDistance(
		float InwardNormalSpeed,
		float MinimumDistance,
		float MaximumDistance,
		float ReferenceImpactSpeed);

	/** 접촉 위치와 벽 법선으로 계산한 World axis-angle 회전량(축 * 각도, deg)이다. */
	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response|Rotor Probes")
	static FVector ComputeContactAngularKickAxisAngleDegrees(
		FVector ContactOffsetFromCenter,
		FVector ImpactNormal,
		float InwardNormalSpeed,
		float MaximumAngularKickDegrees,
		float ReferenceImpactSpeed,
		float ReferenceLeverArmCentimeters);

	/** 바닥/천장은 제외하고 벽·기둥·구조물에 가까운 법선인지 판정한다. */
	UFUNCTION(BlueprintPure, Category="Drone|Physics|Collision Response")
	static bool IsWallLikeSurfaceNormal(FVector ImpactNormal, float MaximumAbsoluteNormalZ);

	/** 그물 Actor가 충돌 지점과 충돌 속도를 전달하는 공용 진입점이다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Physics|Net Entanglement")
	bool ApplyNetEntanglement(FVector ContactPoint, FVector ContactNormal, float ImpactSpeedCentimetersPerSecond);

	UFUNCTION(BlueprintCallable, Category="Drone|Physics|Net Entanglement")
	void ClearNetEntanglement();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response")
	bool bCollisionResponseEnabled = true;

	/** 바닥 착륙과 천장 접촉은 건드리지 않고 수평 벽·기둥·구조물만 반발시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response")
	bool bOnlyRespondToWallLikeSurfaces = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaximumWallSurfaceAbsoluteNormalZ = 0.72f;

	/** 저속 접촉에서도 관통을 막기 위한 최소 분리 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ClampMax="5.0", ForceUnits="cm"))
	float MinimumSurfaceSeparationDistanceCentimeters = 0.25f;

	/** 기준 속도 이상의 강한 충돌에서 사용하는 최대 분리 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ClampMax="30.0", ForceUnits="cm"))
	float SurfaceSeparationDistanceCentimeters = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float ReferenceImpactSpeedForMaximumSeparation = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Restitution = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumImpactSpeedCentimetersPerSecond = 1.0f;

	/** 고정 Kick이 아니라 최대 반발속도에 비례해 보조하는 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumSeparationSpeedCentimetersPerSecond = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MaximumResponseSpeedCentimetersPerSecond = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ForceUnits="s"))
	float ResponseCooldownSeconds = 0.04f;

	/** Collision Root보다 바깥의 Rotor/날개 끝까지 벽 접촉을 검사한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Rotor Probes")
	bool bUseRotorContactProbes = true;

	/** 기체 Local 좌표의 날개/로터 끝 검사점. 기체 Mesh 크기에 맞춰 Blueprint에서 조정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Rotor Probes")
	TArray<FVector> RotorContactProbeLocalOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Rotor Probes", meta=(ClampMin="1.0", ClampMax="50.0", ForceUnits="cm"))
	float RotorContactProbeRadiusCentimeters = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Rotor Probes", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg"))
	float MaximumRotorContactAngularKickDegrees = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Rotor Probes", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float RotorContactAngularReferenceSpeedCentimetersPerSecond = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Rotor Probes", meta=(ClampMin="1.0", ForceUnits="cm"))
	float RotorContactReferenceLeverArmCentimeters = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Rotor Probes")
	TEnumAsByte<ECollisionChannel> RotorContactTraceChannel = ECC_Visibility;

	/** 시험 Pawn이 충돌한 파괴 대상에 같은 Hit 위치의 Point Damage를 전달한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Impact Damage")
	bool bApplyImpactDamageToOtherActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Impact Damage", meta=(ClampMin="0.0"))
	float ImpactDamageToOtherActor = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement")
	bool bNetEntanglementEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumNetContactSpeedCentimetersPerSecond = 75.0f;

	/** 이 속도 이상의 첫 접촉은 즉시 포획 단계까지 올라간다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float ReferenceNetCaptureSpeedCentimetersPerSecond = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.1", ClampMax="30.0", ForceUnits="s"))
	float NetEntanglementDurationSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0", ClampMax="1.0"))
	float NetCaptureSeverityThreshold = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumNetControlEffectiveness = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0"))
	float NetHorizontalVelocityDampingPerSecond = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0", ForceUnits="cm/s^2"))
	float NetDownwardAccelerationCentimetersPerSecondSquared = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float NetMaximumUpwardSpeedCentimetersPerSecond = 80.0f;

	/** 이전 자산 직렬화 호환용. Root 회전 교란 대신 NetSwayMaximumDegrees를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Legacy", meta=(ClampMin="0.0", ClampMax="180.0", ForceUnits="deg/s", ToolTip="호환용 미사용 값. 그물 완만한 기울기 각도를 조정하세요."))
	float NetAttitudeDisturbanceDegreesPerSecond = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement")
	bool bAutoReleaseNetEntanglement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0", ClampMax="2.0", ForceUnits="s"))
	float NetContactAccumulationCooldownSeconds = 0.12f;

	/** 같은 벽을 계속 누르는 접촉을 새 충격으로 재생하지 않는 접촉 해제 여유다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Contact Feedback", meta=(ClampMin="0.01", ForceUnits="s", DisplayName="벽 접촉 해제 여유 시간"))
	float WallContactReleaseGraceSeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Contact Feedback", meta=(ClampMin="0.01", ForceUnits="s", DisplayName="표면 분리 보간 시간"))
	float SurfaceSeparationResponseTimeSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Contact Feedback", meta=(ClampMin="0.05", ForceUnits="s", DisplayName="벽 접촉 기울기 지속 시간"))
	float WallContactVisualDurationSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Contact Feedback", meta=(ClampMin="0.01", ForceUnits="s", DisplayName="접촉 외형 기울기 보간 시간"))
	float ContactVisualResponseTimeSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.01", ForceUnits="s", DisplayName="그물 감속 반응 시간"))
	float NetResponseTimeSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0", ClampMax="15.0", ForceUnits="deg", DisplayName="그물 완만한 기울기 각도"))
	float NetSwayMaximumDegrees = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net Entanglement", meta=(ClampMin="0.0", ClampMax="2.0", DisplayName="그물 완만한 기울기 주파수"))
	float NetSwayFrequencyHertz = 0.35f;

private:
	UFUNCTION()
	void HandleOwnerHit(
		AActor* SelfActor,
		AActor* OtherActor,
		FVector NormalImpulse,
		const FHitResult& Hit);

	bool ResolveWallContact(
		ADronePrototypePawn* Drone,
		AActor* OtherActor,
		const FHitResult& Hit,
		FVector IncomingVelocity,
		bool bAllowPreviousVelocity = true);

	void TraceRotorWingContacts(ADronePrototypePawn* Drone, class UFloatingPawnMovement* Movement);
	void UpdateContactFeedback(ADronePrototypePawn* Drone, float DeltaSeconds);

	float LastResolvedWorldSeconds = -BIG_NUMBER;
	float LastNetContactWorldSeconds = -BIG_NUMBER;
	float NetDisturbancePhaseRadians = 0.0f;
	FVector LastObservedFlightVelocity = FVector::ZeroVector;
	TWeakObjectPtr<AActor> LastWallContactActor;
	FVector LastWallContactNormal = FVector::ZeroVector;
	float LastWallTouchWorldSeconds = -BIG_NUMBER;
	FVector PendingSurfaceSeparation = FVector::ZeroVector;
	bool bApplyingSurfaceSeparation = false;
	float WallContactVisualTimeRemaining = 0.0f;
	FRotator WallContactVisualPeak = FRotator::ZeroRotator;
	FRotator CurrentContactVisualRotation = FRotator::ZeroRotator;
	float SmoothedNetSeverity = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Net Entanglement")
	float NetEntanglementSeverity = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Net Entanglement")
	float NetEntanglementRemainingSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Net Entanglement")
	bool bNetCaptured = false;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Collision Response")
	int32 ResolvedImpactCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Collision Response")
	FVector LastResolvedVelocity = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Collision Response")
	FVector LastContactAngularKickAxisAngleDegrees = FVector::ZeroVector;
};
