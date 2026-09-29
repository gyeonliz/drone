#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DronePrototypePlayerController.generated.h"

class UDroneFlightHUDWidget;
class ADroneTrainingCourse;
class ADroneTrainingRouteSelector;

/**
 * Drone Prototype의 로컬 HUD 수명주기를 소유하는 PlayerController.
 *
 * Pawn은 UnPossess/교체될 수 있으므로 HUD를 Pawn에 만들지 않는다. 더 오래 살아 있는
 * PlayerController가 Widget 하나를 생성·재사용하고, 현재 Possess Pawn의 Telemetry만
 * 연결한다. BP_DronePrototypePlayerController에서는 FlightHUDWidgetClass만 WBP로
 * 지정하며 생성·구독·정리 로직은 C++ 한 곳에 유지한다.
 */
UCLASS(Blueprintable)
class ADronePrototypePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADronePrototypePlayerController();

	/** 현재 화면에 생성된 HUD 인스턴스다. BeginPlay 전이나 생성 실패 시 null일 수 있다. */
	UFUNCTION(BlueprintPure, Category="Prototype|UI")
	UDroneFlightHUDWidget* GetFlightHUDWidget() const { return FlightHUDWidget; }

	/** Class Defaults에서 선택한 WBP 또는 native fallback Class를 반환한다. */
	UFUNCTION(BlueprintPure, Category="Prototype|UI")
	TSubclassOf<UDroneFlightHUDWidget> GetFlightHUDWidgetClass() const { return FlightHUDWidgetClass; }

protected:
	/** 로컬 Controller만 HUD를 만들고, 이미 Possess된 Pawn까지 즉시 동기화한다. */
	virtual void BeginPlay() override;

	/** PIE 종료/맵 전환에서도 Delegate와 Viewport 참조가 남지 않게 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/**
	 * BP_DronePrototypePlayerController의 Class Defaults에서 WBP_DroneFlightHUD를 지정한다.
	 * 지정하지 않으면 생성자에서 설정한 native UDroneFlightHUDWidget을 안전망으로 사용한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UDroneFlightHUDWidget> FlightHUDWidgetClass;

	/** Controller 수명 동안 하나만 만들고 Pawn이 바뀌어도 같은 인스턴스를 재사용한다. */
	UPROPERTY(Transient)
	TObjectPtr<UDroneFlightHUDWidget> FlightHUDWidget;

	TWeakObjectPtr<ADroneTrainingRouteSelector> TrainingRouteSelector;

	/** AController가 제공하는 Possess 변경 Event의 수신 함수다. */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

	/** Route TestMap에서 숫자키로 Course가 바뀌면 같은 HUD의 기록 Source만 교체한다. */
	UFUNCTION()
	void HandleTrainingRouteChanged(int32 ActiveRouteNumber, ADroneTrainingCourse* ActiveCourse);

	/** 선택한 Class로 HUD를 한 번만 생성해 로컬 Player Layer에 올린다. */
	void CreateFlightHUD();

	/** 특정 Pawn 타입 대신 Telemetry Component 유무만 보고 HUD Source를 바꾼다. */
	void SyncFlightHUDToPawn(APawn* NewPawn);

	/** 현재 World의 Tutorial Course가 있으면 같은 HUD에 구간 기록 Source를 한 번 연결한다. */
	void SyncTrainingHUDToWorld();
};
