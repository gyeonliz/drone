#include "Health/DroneBatteryComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace DroneBattery
{
// HUD 갱신 주기. 매 프레임 Delegate를 쏘지 않는다.
constexpr float BroadcastIntervalSeconds = 0.25f;
}

UDroneBatteryComponent::UDroneBatteryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDroneBatteryComponent::ConfigureCapacity(const float InCapacitySeconds)
{
	CapacitySeconds = FMath::Max(0.0f, InCapacitySeconds);
	RemainingSeconds = CapacitySeconds;
	bLowBroadcast = false;
	bDepleted = false;
	SecondsSinceBroadcast = 0.0f;
	OnBatteryChanged.Broadcast(GetRemainingFraction());
}

bool UDroneBatteryComponent::IsDrainingNow() const
{
	// 플레이어가 실제로 조종 중일 때만 쓴다. 선택 화면 미리보기나 AI·빙의 전에는 줄지 않는다.
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return IsBatteryEnabled() && !bDepleted && OwnerPawn && Cast<APlayerController>(OwnerPawn->GetController()) != nullptr;
}

void UDroneBatteryComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsDrainingNow())
	{
		return;
	}
	SecondsSinceBroadcast += DeltaTime;
	ConsumeSeconds(DeltaTime * DrainRate);
}

void UDroneBatteryComponent::ConsumeSeconds(const float Seconds)
{
	if (!IsBatteryEnabled() || bDepleted || Seconds <= 0.0f)
	{
		return;
	}
	RemainingSeconds = FMath::Max(0.0f, RemainingSeconds - Seconds);
	const bool bShouldBroadcast = SecondsSinceBroadcast >= DroneBattery::BroadcastIntervalSeconds || RemainingSeconds <= 0.0f
		|| (!bLowBroadcast && IsLow());
	if (bShouldBroadcast)
	{
		SecondsSinceBroadcast = 0.0f;
		OnBatteryChanged.Broadcast(GetRemainingFraction());
	}
	if (!bLowBroadcast && IsLow())
	{
		bLowBroadcast = true;
		OnBatteryLow.Broadcast();
	}
	if (RemainingSeconds <= 0.0f)
	{
		bDepleted = true;
		OnBatteryDepleted.Broadcast();
	}
}
