#include "UI/DroneMissionSelectionButton.h"

void UDroneMissionSelectionButton::InitializeMissionSelection(const FName InMissionId)
{
	MissionId = InMissionId;
	OnClicked.AddUniqueDynamic(this, &UDroneMissionSelectionButton::HandleMissionButtonClicked);
}

void UDroneMissionSelectionButton::HandleMissionButtonClicked()
{
	if (!MissionId.IsNone())
	{
		OnMissionSelectionRequested.Broadcast(MissionId);
	}
}
