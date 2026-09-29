#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "DroneBreakableWallPanel.generated.h"

class UInstancedStaticMeshComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FHitResult;

/**
 * Chaos Geometry Collection 자산이 준비되기 전, 국소 파괴/충격 방향/복구 규칙을 검증하는 격자형 벽이다.
 * 온전한 조각은 ISM 한 개로 유지하고, 맞은 조각만 물리 StaticMeshComponent로 전환한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneBreakableWallPanel : public AActor
{
	GENERATED_BODY()

public:
	ADroneBreakableWallPanel();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Drone|Physics|Breakable Wall")
	void RebuildWall();

	/** 반환값은 이번 호출에서 새로 파괴된 조각 수다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Physics|Breakable Wall")
	int32 BreakWallAtWorldLocation(FVector WorldLocation, float RadiusCentimeters, FVector ImpulseDirection);

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Drone|Physics|Breakable Wall")
	void ResetWall();

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Breakable Wall")
	int32 GetIntactPieceCount() const;

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Breakable Wall")
	int32 GetBrokenPieceCount() const { return BrokenPieceIndices.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Breakable Wall")
	int32 GetLivePhysicsPieceCount() const { return PhysicsPieces.Num(); }

	UFUNCTION(BlueprintPure, Category="Drone|Physics|Breakable Wall")
	FVector GetConfiguredWallSizeCentimeters() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall")
	TObjectPtr<UInstancedStaticMeshComponent> IntactPieces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Visual")
	TObjectPtr<UStaticMesh> PieceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Layout", meta=(ClampMin="1", ClampMax="24"))
	int32 Columns = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Layout", meta=(ClampMin="1", ClampMax="24"))
	int32 Rows = 4;

	/** X=두께, Y=가로, Z=높이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Layout", meta=(ClampMin="10.0", ForceUnits="cm"))
	FVector PieceSizeCentimeters = FVector(35.0f, 90.0f, 90.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Layout", meta=(ClampMin="0.0", ForceUnits="cm"))
	float PieceGapCentimeters = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Damage", meta=(ClampMin="1.0", ForceUnits="cm"))
	float LocalBreakRadiusCentimeters = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Damage", meta=(ClampMin="0.0"))
	float MinimumPointDamage = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Physics", meta=(ClampMin="0.0"))
	float BreakImpulseStrength = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Physics", meta=(ClampMin="0.0"))
	float RandomImpulseStrength = 120.0f;

	/** 0이면 Reset/Actor 종료 전까지 유지한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Physics", meta=(ClampMin="0.0", ForceUnits="s"))
	float DebrisLifetimeSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Impact")
	bool bBreakOnImpact = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Physics|Breakable Wall|Impact", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float MinimumImpactSpeed = 250.0f;

private:
	void RebuildIntactInstances();
	void SpawnPhysicsPiece(int32 PieceIndex, const FVector& ImpulseDirection);
	void ExpirePhysicsPiece(UStaticMeshComponent* Piece);
	void DestroyPhysicsPieces();

	UFUNCTION()
	void HandleIntactPieceHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	TArray<FTransform> PieceTransforms;
	TSet<int32> BrokenPieceIndices;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PhysicsPieces;

	TArray<FTimerHandle> DebrisTimerHandles;
};
