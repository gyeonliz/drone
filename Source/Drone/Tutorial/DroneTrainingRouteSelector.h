#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneTrainingRouteSelector.generated.h"

class ADroneTrainingCourse;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDroneTrainingRouteChangedSignature,
	int32, ActiveRouteNumber,
	ADroneTrainingCourse*, ActiveCourse);

/**
 * TestMap의 Training Course 네 개 중 하나만 Runtime에서 활성화한다.
 * 숫자 1~4는 고정 선택, 숫자 5는 무작위 선택의 단일 입력 소유자다.
 */
UCLASS(Blueprintable)
class DRONE_API ADroneTrainingRouteSelector : public AActor
{
	GENERATED_BODY()

public:
	ADroneTrainingRouteSelector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Tutorial|Route Selection")
	void ConfigureRoutes(const TArray<ADroneTrainingCourse*>& InRoutes);

	/** 1부터 시작하는 Route 번호를 활성화한다. */
	UFUNCTION(BlueprintCallable, Category="Tutorial|Route Selection")
	bool ActivateRouteNumber(int32 RouteNumber);

	/** 유효 Route 중 하나를 무작위로 활성화한다. */
	UFUNCTION(BlueprintCallable, Category="Tutorial|Route Selection")
	bool ActivateRandomRoute();

	UFUNCTION(BlueprintPure, Category="Tutorial|Route Selection")
	int32 GetConfiguredRouteCount() const { return Routes.Num(); }

	UFUNCTION(BlueprintPure, Category="Tutorial|Route Selection")
	int32 GetActiveRouteNumber() const { return ActiveRouteIndex == INDEX_NONE ? 0 : ActiveRouteIndex + 1; }

	UFUNCTION(BlueprintPure, Category="Tutorial|Route Selection")
	ADroneTrainingCourse* GetActiveRoute() const;

	UFUNCTION(BlueprintPure, Category="Tutorial|Route Selection")
	bool IsRouteActive(int32 RouteNumber) const;

	UPROPERTY(BlueprintAssignable, Category="Tutorial|Route Selection")
	FDroneTrainingRouteChangedSignature OnActiveRouteChanged;

protected:
	/** TestMap에서 순서대로 Route 1~4를 연결한다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Tutorial|Route Selection")
	TArray<TObjectPtr<ADroneTrainingCourse>> Routes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Route Selection", meta=(ClampMin="1"))
	int32 InitialRouteNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Route Selection")
	int32 RandomSeed = 260929;

	/** Route가 둘 이상이면 5번을 누를 때 현재 Route를 다시 고르지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Route Selection")
	bool bAvoidImmediateRandomRepeat = true;

private:
	bool ApplyActiveRouteIndex(int32 NewActiveRouteIndex);
	void BindTestInput();
	void ShowRouteMessage() const;

	void SelectRouteOne();
	void SelectRouteTwo();
	void SelectRouteThree();
	void SelectRouteFour();
	void SelectRandomRoute();

	UPROPERTY(Transient)
	int32 ActiveRouteIndex = INDEX_NONE;

	FRandomStream RandomStream;
	bool bInputBound = false;
};

