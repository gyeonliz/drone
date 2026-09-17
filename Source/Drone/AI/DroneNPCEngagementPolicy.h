#pragma once

#include "CoreMinimal.h"

/** 개인화기 NPC가 감지 표적을 사격·추적·포기 중 어디로 보낼지 결정한다. */
enum class EDroneNPCEngagementDecision : uint8
{
	Fire,
	Pursue,
	Disengage
};

/**
 * 월드나 Controller 상태를 변경하지 않는 개인화기 교전 정책이다.
 * StateTree 런타임과 빠른 자동화 테스트가 같은 경계 규칙을 공유한다.
 */
struct DRONE_API FDroneNPCEngagementPolicy
{
	static EDroneNPCEngagementDecision Evaluate(
		bool bHasCombatOrigin,
		const FVector& CombatOrigin,
		const FVector& PawnLocation,
		const FVector& TargetLocation,
		float WeaponRange,
		float CombatLeashRadius);

	/** 같은 지상 목표로 이동 중일 때 경로 요청을 다시 만들어야 하는지 판단한다. */
	static bool ShouldRequestPursuitMove(
		bool bHasPreviousDestination,
		const FVector& PreviousDestination,
		const FVector& NewDestination,
		float RepathDistance,
		bool bMoveInProgress);
};
