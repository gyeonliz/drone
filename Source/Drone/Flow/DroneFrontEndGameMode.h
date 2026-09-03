#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DroneFrontEndGameMode.generated.h"

/** Drone을 Spawn하지 않고 시작 화면과 로비만 실행하는 Front-end 전용 GameMode다. */
UCLASS(Blueprintable)
class DRONE_API ADroneFrontEndGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADroneFrontEndGameMode();
};
