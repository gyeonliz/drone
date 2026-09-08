#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneReconScanTargetComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneReconTargetScannedSignature,
	AActor*, TargetActor,
	AActor*, ScannerActor);

/** 정찰 Scan 대상으로 사용할 Actor에 붙이는 최소 표식 Component다. */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneReconScanTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneReconScanTargetComponent();

	/** 한 번만 완료 처리한다. 이미 완료됐거나 Scanner가 없으면 false다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Recon|Target")
	bool MarkScanned(AActor* ScannerActor);

	/** Mission 재시작이나 작성 시험에서만 완료 상태를 초기화한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Recon|Target")
	void ResetScanState();

	UFUNCTION(BlueprintPure, Category="Drone|Recon|Target")
	bool IsScanCompleted() const { return bScanCompleted; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Recon|Target")
	FDroneReconTargetScannedSignature OnTargetScanned;

private:
	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Recon|Target")
	bool bScanCompleted = false;
};
