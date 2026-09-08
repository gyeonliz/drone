#include "Flow/DroneFrontEndGameMode.h"

#include "Flow/DroneFrontEndPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ADroneFrontEndGameMode::ADroneFrontEndGameMode()
{
	// Engine 기본 Spawn 경고 없이 UI Camera만 유지하는 비-Drone Spectator다.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	PlayerControllerClass = ADroneFrontEndPlayerController::StaticClass();
	bStartPlayersAsSpectators = true;
	SpectatorClass = ASpectatorPawn::StaticClass();
}
