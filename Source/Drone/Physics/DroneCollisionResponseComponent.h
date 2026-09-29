#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneCollisionResponseComponent.generated.h"

class AActor;
class UPrimitiveComponent;

/**
 * 벽/장애물 충돌 시험에서 비행 Drone을 충돌 법선 반대 방향으로 되돌리는 경량 Greybox다.
 * 기본값은 비활성이라 기존 Drone 동작을 바꾸지 않으며, 시험용 파생 Blueprint에서만 켠다.
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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response")
	bool bCollisionResponseEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Restitution = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumImpactSpeedCentimetersPerSecond = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumSeparationSpeedCentimetersPerSecond = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MaximumResponseSpeedCentimetersPerSecond = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response", meta=(ClampMin="0.0", ForceUnits="s"))
	float ResponseCooldownSeconds = 0.08f;

	/** 시험 Pawn이 충돌한 파괴 대상에 같은 Hit 위치의 Point Damage를 전달한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Impact Damage")
	bool bApplyImpactDamageToOtherActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Collision Response|Impact Damage", meta=(ClampMin="0.0"))
	float ImpactDamageToOtherActor = 10.0f;

private:
	UFUNCTION()
	void HandleOwnerHit(
		AActor* SelfActor,
		AActor* OtherActor,
		FVector NormalImpulse,
		const FHitResult& Hit);

	float LastResolvedWorldSeconds = -BIG_NUMBER;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Collision Response")
	int32 ResolvedImpactCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Collision Response")
	FVector LastResolvedVelocity = FVector::ZeroVector;
};
