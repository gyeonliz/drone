#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneJammingVolume.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDroneJammerDisabledSignature, AActor*, JammerActor);
DECLARE_MULTICAST_DELEGATE_OneParam(FDroneJammerDisabledNativeSignature, AActor*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneJammingExitedSignature,
	AActor*, DroneActor,
	AActor*, JammerActor);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDroneJammingExitedNativeSignature, AActor*, AActor*);

/** Box 안의 Drone 신호를 지정 강도로 방해한다. 위치·크기·강도는 BP/맵 Instance 값이며 실제 전파 물리가 아니다. */
UCLASS(Blueprintable)
class DRONE_API ADroneJammingVolume : public AActor
{
	GENERATED_BODY()

public:
	ADroneJammingVolume();

	UFUNCTION(BlueprintPure, Category="Drone|Jamming")
	UBoxComponent* GetJammingBounds() const { return JammingBounds; }

	UFUNCTION(BlueprintPure, Category="Drone|Jamming")
	bool IsJammerActive() const { return bJammerActive; }

	UFUNCTION(BlueprintPure, Category="Drone|Jamming")
	float GetNormalizedJammingStrength() const { return bJammerActive ? NormalizedJammingStrength : 0.0f; }

	/** BP가 전원/피격/연출로 강도를 바꾸면 현재 Box 안의 Drone도 즉시 갱신한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Jamming")
	void SetNormalizedJammingStrength(float NewStrength);

	/** 한 번만 무력화하고 신호를 복원한다. Mission 목표 Event는 이 명시적 경계만 구독한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Jamming")
	bool DisableJammer();

	UPROPERTY(BlueprintAssignable, Category="Drone|Jamming")
	FDroneJammerDisabledSignature OnJammerDisabled;
	/** C++ Mission Director가 직접 구독하는 게임 규칙 Event. BP Event는 연출에 사용한다. */
	FDroneJammerDisabledNativeSignature OnJammerDisabledNative;

	UPROPERTY(BlueprintAssignable, Category="Drone|Jamming")
	FDroneJammingExitedSignature OnDroneExitedJamming;
	FDroneJammingExitedNativeSignature OnDroneExitedJammingNative;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Jamming")
	TObjectPtr<UBoxComponent> JammingBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Jamming", meta=(ClampMin="0.0", ClampMax="1.0"))
	float NormalizedJammingStrength = 0.60f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Jamming")
	bool bJammerActive = true;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void RefreshOverlappingDroneSignals(bool bRemoveSource);
};
