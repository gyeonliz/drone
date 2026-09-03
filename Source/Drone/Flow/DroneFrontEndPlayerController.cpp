#include "Flow/DroneFrontEndPlayerController.h"

#include "Drone.h"
#include "Flow/DroneGameFlowSubsystem.h"
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
		FrontEndWidget->SetFlowSubsystem(Flow);
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
		FrontEndWidget->SetFlowSubsystem(nullptr);
		FrontEndWidget->RemoveFromParent();
		FrontEndWidget = nullptr;
	}
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	Super::EndPlay(EndPlayReason);
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
