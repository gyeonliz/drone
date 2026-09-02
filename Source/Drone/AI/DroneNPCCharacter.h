#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DroneNPCCharacter.generated.h"

class UDroneNPCProfileComponent;
class UDroneNPCWeaponComponent;
class USmartObjectUserComponent;

/**
 * 적 경계병과 기지 아군 NPC가 공유하는 프로젝트 소유 Character 기반 클래스다.
 *
 * 이동·AIController·Smart Object User·역할 데이터만 제공한다. 이식한 Soldier/Insurgent
 * Mesh와 Animation Blueprint는 파생 Blueprint에서 지정한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneNPCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADroneNPCCharacter();

	UFUNCTION(BlueprintPure, Category="Drone|AI|NPC")
	UDroneNPCProfileComponent* GetNPCProfileComponent() const { return NPCProfileComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|NPC")
	USmartObjectUserComponent* GetSmartObjectUserComponent() const { return SmartObjectUserComponent; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|NPC")
	UDroneNPCWeaponComponent* GetNPCWeaponComponent() const { return NPCWeaponComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|NPC|Components")
	TObjectPtr<UDroneNPCProfileComponent> NPCProfileComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|NPC|Components")
	TObjectPtr<USmartObjectUserComponent> SmartObjectUserComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|NPC|Components")
	TObjectPtr<UDroneNPCWeaponComponent> NPCWeaponComponent;
};
