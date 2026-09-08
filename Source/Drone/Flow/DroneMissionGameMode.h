#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DroneMissionGameMode.generated.h"

/**
 * 로비에서 Mission Map으로 이동할 때 URL로만 적용하는 FLOW-05 GameMode다.
 * 선택 전에는 Drone을 자동 생성하지 않고 Mission PlayerController가 확정된 기체 한 대만 만든다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneMissionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADroneMissionGameMode();
};
