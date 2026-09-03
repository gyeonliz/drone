#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectRuntime.h"
#include "DroneSmartObjectReservationComponent.generated.h"

class USmartObjectBehaviorDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneSmartObjectReservationChangedSignature,
	bool, bHasReservation,
	FTransform, SlotTransform);

/**
 * AI가 Smart Object Slot을 검색·예약·해제하는 수명주기를 소유한다.
 *
 * Claim Handle을 Blueprint나 StateTree 여러 곳에 흩어 놓지 않고 이 Component 하나가
 * 보관한다. Owner가 파괴되거나 다른 행동으로 전환되면 ReleaseReservation()을 호출하며,
 * EndPlay에서도 마지막 안전 해제를 시도한다.
 */
UCLASS(ClassGroup=(DroneAI), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class DRONE_API UDroneSmartObjectReservationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneSmartObjectReservationComponent();

	/**
	 * SearchOrigin 주변의 가장 가까운 일치 Slot을 예약한다.
	 * RequiredActivityTags가 비어 있으면 Friendly NPC의 적 Turret 오점유를 막기 위해 실패한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	bool ClaimNearestAvailableSlot(const FVector& SearchOrigin, FTransform& OutSlotTransform);

	/**
	 * 직전에 완료한 Slot 주변을 피해서 다음 후보를 예약한다.
	 * 모든 다른 후보가 사용 중이면 일반 가장 가까운 검색으로 한 번 더 시도한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	bool ClaimNearestAvailableSlotAvoiding(
		const FVector& SearchOrigin,
		const FVector& AvoidLocation,
		float AvoidRadius,
		FTransform& OutSlotTransform);

	/** Gameplay Interaction Task를 직접 쓰지 않을 때 Claim을 Occupied로 전환한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	bool MarkReservationOccupied();

	/** 현재 Claim을 Free로 되돌린다. 성공 여부와 무관하게 로컬 Handle은 정리한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	bool ReleaseReservation();

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	bool HasValidReservation() const;

	/** 현재 Handle이 Gameplay Interaction을 실행 중인 Occupied Slot인지 확인한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	bool IsReservationOccupied() const;

	/** 현재 예약한 Smart Object를 소유한 Map Actor를 반환한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	AActor* GetReservedSmartObjectActor() const;

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	FSmartObjectClaimHandle GetClaimHandle() const { return ClaimHandle; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	bool GetReservedSlotTransform(FTransform& OutSlotTransform) const;

	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	void SetRequiredActivityTags(const FGameplayTagContainer& NewActivityTags);

	UFUNCTION(BlueprintCallable, Category="Drone|AI|SmartObject")
	void SetUserTags(const FGameplayTagContainer& NewUserTags);

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	const FGameplayTagContainer& GetRequiredActivityTags() const { return RequiredActivityTags; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	const FGameplayTagContainer& GetUserTags() const { return UserTags; }

	UPROPERTY(BlueprintAssignable, Category="Drone|AI|SmartObject")
	FDroneSmartObjectReservationChangedSignature OnReservationChanged;

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	float GetSearchRadius() const { return SearchRadius; }

	UFUNCTION(BlueprintPure, Category="Drone|AI|SmartObject")
	float GetSearchHalfHeight() const { return SearchHalfHeight; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** XY 검색 반경. 최종 Level 규모와 NavMesh 확인 뒤 Blueprint에서 조정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Search", meta=(ClampMin="100.0"))
	float SearchRadius = 2500.0f;

	/** Z 검색 반높이. 고저차가 큰 Map은 별도 튜닝한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Search", meta=(ClampMin="100.0"))
	float SearchHalfHeight = 1000.0f;

	/** Smart Object Definition Slot의 Activity Tag와 비교한다. 비어 있으면 검색하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Filter")
	FGameplayTagContainer RequiredActivityTags;

	/** Faction, Weapon, Role Tag. NPC Profile에서 공급한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Filter")
	FGameplayTagContainer UserTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Filter")
	ESmartObjectClaimPriority ClaimPriority = ESmartObjectClaimPriority::Normal;

	/** 기본값은 UE 5.8 Gameplay Interaction(StateTree) Behavior Definition이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Filter")
	TArray<TSubclassOf<USmartObjectBehaviorDefinition>> BehaviorDefinitionClasses;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Runtime")
	FSmartObjectClaimHandle ClaimHandle;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Drone|AI|SmartObject|Runtime")
	FTransform CachedSlotTransform = FTransform::Identity;

private:
	bool ClaimNearestAvailableSlotInternal(
		const FVector& SearchOrigin,
		const FVector* AvoidLocation,
		float AvoidRadius,
		FTransform& OutSlotTransform);
};
