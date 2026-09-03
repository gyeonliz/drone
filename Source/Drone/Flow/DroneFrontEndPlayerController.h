#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DroneFrontEndPlayerController.generated.h"

class UDroneFrontEndRootWidget;

/** UI 전용 Front-end Map에서 Root Widget 하나와 입력 모드를 소유한다. */
UCLASS(Blueprintable)
class DRONE_API ADroneFrontEndPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADroneFrontEndPlayerController();

	UFUNCTION(BlueprintPure, Category="Drone|Front End")
	UDroneFrontEndRootWidget* GetFrontEndWidget() const { return FrontEndWidget; }

	UFUNCTION(BlueprintPure, Category="Drone|Front End")
	TSubclassOf<UDroneFrontEndRootWidget> GetFrontEndWidgetClass() const { return FrontEndWidgetClass; }

	/** 중복 BeginPlay/호출 방어를 자동화에서 확인하는 생성 횟수다. */
	UFUNCTION(BlueprintPure, Category="Drone|Front End")
	int32 GetFrontEndWidgetCreationCount() const { return FrontEndWidgetCreationCount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CreateFrontEndWidget();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone|Front End", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UDroneFrontEndRootWidget> FrontEndWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDroneFrontEndRootWidget> FrontEndWidget;

	UPROPERTY(Transient)
	int32 FrontEndWidgetCreationCount = 0;
};
