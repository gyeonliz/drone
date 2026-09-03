#include "Flow/DroneFrontEndGameMode.h"

#include "Flow/DroneFrontEndPlayerController.h"

ADroneFrontEndGameMode::ADroneFrontEndGameMode()
{
	// Drone 선택 전에는 Pawn을 만들지 않는다. FLOW-05가 Spawn/Possess 책임을 이어받는다.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ADroneFrontEndPlayerController::StaticClass();
	HUDClass = nullptr;
	bStartPlayersAsSpectators = true;
}
