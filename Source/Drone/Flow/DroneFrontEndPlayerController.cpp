#include "Flow/DroneFrontEndPlayerController.h"

#include "Drone.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Mission/DroneMissionDefinition.h"
#include "UI/DroneFrontEndRootWidget.h"

ADroneFrontEndPlayerController::ADroneFrontEndPlayerController()
{
	FrontEndWidgetClass = UDroneFrontEndRootWidget::StaticClass();
	bShowMouseCursor = true;
}

void ADroneFrontEndPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalPlayerController())
	{
		return;
	}

	CreateFrontEndWidget();
	UDroneGameFlowSubsystem* Flow = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
		: nullptr;
	if (!Flow || !Flow->EnsureDefaultCatalog())
	{
		UE_LOG(LogDrone, Error, TEXT("Front-end could not prepare the default Mission/Drone catalog."));
		return;
	}

	if (FrontEndWidget)
	{
		FrontEndWidget->OnMissionMapLoadRequested.AddUniqueDynamic(
			this,
			&ADroneFrontEndPlayerController::HandleMissionMapLoadRequested);
		FrontEndWidget->SetFlowSubsystem(Flow);
	}
	// Mission 결과 화면에서 돌아왔다면 로비 상태와 선택 초기화는 유지하고 요청 표지만 한 번 소비한다.
	if (Flow->GetSnapshot().State == EDroneGameFlowState::LobbyMissionSelect
		&& Flow->GetSnapshot().bLobbyReturnRequested
		&& !Flow->ConsumeLobbyReturnRequest())
	{
		UE_LOG(LogDrone, Warning, TEXT("Front-end could not consume the Lobby return request."));
	}

	// 새 실행에서만 Opening Trailer를 시작한다. 맵 재진입 시 이미 보존된 상태를 덮어쓰지 않는다.
	if (Flow->GetSnapshot().State == EDroneGameFlowState::Boot && !Flow->BeginOpeningTrailer())
	{
		UE_LOG(LogDrone, Error, TEXT("Front-end could not enter the Opening Trailer state."));
	}

	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	if (FrontEndWidget)
	{
		InputMode.SetWidgetToFocus(FrontEndWidget->TakeWidget());
	}
	SetInputMode(InputMode);
}

void ADroneFrontEndPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FrontEndWidget)
	{
		FrontEndWidget->OnMissionMapLoadRequested.RemoveDynamic(
			this,
			&ADroneFrontEndPlayerController::HandleMissionMapLoadRequested);
		FrontEndWidget->SetFlowSubsystem(nullptr);
		FrontEndWidget->RemoveFromParent();
		FrontEndWidget = nullptr;
	}
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	Super::EndPlay(EndPlayReason);
}

void ADroneFrontEndPlayerController::HandleMissionMapLoadRequested(
	UDroneMissionDefinition* MissionDefinition)
{
	UDroneGameFlowSubsystem* Flow = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
		: nullptr;
	if (!Flow
		|| Flow->GetSnapshot().State != EDroneGameFlowState::LoadingMissionMap
		|| !IsValid(MissionDefinition)
		|| MissionDefinition->MissionId != Flow->GetSnapshot().SelectedMissionId
		|| MissionDefinition->MissionMap.IsNull()
		|| MissionMapLoadRequestCount > 0)
	{
		UE_LOG(LogDrone, Warning, TEXT("Front-end rejected an invalid or duplicate Mission Map load request."));
		return;
	}

	++MissionMapLoadRequestCount;
	LastRequestedMissionMap = MissionDefinition->MissionMap.ToSoftObjectPath();
	// URL의 native Mission GameMode가 Map에 저장된 Prototype GameMode를 이번 진입에만 덮어쓴다.
	// 따라서 선택 전에는 Drone이 생기지 않고, 기존 Training Map을 직접 열어 시험하는 흐름도 유지된다.
	UGameplayStatics::OpenLevelBySoftObjectPtr(
		this,
		MissionDefinition->MissionMap,
		true,
		TEXT("game=/Script/Drone.DroneMissionGameMode"));
}

void ADroneFrontEndPlayerController::CreateFrontEndWidget()
{
	if (FrontEndWidget || !FrontEndWidgetClass)
	{
		return;
	}

	FrontEndWidget = CreateWidget<UDroneFrontEndRootWidget>(this, FrontEndWidgetClass);
	if (!FrontEndWidget || !FrontEndWidget->AddToPlayerScreen(0))
	{
		UE_LOG(LogDrone, Error, TEXT("Front-end could not create or display its Root Widget."));
		FrontEndWidget = nullptr;
		return;
	}
	++FrontEndWidgetCreationCount;
}
