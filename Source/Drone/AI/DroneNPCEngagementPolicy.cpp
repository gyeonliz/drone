#include "AI/DroneNPCEngagementPolicy.h"

EDroneNPCEngagementDecision FDroneNPCEngagementPolicy::Evaluate(
	const bool bHasCombatOrigin,
	const FVector& CombatOrigin,
	const FVector& PawnLocation,
	const FVector& TargetLocation,
	const float WeaponRange,
	const float CombatLeashRadius)
{
	const float SafeLeashRadius = FMath::Max(0.0f, CombatLeashRadius);
	if (bHasCombatOrigin && SafeLeashRadius > 0.0f)
	{
		const float LeashRadiusSquared = FMath::Square(SafeLeashRadius);
		if (FVector::DistSquared2D(CombatOrigin, PawnLocation) > LeashRadiusSquared
			|| FVector::DistSquared2D(CombatOrigin, TargetLocation) > LeashRadiusSquared)
		{
			return EDroneNPCEngagementDecision::Disengage;
		}
	}

	const float SafeWeaponRange = FMath::Max(0.0f, WeaponRange);
	if (SafeWeaponRange <= 0.0f)
	{
		return EDroneNPCEngagementDecision::Pursue;
	}

	// 실제 무기 Component와 같은 3D 거리 경계를 쓴다. 경계 떨림 안정화는 Controller가
	// 시간으로 처리하며, 사거리 안쪽인데 더 쫓아가는 공간 Hysteresis는 두지 않는다.
	return FVector::DistSquared(PawnLocation, TargetLocation) <= FMath::Square(SafeWeaponRange)
		? EDroneNPCEngagementDecision::Fire
		: EDroneNPCEngagementDecision::Pursue;
}

bool FDroneNPCEngagementPolicy::ShouldRequestPursuitMove(
	const bool bHasPreviousDestination,
	const FVector& PreviousDestination,
	const FVector& NewDestination,
	const float RepathDistance,
	const bool bMoveInProgress)
{
	if (!bHasPreviousDestination || !bMoveInProgress)
	{
		return true;
	}

	const float SafeRepathDistance = FMath::Max(0.0f, RepathDistance);
	return FVector::DistSquared2D(PreviousDestination, NewDestination)
		> FMath::Square(SafeRepathDistance);
}
