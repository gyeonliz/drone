#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneReconScanComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneReconScanProgressSignature,
	AActor*, TargetActor,
	float, NormalizedProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDroneReconScanTargetSignature, AActor*, TargetActor);

/**
 * 정찰 드론의 거리·화각·시야 유지형 Scan Greybox다.
 * 입력 키와 UI는 소유하지 않고 Blueprint가 Start/Cancel과 Event를 연결한다.
 */
UCLASS(ClassGroup=(Drone), BlueprintType, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneReconScanComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneReconScanComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ConfigureFeatureEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Drone|Recon")
	bool StartScan(AActor* TargetActor);

	/** 입력 시점에만 World를 한 번 검색해 가장 가까운 유효 Scan Target을 시작한다. Tick 검색은 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Recon")
	bool StartBestAvailableScan();

	/** 현재 거리·화각·LOS 조건을 통과하는 가장 가까운 미완료 Target을 찾는다. */
	UFUNCTION(BlueprintPure, Category="Drone|Recon")
	AActor* FindBestAvailableScanTarget() const;

	UFUNCTION(BlueprintCallable, Category="Drone|Recon")
	void CancelScan();

	UFUNCTION(BlueprintPure, Category="Drone|Recon")
	bool IsFeatureEnabled() const { return bFeatureEnabled; }

	UFUNCTION(BlueprintPure, Category="Drone|Recon")
	bool IsScanning() const { return ActiveTarget.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Drone|Recon")
	float GetScanProgressNormalized() const;

	UFUNCTION(BlueprintPure, Category="Drone|Recon")
	AActor* GetActiveScanTarget() const { return ActiveTarget.Get(); }

	/** 자동화와 초기 밸런싱용. 최종 수치는 파생 BP Class Defaults에서도 조정할 수 있다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Recon|Greybox")
	void ConfigureScanGreybox(float NewRange, float NewDuration, float NewHalfAngleDegrees, bool bNewRequireLineOfSight);

	UFUNCTION(BlueprintPure, Category="Drone|Recon|Debug")
	int32 GetCompletedScanCount() const { return CompletedScanCount; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Recon")
	FDroneReconScanProgressSignature OnScanProgress;

	UPROPERTY(BlueprintAssignable, Category="Drone|Recon")
	FDroneReconScanTargetSignature OnScanCompleted;

	UPROPERTY(BlueprintAssignable, Category="Drone|Recon")
	FDroneReconScanTargetSignature OnScanCanceled;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Recon", meta=(ClampMin="1.0", ForceUnits="cm"))
	float ScanRangeCentimeters = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Recon", meta=(ClampMin="0.05", ForceUnits="s"))
	float ScanDurationSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Recon", meta=(ClampMin="1.0", ClampMax="180.0", ForceUnits="deg"))
	float ScanHalfAngleDegrees = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Recon")
	bool bRequireLineOfSight = true;

private:
	bool IsTargetValidForScan(AActor* TargetActor) const;
	void CompleteScan();

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Recon")
	bool bFeatureEnabled = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ActiveTarget;

	UPROPERTY(Transient, VisibleAnywhere, Category="Drone|Recon")
	float ScanElapsedSeconds = 0.0f;

	UPROPERTY(Transient)
	int32 CompletedScanCount = 0;
};
