#include "AI/DroneNPCCharacter.h"

#include "AI/DroneNPCAIController.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Health/DroneHealthComponent.h"
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
	HealthComponent = CreateDefaultSubobject<UDroneHealthComponent>(TEXT("HealthComponent"));
}

void ADroneNPCCharacter::BeginPlay()
{
	Super::BeginPlay();
	HealthComponent->OnDeath.AddDynamic(this, &ADroneNPCCharacter::HandleDeath);
}

void ADroneNPCCharacter::HandleDeath(
	AActor* DeadActor,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	if (DeadActor != this)
	{
		return;
	}

	// 회색상자 단계에서는 시체를 제거하거나 래그돌로 바꾸지 않는다. 이동·충돌·AI만
	// 확실히 정지해 이후 Animation/Respawn 규칙을 Blueprint에서 안전하게 추가한다.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	SetActorEnableCollision(false);

	if (ADroneNPCAIController* DroneController = Cast<ADroneNPCAIController>(GetController()))
	{
		DroneController->HandlePossessedPawnDeath();
	}
}
