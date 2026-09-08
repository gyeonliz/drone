#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneRoleTestTarget.generated.h"

class UDronePayloadTargetComponent;
class UDroneReconScanTargetComponent;
class UDroneHealthComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** 역할 기능을 에디터 배치 즉시 시험할 수 있는 충돌/안내 공용 Greybox 표적이다. */
UCLASS(Abstract)
class DRONE_API ADroneRoleTestTarget : public AActor
{
	GENERATED_BODY()

public:
	ADroneRoleTestTarget();

	UStaticMeshComponent* GetTargetMesh() const { return TargetMesh; }
	UTextRenderComponent* GetInstructionText() const { return InstructionText; }

protected:
	void ConfigurePresentation(const FText& InText, const FColor& InColor, const FVector& InScale);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Role Test")
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Role Test")
	TObjectPtr<UTextRenderComponent> InstructionText;
};

UCLASS()
class DRONE_API ADroneReconRoleTestTarget : public ADroneRoleTestTarget
{
	GENERATED_BODY()

public:
	ADroneReconRoleTestTarget();
	UDroneReconScanTargetComponent* GetReconTargetComponent() const { return ReconTargetComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category="Drone|Role Test")
	TObjectPtr<UDroneReconScanTargetComponent> ReconTargetComponent;
};

UCLASS()
class DRONE_API ADroneImpactRoleTestTarget : public ADroneRoleTestTarget
{
	GENERATED_BODY()

public:
	ADroneImpactRoleTestTarget();
	UDroneHealthComponent* GetHealthComponent() const { return HealthComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category="Drone|Role Test")
	TObjectPtr<UDroneHealthComponent> HealthComponent;
};

UCLASS()
class DRONE_API ADronePayloadRoleTestTarget : public ADroneRoleTestTarget
{
	GENERATED_BODY()

public:
	ADronePayloadRoleTestTarget();
	UDronePayloadTargetComponent* GetPayloadTargetComponent() const { return PayloadTargetComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category="Drone|Role Test")
	TObjectPtr<UDronePayloadTargetComponent> PayloadTargetComponent;
};
