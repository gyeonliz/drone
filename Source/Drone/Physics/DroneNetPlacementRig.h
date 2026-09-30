#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "DroneNetPlacementRig.generated.h"

class UInstancedStaticMeshComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
struct FHitResult;

/**
 * 네 모서리와 처짐을 Instance 값으로 조정하는 그물 배치/파괴 Greybox다.
 * 현재는 Cube Strand를 사용하며, 배치 검증이 끝나면 같은 Actor 자리를 Dataflow/Chaos 자산으로 교체한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneNetPlacementRig : public AActor
{
	GENERATED_BODY()

public:
	ADroneNetPlacementRig();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Drone|Physics|Net")
	void RebuildNet();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Drone|Physics|Net")
	void BreakNetGreybox();

	/** 충돌 지점 주변의 Strand Segment만 제거한다. 반환값은 이번 호출에서 새로 끊어진 Segment 수다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Physics|Net")
	int32 BreakNetAtWorldLocation(FVector WorldLocation, float RadiusCentimeters);

	/** 실제 Hit Delegate와 자동화가 공유하는 Drone 접촉 처리다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Physics|Net|Impact")
	bool ApplyDroneImpact(
		AActor* OtherActor,
		FVector ContactPoint,
		FVector ContactNormal,
		FVector ContactVelocity);

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Drone|Physics|Net")
	void ResetNetGreybox();

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	bool IsNetBroken() const { return bNetBroken; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	int32 GetStrandInstanceCount() const;

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	int32 GetIntactStrandSegmentCount() const;

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	int32 GetBrokenStrandSegmentCount() const { return BrokenSegmentIndices.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	int32 GetLiveDetachedPhysicsSegmentCount() const { return DetachedPhysicsSegments.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	int32 GetAnchorCount() const { return 4; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net|Impact")
	bool EntanglesDronesOnImpact() const { return bEntangleDronesOnImpact; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net|Impact")
	bool BreaksOnDroneImpact() const { return bBreakOnImpact; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	FVector GetTopLeftCorner() const { return TopLeftCorner; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	FVector GetTopRightCorner() const { return TopRightCorner; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	FVector GetBottomLeftCorner() const { return BottomLeftCorner; }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Net")
	FVector GetBottomRightCorner() const { return BottomRightCorner; }

protected:
	virtual void BeginPlay() override;

	/** 줄마다 Spring Arm이 수축/복귀하면 화면이 떨리므로 기본적으로 Camera만 통과시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Camera", meta=(DisplayName="그물이 카메라 암을 막음"))
	bool bBlockCamera = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net")
	TObjectPtr<UInstancedStaticMeshComponent> NetStrands;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net")
	TObjectPtr<UStaticMeshComponent> TopLeftAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net")
	TObjectPtr<UStaticMeshComponent> TopRightAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net")
	TObjectPtr<UStaticMeshComponent> BottomLeftAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net")
	TObjectPtr<UStaticMeshComponent> BottomRightAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Placement")
	FVector TopLeftCorner = FVector(0.0f, -300.0f, 330.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Placement")
	FVector TopRightCorner = FVector(0.0f, 300.0f, 330.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Placement")
	FVector BottomLeftCorner = FVector(0.0f, -300.0f, 30.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Placement")
	FVector BottomRightCorner = FVector(0.0f, 300.0f, 30.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Layout", meta=(ClampMin="2", ClampMax="32"))
	int32 HorizontalStrandCount = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Layout", meta=(ClampMin="2", ClampMax="32"))
	int32 VerticalStrandCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Layout", meta=(ClampMin="1", ClampMax="16"))
	int32 SegmentsPerStrand = 6;

	/** 그물 중심이 Actor +X 방향으로 늘어지는 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Layout", meta=(ClampMin="0.0", ForceUnits="cm"))
	float SagDepthCentimeters = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Layout", meta=(ClampMin="0.5", ForceUnits="cm"))
	float StrandThicknessCentimeters = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Damage", meta=(ClampMin="1.0"))
	float BreakDamageThreshold = 100.0f;

	/** Point Damage 또는 비행체 충돌 한 번이 주변 그물을 끊는 반경이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Damage", meta=(ClampMin="1.0", ForceUnits="cm"))
	float LocalBreakRadiusCentimeters = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Damage", meta=(ClampMin="0.0"))
	float MinimumPointDamage = 1.0f;

	/** 비행 Drone 충돌은 기본적으로 절단이 아니라 Rotor 얽힘 상태를 만든다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Impact")
	bool bEntangleDronesOnImpact = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Impact", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumEntanglementSpeed = 75.0f;

	/** 진단용 절단 연출. 기본 Off이며 탄환 Point Damage에 의한 국소 절단은 별도로 유지한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Impact")
	bool bBreakOnImpact = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Impact", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumImpactSpeed = 250.0f;

	/** 끊어진 Segment를 숨기는 대신 잠시 물리 조각으로 떨어뜨려 충돌 반응을 눈으로 확인한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Physics")
	bool bSpawnDetachedStrandPhysics = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Physics", meta=(ClampMin="0.0"))
	float DetachedStrandImpulseStrength = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Physics", meta=(ClampMin="0.0"))
	float DetachedStrandRandomImpulseStrength = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Physics", meta=(ClampMin="0.0", ForceUnits="s"))
	float DetachedStrandLifetimeSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Physics", meta=(ClampMin="1", ClampMax="64"))
	int32 MaximumDetachedPhysicsSegmentsPerBreak = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Net|Collision")
	bool bEnableStrandCollision = true;

private:
	FVector EvaluateNetPoint(float HorizontalAlpha, float VerticalAlpha) const;
	void AddStrandSegment(const FVector& Start, const FVector& End);
	void RebuildVisibleSegments();
	void RefreshAnchorVisuals();
	int32 BreakNetAtWorldLocationInternal(
		FVector WorldLocation,
		float RadiusCentimeters,
		FVector ImpulseDirection);
	void SpawnDetachedPhysicsSegment(int32 SegmentIndex, const FVector& ImpulseDirection);
	void ExpireDetachedPhysicsSegment(UStaticMeshComponent* Segment);
	void DestroyDetachedPhysicsSegments();

	UFUNCTION()
	void HandleStrandHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	TArray<FTransform> StrandSegmentTransforms;
	TSet<int32> BrokenSegmentIndices;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DetachedPhysicsSegments;

	TArray<FTimerHandle> DetachedSegmentTimerHandles;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Net|Damage")
	float AccumulatedDamage = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Physics|Net|Damage")
	bool bNetBroken = false;
};
