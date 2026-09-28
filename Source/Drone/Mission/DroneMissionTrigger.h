#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mission/DroneMissionObjectiveTypes.h"
#include "DroneMissionTrigger.generated.h"

class ADroneMissionDirector;
class UBoxComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EDroneMissionTriggerAction : uint8
{
	ReportObjectiveEvent UMETA(DisplayName="Report Objective Event"),
	FailMission UMETA(DisplayName="Fail Mission")
};

UENUM(BlueprintType)
enum class EDroneMissionTriggerActorPolicy : uint8
{
	ActivePlayerDrone UMETA(DisplayName="Active Player Drone"),
	ActorWithTag UMETA(DisplayName="Actor With Required Tag"),
	AnyActor UMETA(DisplayName="Any Actor")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneMissionTriggerActivatedSignature,
	AActor*, TriggerActor,
	AActor*, ActivatingActor);

/**
 * Story Map에서 성공 구역, 진행 구역, 차량 도착 실패 구역을 같은 방식으로 배치하는 공용 Box다.
 * 실제 목표 순서와 대상 ID는 Mission Definition/Director가 소유하고 이 Actor는 Overlap 사건만 전달한다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneMissionTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADroneMissionTrigger();

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Trigger")
	UBoxComponent* GetTriggerBox() const { return TriggerBox; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Trigger")
	bool HasTriggered() const { return bHasTriggered; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Trigger")
	EDroneMissionTriggerAction GetTriggerAction() const { return TriggerAction; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Trigger")
	EDroneMissionObjectiveEvent GetObjectiveEvent() const { return ObjectiveEvent; }

	UFUNCTION(BlueprintPure, Category="Drone|Mission|Trigger")
	EDroneMissionTriggerActorPolicy GetActorPolicy() const { return ActorPolicy; }

	/** Overlap 없이 Sequencer·다른 Blueprint·자동화에서 같은 판정 경계를 호출한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission|Trigger")
	bool TryActivateMissionTrigger(AActor* ActivatingActor);

	/** 이미 알고 있는 Director에 직접 전달하는 경계. 동적 연출 Actor와 자동화에서 사용한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Mission|Trigger")
	bool ActivateMissionTriggerWithDirector(ADroneMissionDirector* MissionDirector, AActor* ActivatingActor);

	UFUNCTION(BlueprintCallable, Category="Drone|Mission|Trigger|Setup")
	void ConfigureMissionTrigger(
		EDroneMissionTriggerAction InAction,
		EDroneMissionObjectiveEvent InObjectiveEvent,
		EDroneMissionTriggerActorPolicy InActorPolicy,
		FName InRequiredActorTag,
		bool bInUseOverlappingActorAsEventActor);

	UFUNCTION(BlueprintCallable, Category="Drone|Mission|Trigger")
	void ResetMissionTrigger();

	UPROPERTY(BlueprintAssignable, Category="Drone|Mission|Trigger")
	FDroneMissionTriggerActivatedSignature OnMissionTriggerActivated;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Mission|Trigger")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Trigger")
	EDroneMissionTriggerAction TriggerAction = EDroneMissionTriggerAction::ReportObjectiveEvent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Trigger")
	EDroneMissionObjectiveEvent ObjectiveEvent = EDroneMissionObjectiveEvent::ReturnToBase;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Trigger")
	EDroneMissionTriggerActorPolicy ActorPolicy = EDroneMissionTriggerActorPolicy::ActivePlayerDrone;

	/** ActorWithTag 정책일 때 Overlap Actor에 필요한 Tag다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Trigger")
	FName RequiredActorTag = NAME_None;

	/** false면 이 Trigger 자신의 Actor Tags를 Rule TargetId와 비교한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Trigger")
	bool bUseOverlappingActorAsEventActor = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drone|Mission|Trigger")
	bool bTriggerOnce = true;

private:
	UFUNCTION()
	void HandleTriggerOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	bool IsActorAllowed(AActor* ActivatingActor) const;
	ADroneMissionDirector* ResolveMissionDirector(AActor* ActivatingActor) const;

	UPROPERTY(Transient)
	bool bHasTriggered = false;
};
