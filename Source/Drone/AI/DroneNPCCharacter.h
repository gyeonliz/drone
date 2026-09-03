#pragma once

#include "CoreMinimal.h"
#include "AI/DroneAITypes.h"
#include "GameFramework/Character.h"
#include "DroneNPCCharacter.generated.h"

class UDroneNPCProfileComponent;
class UDroneNPCWeaponComponent;
class UDroneHealthComponent;
class UAnimSequenceBase;
class USceneComponent;
class USmartObjectUserComponent;
class UStaticMeshComponent;

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

	UFUNCTION(BlueprintPure, Category="Drone|Health")
	UDroneHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/**
	 * 역할 Blueprint가 Rifle/Shotgun 외형만 교체하는 프로젝트 소유 Component다.
	 * Trace·Damage·탄약은 Weapon Component에 남기고 이 Component에는 표현만 넣는다.
	 */
	UFUNCTION(BlueprintPure, Category="Drone|AI|NPC|Visual")
	UStaticMeshComponent* GetWeaponVisualComponent() const { return WeaponVisualComponent; }

	/** Muzzle Flash·발사음 기준을 Blueprint에서 눈으로 조정하는 자식 Scene Component다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|NPC|Visual")
	USceneComponent* GetWeaponMuzzleComponent() const { return WeaponMuzzleComponent; }

	/** Blueprint에서 WeaponAttachPointName을 바꾼 뒤 부착을 즉시 다시 적용할 때 사용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|NPC|Visual")
	void RefreshWeaponVisualAttachment();

	/** 현재 역할이 사용할 임시 발사 Animation이다. 역할 BP에서 최종 자산으로 교체할 수 있다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|NPC|Visual|Animation")
	UAnimSequenceBase* GetGreyboxFireAnimation(EDroneNPCWeaponType WeaponType) const;

	/** 현재 역할이 사용할 임시 재장전 Animation이다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|NPC|Visual|Animation")
	UAnimSequenceBase* GetGreyboxReloadAnimation(EDroneNPCWeaponType WeaponType) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/**
	 * Rifle 한 발 또는 Shotgun 한 Volley마다 한 번 호출되는 표현 전용 Event다.
	 * Blueprint는 여기서 Montage·Niagara·Sound만 실행하고 Trace/Damage를 만들지 않는다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Drone|AI|NPC|Visual", meta=(DisplayName="NPC Weapon Fired Visual"))
	void ReceiveWeaponFiredVisual(
		EDroneNPCWeaponType WeaponType,
		FTransform MuzzleTransform,
		FVector AimPoint);

	/** 실제 Reload가 성공한 경우에만 호출되는 표현 전용 Event다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Drone|AI|NPC|Visual", meta=(DisplayName="NPC Reload Completed Visual"))
	void ReceiveReloadCompletedVisual(
		EDroneNPCWeaponType WeaponType,
		int32 CurrentAmmo,
		int32 MagazineCapacity);

	UFUNCTION()
	void HandleWeaponFiredVisual(
		EDroneNPCWeaponType WeaponType,
		FVector TraceStart,
		FVector AimPoint);

	UFUNCTION()
	void HandleReloadCompletedVisual(
		EDroneNPCWeaponType WeaponType,
		int32 CurrentAmmo,
		int32 MagazineCapacity);

	UFUNCTION()
	void HandleDeath(AActor* DeadActor, AController* InstigatorController, AActor* DamageCauser);

	/** ABP의 Slot을 통해 임시 Sequence를 Dynamic Montage로 재생한다. Gameplay 판정은 건드리지 않는다. */
	void PlayGreyboxWeaponAnimation(UAnimSequenceBase* Animation, float PlayRate);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone AI NPC Components")
	TObjectPtr<UDroneNPCProfileComponent> NPCProfileComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone AI NPC Components")
	TObjectPtr<USmartObjectUserComponent> SmartObjectUserComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone AI NPC Components")
	TObjectPtr<UDroneNPCWeaponComponent> NPCWeaponComponent;

	/** NPC 공통 기본 체력 100과 사망 한 번 처리 규칙을 제공한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone AI NPC Components")
	TObjectPtr<UDroneHealthComponent> HealthComponent;

	/**
	 * 최종 무기 Actor 구조가 확정되기 전 사용하는 교체 가능한 시각 전용 Mesh다.
	 * Collision과 Navigation 영향은 항상 끈다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone AI NPC Visual")
	TObjectPtr<UStaticMeshComponent> WeaponVisualComponent;

	/** 총구 표현의 위치·회전을 무기 Mesh와 분리해 Blueprint에서 조정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone AI NPC Visual")
	TObjectPtr<USceneComponent> WeaponMuzzleComponent;

	/** Manny 기본 뼈 이름이다. 다른 Skeleton을 채택할 때 역할 Blueprint에서 바꾼다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual")
	FName WeaponAttachPointName = TEXT("hand_r");

	/**
	 * true면 기본 Manny 발사·재장전 Sequence를 AnimBP의 Slot에서 임시 재생한다.
	 * 최종 AnimBP가 자체 Montage를 재생할 때는 역할 Blueprint에서 끈다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation")
	bool bUseGreyboxWeaponAnimations = true;

	/** 현재 Manny용 임시 소총 발사 Sequence다. Shotgun도 별도 자산 확보 전 같은 동작을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation")
	TObjectPtr<UAnimSequenceBase> RifleFireAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation")
	TObjectPtr<UAnimSequenceBase> ShotgunFireAnimation;

	/** 현재 즉시 Reload Gameplay와 분리된 Manny용 임시 표현 Sequence다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation")
	TObjectPtr<UAnimSequenceBase> RifleReloadAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation")
	TObjectPtr<UAnimSequenceBase> ShotgunReloadAnimation;

	/** ABP_NPC_Rifle_Greybox가 사용하는 Montage 출력 Slot이다. 다른 AnimBP도 같은 Slot 이름이 필요하다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation")
	FName GreyboxWeaponAnimationSlotName = TEXT("DefaultSlot");

	/** 임시 발사 가산 동작 속도다. 재생 중 같은 Sequence는 다시 시작하지 않아 연사 떨림을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation", meta=(ClampMin="0.01"))
	float GreyboxFireAnimationPlayRate = 1.0f;

	/** 재장전은 현재 즉시 판정의 시각 표현이므로 원본 속도로 한 번 재생한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation", meta=(ClampMin="0.01"))
	float GreyboxReloadAnimationPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation", meta=(ClampMin="0.0", ForceUnits="s"))
	float GreyboxAnimationBlendInSeconds = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone AI NPC Visual Animation", meta=(ClampMin="0.0", ForceUnits="s"))
	float GreyboxAnimationBlendOutSeconds = 0.1f;
};
