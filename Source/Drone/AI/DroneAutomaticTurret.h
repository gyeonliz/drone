#pragma once

#include "CoreMinimal.h"
#include "AI/DroneMGTurretStation.h"
#include "DroneAutomaticTurret.generated.h"

class ADronePrototypePawn;

/** 같은 자동 조준 코어를 차량 장착형과 지면 설치형에서 구분하기 위한 배치 역할이다. */
UENUM(BlueprintType)
enum class EDroneAutomaticTurretMountType : uint8
{
	Emplaced,
	VehicleMounted
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneAutomaticTurretTargetChangedSignature,
	AActor*, PreviousTarget,
	AActor*, NewTarget);

/**
 * 기존 유인 MG의 3분할 회전·발사 파이프라인을 재사용하는 무인 자동포탑 기반이다.
 *
 * Smart Object 점유자는 필요하지 않다. 일정 주기로 살아 있는 Prototype Drone을 찾고,
 * 사거리와 Visibility 시야선을 모두 통과한 표적만 Yaw/Pitch로 추적해 발사한다.
 * 차량에 쓸 때는 Actor 자체를 차량 Mesh/Socket에 Attach하면 부모 Transform을 그대로 따른다.
 */
UCLASS(Abstract, Blueprintable)
class DRONE_API ADroneAutomaticTurret : public ADroneMGTurretStation
{
	GENERATED_BODY()

public:
	ADroneAutomaticTurret();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Drone|AI|AutomaticTurret")
	EDroneAutomaticTurretMountType GetMountType() const { return MountType; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|AutomaticTurret")
	bool IsAutomaticTurretEnabled() const { return bAutomaticTurretEnabled; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|AutomaticTurret")
	AActor* GetAutomaticTarget() const { return AutomaticTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Drone|AI|AutomaticTurret")
	float GetDetectionRange() const { return DetectionRange; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|AutomaticTurret")
	float GetLoseTargetRange() const { return LoseTargetRange; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|AutomaticTurret")
	bool RequiresTargetLineOfSight() const { return bRequireTargetLineOfSight; }

	UFUNCTION(BlueprintCallable, Category="Drone|AI|AutomaticTurret")
	void SetAutomaticTurretEnabled(bool bEnabled);

	/** 즉시 표적을 재검사한다. Blueprint 디버그 버튼과 자동화 테스트에서 함께 사용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|AutomaticTurret")
	AActor* RefreshAutomaticTarget();

	/** Greybox 단계에서 탐지 거리·이탈 거리·검사 간격·시야선 사용 여부를 한 번에 조정한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|AutomaticTurret")
	void ConfigureAutomaticTargetingGreybox(
		float InDetectionRange,
		float InLoseTargetRange,
		float InScanIntervalSeconds,
		bool bInRequireTargetLineOfSight);

	UPROPERTY(BlueprintAssignable, Category="Drone|AI|AutomaticTurret")
	FDroneAutomaticTurretTargetChangedSignature OnAutomaticTargetChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual EDroneNPCProjectileSource GetMGTurretProjectileSource() const override
	{
		return EDroneNPCProjectileSource::AutomaticTurret;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|AI|AutomaticTurret")
	EDroneAutomaticTurretMountType MountType = EDroneAutomaticTurretMountType::Emplaced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|AutomaticTurret")
	bool bAutomaticTurretEnabled = true;

	/** 새 표적을 처음 획득하는 거리다. 실제 발사 거리를 넘지 않게 설정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|AutomaticTurret|Detection", meta=(ClampMin="1.0", ForceUnits="cm"))
	float DetectionRange = 5000.0f;

	/** 이미 잡은 표적을 놓는 거리다. 경계에서 표적이 빠르게 깜박이지 않도록 탐지 거리보다 크게 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|AutomaticTurret|Detection", meta=(ClampMin="1.0", ForceUnits="cm"))
	float LoseTargetRange = 5750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|AutomaticTurret|Detection", meta=(ClampMin="0.02", ForceUnits="s"))
	float TargetScanIntervalSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|AutomaticTurret|Detection")
	bool bRequireTargetLineOfSight = true;

	/** true면 현재 표적과 포탑을 잇는 Visibility Trace를 Editor/PIE에서 표시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|AutomaticTurret|Debug")
	bool bDrawDetectionDebugLine = false;

private:
	bool IsEligibleDroneTarget(const ADronePrototypePawn* Candidate, float Range) const;
	bool HasTargetLineOfSight(const AActor* Candidate) const;
	void ChangeAutomaticTarget(AActor* NewTarget);

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone AI Automatic Turret")
	TWeakObjectPtr<AActor> AutomaticTarget;

	float TargetScanTimeRemaining = 0.0f;
};

/** 지면 고정 받침대를 포함하는 설치형 자동포탑 Greybox 기본값이다. */
UCLASS(Blueprintable)
class DRONE_API ADroneEmplacedAutomaticTurret : public ADroneAutomaticTurret
{
	GENERATED_BODY()

public:
	ADroneEmplacedAutomaticTurret();
};

/** 차량 Roof/Socket에 Actor를 Attach해서 쓰는 낮은 마운트형 자동포탑 Greybox 기본값이다. */
UCLASS(Blueprintable)
class DRONE_API ADroneVehicleAutomaticTurret : public ADroneAutomaticTurret
{
	GENERATED_BODY()

public:
	ADroneVehicleAutomaticTurret();
};
