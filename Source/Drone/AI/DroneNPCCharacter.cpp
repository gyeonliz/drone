#include "AI/DroneNPCCharacter.h"

#include "AI/DroneNPCAIController.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "SmartObjectUserComponent.h"

ADroneNPCCharacter::ADroneNPCCharacter()
{
	// CharacterMovement와 Animation Component가 각자 갱신되므로 Actor Tick은 별도로 쓰지 않는다.
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = ADroneNPCAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	NPCProfileComponent = CreateDefaultSubobject<UDroneNPCProfileComponent>(TEXT("NPCProfileComponent"));
	SmartObjectUserComponent = CreateDefaultSubobject<USmartObjectUserComponent>(TEXT("SmartObjectUserComponent"));
	NPCWeaponComponent = CreateDefaultSubobject<UDroneNPCWeaponComponent>(TEXT("NPCWeaponComponent"));
}
