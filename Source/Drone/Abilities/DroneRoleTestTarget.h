#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneRoleTestTarget.generated.h"

class UDronePayloadTargetComponent;
class UDroneReconScanTargetComponent;
class UDroneHealthComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/** 역할 기능을 에디터 배치 즉시 시험할 수 있는 충돌/안내 공용 Greybox 표적이다. */
UCLASS(Abstract)
class DRONE_API ADroneRoleTestTarget : public AActor
{
	GENERATED_BODY()

public:
	ADroneRoleTestTarget();
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category="Drone|Role Test|Presentation")
	UStaticMeshComponent* GetTargetMesh() const { return TargetMesh; }

	UFUNCTION(BlueprintPure, Category="Drone|Role Test|Presentation")
	UTextRenderComponent* GetInstructionText() const { return InstructionText; }

	/** BP Class Defaults 또는 배치 Instance에서 바꾼 표시값을 Component에 다시 적용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Role Test|Presentation")
	void RefreshPresentation();

protected:
	void ConfigurePresentation(const FText& InText, const FColor& InColor, const FVector& InScale);

	/** 표적 외형은 BP에서 다른 Static Mesh로 교체할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Role Test|Presentation")
	TObjectPtr<UStaticMesh> TargetVisualMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Role Test|Presentation")
	FVector TargetVisualScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Role Test|Presentation", meta=(MultiLine="true"))
	FText InstructionMessage;

	/** 기본 Font에서 깨질 수 있는 World Text는 숨긴다. 필요할 때 BP/배치 Instance에서만 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Role Test|Presentation")
	bool bShowInstructionText = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Role Test|Presentation")
	FColor InstructionColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Role Test|Presentation", meta=(ClampMin="1.0"))
	float InstructionWorldSize = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Role Test|Presentation")
	FVector InstructionRelativeLocation = FVector(0.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Role Test|Presentation")
	FRotator InstructionRelativeRotation = FRotator::ZeroRotator;

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

	UFUNCTION(BlueprintPure, Category="Drone|Role Test|Recon")
	UDroneReconScanTargetComponent* GetReconTargetComponent() const { return ReconTargetComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Role Test|Recon", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDroneReconScanTargetComponent> ReconTargetComponent;
};

UCLASS()
class DRONE_API ADroneImpactRoleTestTarget : public ADroneRoleTestTarget
{
	GENERATED_BODY()

public:
	ADroneImpactRoleTestTarget();

	UFUNCTION(BlueprintPure, Category="Drone|Role Test|Impact")
	UDroneHealthComponent* GetHealthComponent() const { return HealthComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Role Test|Impact", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDroneHealthComponent> HealthComponent;
};

UCLASS()
class DRONE_API ADronePayloadRoleTestTarget : public ADroneRoleTestTarget
{
	GENERATED_BODY()

public:
	ADronePayloadRoleTestTarget();

	UFUNCTION(BlueprintPure, Category="Drone|Role Test|Payload")
	UDronePayloadTargetComponent* GetPayloadTargetComponent() const { return PayloadTargetComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Role Test|Payload", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDronePayloadTargetComponent> PayloadTargetComponent;
};
