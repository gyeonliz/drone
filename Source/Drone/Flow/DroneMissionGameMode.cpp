#include "Flow/DroneMissionGameMode.h"

#include "Flow/DroneMissionPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ADroneMissionGameMode::ADroneMissionGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	PlayerControllerClass = ADroneMissionPlayerController::StaticClass();
	bStartPlayersAsSpectators = true;
	// 선택 전에는 비-Drone Spectator로 Camera만 유지하고 확정 시 Controller가 이를 제거한다.
	SpectatorClass = ASpectatorPawn::StaticClass();
}
