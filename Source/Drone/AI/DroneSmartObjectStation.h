#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "AI/DroneAITypes.h"
#include "AI/Weapons/DroneNPCProjectile.h"
#include "DroneSmartObjectStation.generated.h"

class UArrowComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class USmartObjectComponent;
class USmartObjectDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDroneMGTurretUseChangedSignature,
	AActor*, UserActor,
	AActor*, TargetActor,
	bool, bInUse);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDroneMGTurretShotSignature,
	FVector, TraceStart,
	FVector, TraceEnd,
	AActor*, HitActor);

/**
 * Patrol, Guard, Ambient, Cover, MG Turret를 Map에 배치하는 프로젝트 소유 Host Actor다.
 *
 * 이 Actor는 외형과 Smart Object Component 경계만 제공한다. 실제 Slot 수·Activity Tag·
 * Gameplay Interaction StateTree는 Smart Object Definition Asset에서 설정한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneSmartObjectStation : public AActor
{
	GENERATED_BODY()

public:
	ADroneSmartObjectStation();

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	USmartObjectComponent* GetSmartObjectComponent() const { return SmartObjectComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	USkeletalMeshComponent* GetStationMesh() const { return StationMesh; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	USceneComponent* GetMGTurretAimPivot() const { return MGTurretAimPivot; }

	/** 프로젝트 소유 Authoring Tool과 Blueprint가 Engine Component 내부를 직접 만지지 않게 한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	void SetSmartObjectDefinition(USmartObjectDefinition* Definition);

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	USmartObjectDefinition* GetSmartObjectDefinition() const;

	/** MG Station만 후보 Mesh를 표시하고 일반 이동 지점은 nullptr로 비워 둔다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	void SetStationSkeletalMesh(USkeletalMesh* SkeletalMesh);

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	USkeletalMesh* GetStationSkeletalMesh() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	UArrowComponent* GetSlotFacingPreview() const { return SlotFacingPreview; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	EDroneSmartObjectActivity GetActivity() const { return Activity; }

	/** Definition의 Activity Tag에 넣어야 하는 Native Tag를 반환한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	FGameplayTag GetExpectedActivityTag() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	bool HasSmartObjectDefinition() const;

	/** Occupied 전환 뒤 호출한다. 같은 사용자만 현재 MG를 계속 조작할 수 있다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	bool BeginMGTurretUse(AActor* UserActor, AActor* TargetActor);

	/** StateTree Tick에서 표적을 향해 Pivot을 갱신하고 Cooldown이 끝나면 Trace를 발사한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	bool UpdateMGTurretUse(AActor* UserActor, AActor* TargetActor);

	/** UserActor가 nullptr이면 UnPossess·EndPlay용 강제 정리로 처리한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	void EndMGTurretUse(AActor* UserActor);

	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	void ConfigureMGTurretGreybox(float InRange, float InCooldownSeconds);

	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG")
	void ConfigureMGTurretDamageGreybox(float InDamage);

	/** true면 회피 가능한 Projectile, false면 기존 즉시 Trace를 사용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG|Projectile")
	void ConfigureMGTurretProjectileGreybox(bool bInUseProjectileBallistics, float InProjectileSpeed);

	/** 0도면 정확 사격, 값이 커질수록 목표 중심 주위 원뿔 내부로 무작위 사격한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|MG|Accuracy")
	void ConfigureMGTurretAccuracyGreybox(float InSpreadHalfAngleDegrees);

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	float GetMGTurretDamage() const { return MGTurretDamage; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Projectile")
	bool UsesMGTurretProjectileBallistics() const { return bUseMGTurretProjectileBallistics; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Projectile")
	float GetMGTurretProjectileSpeed() const { return MGTurretProjectileSpeed; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Accuracy")
	float GetMGTurretSpreadHalfAngleDegrees() const { return MGTurretSpreadHalfAngleDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Projectile|Debug")
	int32 GetMGTurretProjectileSpawnCount() const { return MGTurretProjectileSpawnCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Projectile|Debug")
	ADroneNPCProjectile* GetLastMGTurretProjectile() const { return LastMGTurretProjectile.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	bool IsMGTurretInUse() const { return bMGTurretInUse && MGTurretUser.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	AActor* GetMGTurretUser() const { return MGTurretUser.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	AActor* GetMGTurretTarget() const { return MGTurretTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG")
	FVector GetMGTurretAimPoint() const { return MGTurretAimPoint; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Debug")
	int32 GetMGTurretOccupationCount() const { return MGTurretOccupationCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Debug")
	int32 GetMGTurretReleaseCount() const { return MGTurretReleaseCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Debug")
	int32 GetMGTurretTraceAttemptCount() const { return MGTurretTraceAttemptCount; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Debug")
	int32 GetMGTurretTargetHitCount() const { return MGTurretTargetHitCount; }

	UPROPERTY(BlueprintAssignable, Category="Drone|AI|MG")
	FDroneMGTurretUseChangedSignature OnMGTurretUseChanged;

	/** Blueprint는 이 Event에서 Muzzle Flash·Sound·탄흔을 붙이고 Trace 계산은 중복하지 않는다. */
	UPROPERTY(BlueprintAssignable, Category="Drone|AI|MG")
	FDroneMGTurretShotSignature OnMGTurretShot;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Components")
	TObjectPtr<USceneComponent> StationRoot;

	/** 최종 Turret Bone 구조와 분리된 Greybox 조준 Pivot이다. BP에서 별도 Mesh를 자식으로 추가해도 된다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Components")
	TObjectPtr<USceneComponent> MGTurretAimPivot;

	/** MG 외형이 필요할 때만 Mesh를 지정한다. Patrol/Ambient Point는 비워도 된다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Components")
	TObjectPtr<USkeletalMeshComponent> StationMesh;

	/** Smart Object Definition Asset을 이 Component의 Definition에 지정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Components")
	TObjectPtr<USmartObjectComponent> SmartObjectComponent;

	/** Editor에서 실제 Slot의 위치와 +X 방향을 맞추기 위한 표시용 Component다. SmartObjectComponent를 따라간다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Components")
	TObjectPtr<UArrowComponent> SlotFacingPreview;

	/** 문서·검증용 역할 값. 검색의 실제 기준은 Definition Activity Tag다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|SmartObject")
	EDroneSmartObjectActivity Activity = EDroneSmartObjectActivity::EnemyPatrol;

	/** AI-MG-02 Greybox 값이며 최종 난이도·실제 MG 제원이 아니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG", meta=(ClampMin="1.0", ForceUnits="cm"))
	float MGTurretRange = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG", meta=(ClampMin="0.01", ForceUnits="s"))
	float MGTurretCooldownSeconds = 0.15f;

	/** 체력 100 기준 회색상자 MG 한 발 피해량이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG", meta=(ClampMin="0.0"))
	float MGTurretDamage = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG", meta=(ForceUnits="cm"))
	FVector MGTurretMuzzleOffset = FVector(100.0f, 0.0f, 120.0f);

	/** 역할 Blueprint의 Details에서 조정하는 MG 사격 원뿔 반각이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Accuracy", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg"))
	float MGTurretSpreadHalfAngleDegrees = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Debug")
	bool bDrawMGTurretDebugTrace = true;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG")
	bool bMGTurretInUse = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG")
	TWeakObjectPtr<AActor> MGTurretUser;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG")
	TWeakObjectPtr<AActor> MGTurretTarget;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG")
	FVector MGTurretAimPoint = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Debug")
	int32 MGTurretOccupationCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Debug")
	int32 MGTurretReleaseCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Debug")
	int32 MGTurretTraceAttemptCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Debug")
	int32 MGTurretTargetHitCount = 0;

	/** true가 Gameplay 기본값이고 false는 기존 Trace 비교·회귀 테스트용이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Projectile")
	bool bUseMGTurretProjectileBallistics = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Projectile")
	TSubclassOf<ADroneNPCProjectile> MGTurretProjectileClass;

	/** 60m에서 약 1.09초가 걸리는 회피 가능한 MG Greybox 탄속이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Projectile", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float MGTurretProjectileSpeed = 5500.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Projectile|Debug")
	int32 MGTurretProjectileSpawnCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Projectile|Debug")
	TWeakObjectPtr<ADroneNPCProjectile> LastMGTurretProjectile;

	double LastMGTurretShotTimeSeconds = -DBL_MAX;

private:
	UFUNCTION()
	void HandleMGTurretProjectileImpact(
		ADroneNPCProjectile* Projectile,
		EDroneNPCProjectileSource Source,
		AActor* HitActor,
		bool bHitIntendedTarget);
};
