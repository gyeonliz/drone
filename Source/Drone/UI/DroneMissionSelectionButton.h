#pragma once

#include "Components/Button.h"
#include "CoreMinimal.h"
#include "DroneMissionSelectionButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDroneMissionSelectionRequestedSignature,
	FName, MissionId);

/** C++ fallback 로비에서 여러 Mission 버튼이 자신의 Mission ID를 전달하는 작은 UI 경계다. */
UCLASS()
class DRONE_API UDroneMissionSelectionButton : public UButton
{
	GENERATED_BODY()

public:
	void InitializeMissionSelection(FName InMissionId);

	UFUNCTION(BlueprintPure, Category="Drone|Front End|Lobby")
	FName GetMissionId() const { return MissionId; }

	UPROPERTY(BlueprintAssignable, Category="Drone|Front End|Lobby")
	FDroneMissionSelectionRequestedSignature OnMissionSelectionRequested;

private:
	UFUNCTION()
	void HandleMissionButtonClicked();

	UPROPERTY(Transient)
	FName MissionId = NAME_None;
};
