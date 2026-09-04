#pragma once

#include "CoreMinimal.h"
#include "AI/DroneSmartObjectStation.h"
#include "DroneMGTurretStation.generated.h"

class UStaticMeshComponent;
class UArrowComponent;

/**
 * MG 전용 3분할 Station이다.
 *
 * 일반 Patrol/Cover/Ambient Smart Object에는 Turret Component를 만들지 않는다.
 * Base Mesh는 고정, Body Mesh는 Yaw, Barrel Mesh는 Pitch만 상속한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneMGTurretStation : public ADroneSmartObjectStation
{
	GENERATED_BODY()

public:
	ADroneMGTurretStation();

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Components")
	UStaticMeshComponent* GetMGTurretBaseMesh() const { return MGTurretBaseMesh; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Components")
	UStaticMeshComponent* GetMGTurretBodyMesh() const { return MGTurretBodyMesh; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Components")
	UStaticMeshComponent* GetMGTurretBarrelMesh() const { return MGTurretBarrelMesh; }

	/** 병사가 점유 중 서 있을 포탑 뒤 조작 위치다. Yaw 몸체를 그대로 따라간다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Operator")
	UArrowComponent* GetMGTurretOperatorAnchor() const { return MGTurretOperatorAnchor; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Operator")
	FTransform GetMGTurretOperatorTransform() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|MG|Operator")
	float GetMGTurretOperatorDistance() const { return MGTurretOperatorDistance; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 지면에 고정되는 하단부 임시 원기둥이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Components")
	TObjectPtr<UStaticMeshComponent> MGTurretBaseMesh;

	/** MGTurretYawPivot만 따라 좌우로 도는 몸체 임시 원기둥이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Components")
	TObjectPtr<UStaticMeshComponent> MGTurretBodyMesh;

	/** MGTurretAimPivot만 따라 상하로 도는 포신 임시 원기둥이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Components")
	TObjectPtr<UStaticMeshComponent> MGTurretBarrelMesh;

	/**
	 * Yaw 몸체에 붙은 병사 조작 위치다. 화살표는 병사가 바라볼 몸체 방향을 표시한다.
	 * 위치는 아래 Operator 값으로 조정하고, 방향은 Yaw 몸체 회전을 그대로 상속한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|MG|Operator")
	TObjectPtr<UArrowComponent> MGTurretOperatorAnchor;

	/** 포탑 중심에서 뒤쪽(-X)으로 떨어질 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|MG|Operator", meta=(ClampMin="10.0", ForceUnits="cm"))
	float MGTurretOperatorDistance = 120.0f;

	/** 포탑 기준 좌우 위치 보정이다. +는 오른쪽(+Y)이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|MG|Operator", meta=(ForceUnits="cm"))
	float MGTurretOperatorLateralOffset = 0.0f;

	/** 조작점의 발 높이 보정이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|AI|MG|Operator", meta=(ForceUnits="cm"))
	float MGTurretOperatorVerticalOffset = 0.0f;

private:
	void RefreshMGTurretOperatorAnchor();
};
