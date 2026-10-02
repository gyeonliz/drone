#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "DroneBatteryComponent.generated.h"

/** 배터리를 다 썼을 때의 처리. Figma에 정해지지 않아(현재 미정) 기본은 경고만 한다. */
UENUM(BlueprintType)
enum class EDroneBatteryDepletedResponse : uint8
{
	WarnOnly UMETA(DisplayName="경고만"),
	/** Mission Director에 실패를 보고한다. 미션이 재출격 설정이면 재출격한다. */
	FailMission UMETA(DisplayName="미션 실패 처리")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDroneBatteryChangedSignature, float, RemainingFraction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDroneBatteryEventSignature);

/**
 * HUD-FIGMA-01: 기체별 배터리 시간(Figma: "배터리 시스템 드론별 시간 지정 가능").
 *
 * 플레이어가 조종하는 동안에만 줄어든다(기체 선택 미리보기·AI 조종 중에는 줄지 않음).
 * 용량 0이면 배터리 시스템을 쓰지 않는 기체로 보고 HUD에도 표시하지 않는다.
 * 시간은 FDroneFlightProfile.BatteryLifeSeconds에서 받는다.
 */
UCLASS(ClassGroup=(Drone), meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneBatteryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneBatteryComponent();

	/** 용량(초)을 정하고 가득 채운다. 0 이하면 배터리를 끈다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	void ConfigureCapacity(float InCapacitySeconds);

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsBatteryEnabled() const { return CapacitySeconds > 0.0f; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	float GetRemainingSeconds() const { return RemainingSeconds; }

	/** 0~1. 배터리를 끈 기체는 1. */
	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	float GetRemainingFraction() const { return IsBatteryEnabled() ? RemainingSeconds / CapacitySeconds : 1.0f; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsLow() const { return IsBatteryEnabled() && GetRemainingFraction() <= LowBatteryFraction; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsDepleted() const { return bDepleted; }

	/** 시간을 바로 줄인다(테스트·충격 소모 등). 소진 판정도 함께 한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	void ConsumeSeconds(float Seconds);

	/** 이 비율 이하가 되면 HUD 경고와 OnBatteryLow(1회). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery", meta=(ClampMin="0", ClampMax="1"))
	float LowBatteryFraction = 0.2f;

	/** 1초에 줄어드는 배터리 초. 1이면 실제 시간 그대로. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery", meta=(ClampMin="0"))
	float DrainRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery")
	EDroneBatteryDepletedResponse DepletedResponse = EDroneBatteryDepletedResponse::WarnOnly;

	/** 남은 비율이 바뀔 때(약 0.25초마다). */
	UPROPERTY(BlueprintAssignable, Category="Drone|Battery")
	FDroneBatteryChangedSignature OnBatteryChanged;

	UPROPERTY(BlueprintAssignable, Category="Drone|Battery")
	FDroneBatteryEventSignature OnBatteryLow;

	UPROPERTY(BlueprintAssignable, Category="Drone|Battery")
	FDroneBatteryEventSignature OnBatteryDepleted;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool IsDrainingNow() const;

	UPROPERTY(VisibleInstanceOnly, Category="Drone|Battery")
	float CapacitySeconds = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category="Drone|Battery")
	float RemainingSeconds = 0.0f;

	bool bLowBroadcast = false;
	bool bDepleted = false;
	float SecondsSinceBroadcast = 0.0f;
};
