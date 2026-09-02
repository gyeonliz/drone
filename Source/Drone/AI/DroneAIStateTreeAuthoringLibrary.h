#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DroneAIStateTreeAuthoringLibrary.generated.h"

/** 프로젝트 소유 AI StateTree를 재현 가능하게 만드는 Editor 작성 도구다. */
UCLASS()
class DRONE_API UDroneAIStateTreeAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * AI-PATROL-01의 Hostile Patrol StateTree를 새로 생성한다.
	 * 기존 Asset은 사용자의 Editor 수정 내용을 보호하기 위해 덮어쓰지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Authoring")
	static bool CreateHostilePatrolStateTree(const FString& AssetPath);

	/** 저장된 StateTree의 Schema·상태 순서·Task 종류·컴파일 상태를 확인한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Authoring")
	static bool ValidateHostilePatrolStateTree(const FString& AssetPath);

	/** 기존 AI-PATROL-01 Asset에 DroneDetected·Search Event 전환을 안전하게 추가한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Authoring")
	static bool UpgradeHostilePatrolStateTreeForPerception(const FString& AssetPath);

	/** AI-PER-01의 감지·실종·Search·Patrol 복귀 계약까지 확인한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Authoring")
	static bool ValidateHostilePerceptionStateTree(const FString& AssetPath);

	/** AI-FRIEND-01의 아군 기지 활동 StateTree를 새로 생성한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Authoring")
	static bool CreateFriendlyBaseRoutineStateTree(const FString& AssetPath);

	/** 저장된 아군 StateTree의 상태 순서·Task 종류·컴파일 상태를 확인한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Authoring")
	static bool ValidateFriendlyBaseRoutineStateTree(const FString& AssetPath);
};
