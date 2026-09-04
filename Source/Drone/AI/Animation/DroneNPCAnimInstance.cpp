#include "AI/Animation/DroneNPCAnimInstance.h"

#include "AI/DroneNPCAIController.h"
#include "GameFramework/Pawn.h"

namespace
{
	constexpr float SpineLookWeight = 0.20f;
	constexpr float NeckLookWeight = 0.45f;
	constexpr float HeadLookWeight = 0.35f;
}

void UDroneNPCAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	const APawn* Pawn = TryGetPawnOwner();
	CachedDroneController = Pawn ? Cast<ADroneNPCAIController>(Pawn->GetController()) : nullptr;
}

void UDroneNPCAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const APawn* Pawn = TryGetPawnOwner();
	ADroneNPCAIController* Controller = CachedDroneController.Get();
	if (!Controller || !Pawn || Pawn->GetController() != Controller)
	{
		Controller = Pawn ? Cast<ADroneNPCAIController>(Pawn->GetController()) : nullptr;
		CachedDroneController = Controller;
	}

	if (!Controller)
	{
		DroneLookRotation = FRotator::ZeroRotator;
		DroneLookSpineRotation = FRotator::ZeroRotator;
		DroneLookNeckRotation = FRotator::ZeroRotator;
		DroneLookHeadRotation = FRotator::ZeroRotator;
		DroneLookAlpha = 0.0f;
		bHasDroneLookTarget = false;
		return;
	}

	DroneLookRotation = Controller->GetSmoothedDroneLookRotation();
	DroneLookSpineRotation = DroneLookRotation * SpineLookWeight;
	DroneLookNeckRotation = DroneLookRotation * NeckLookWeight;
	DroneLookHeadRotation = DroneLookRotation * HeadLookWeight;
	DroneLookAlpha = Controller->GetDroneLookAlpha();
	bHasDroneLookTarget = Controller->HasActiveDroneLookTarget();
}
