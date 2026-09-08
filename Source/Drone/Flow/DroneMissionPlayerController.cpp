#include "Flow/DroneMissionPlayerController.h"

#include "Drone.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Mission/DroneDefinition.h"
#include "Mission/DroneMissionDefinition.h"
#include "Mission/DroneMissionDirector.h"
#include "Prototype/DronePrototypePawn.h"
#include "UI/DroneFlightHUDWidget.h"
#include "UI/DroneMissionObjectiveWidget.h"
#include "UI/DroneMissionResultWidget.h"
#include "UI/DroneSelectionWidget.h"

ADroneMissionPlayerController::ADroneMissionPlayerController()
{
	DroneSelectionWidgetClass = UDroneSelectionWidget::StaticClass();
	MissionDirectorClass = ADroneMissionDirector::StaticClass();
	MissionObjectiveWidgetClass = UDroneMissionObjectiveWidget::StaticClass();
	MissionResultWidgetClass = UDroneMissionResultWidget::StaticClass();
	bShowMouseCursor = true;
}

void ADroneMissionPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalPlayerController())
	{
		return;
	}

	UDroneGameFlowSubsystem* Flow = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
		: nullptr;
	if (!Flow || !Flow->EnsureDefaultCatalog())
	{
		UE_LOG(LogDrone, Error, TEXT("Mission entry could not prepare the Mission/Drone catalog."));
		return;
	}

	// OpenLevel 동안 유지된 GameInstance 선택을 Map 준비 완료 상태로 한 번만 넘긴다.
	if (Flow->GetSnapshot().State == EDroneGameFlowState::LoadingMissionMap
		&& !Flow->NotifyMissionMapReady())
	{
		UE_LOG(LogDrone, Error, TEXT("Mission entry could not enter Drone Select."));
		return;
	}
	if (Flow->GetSnapshot().State != EDroneGameFlowState::DroneSelect)
	{
		UE_LOG(
			LogDrone,
			Warning,
			TEXT("Mission entry started in unexpected Flow state %d."),
			static_cast<uint8>(Flow->GetSnapshot().State));
		return;
	}

	CreateDroneSelectionWidget(Flow);
	if (UDroneFlightHUDWidget* FlightHUD = GetFlightHUDWidget())
	{
		FlightHUD->SetVisibility(ESlateVisibility::Collapsed);
	}

	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	if (DroneSelectionWidget)
	{
		InputMode.SetWidgetToFocus(DroneSelectionWidget->TakeWidget());
	}
	SetInputMode(InputMode);
}

void ADroneMissionPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DroneSelectionWidget)
	{
		DroneSelectionWidget->SetFlowSubsystem(nullptr);
		DroneSelectionWidget->RemoveFromParent();
		DroneSelectionWidget = nullptr;
	}
	if (MissionObjectiveWidget)
	{
		MissionObjectiveWidget->ClearDronePawn();
		MissionObjectiveWidget->ClearMissionDirector();
		MissionObjectiveWidget->RemoveFromParent();
		MissionObjectiveWidget = nullptr;
	}
	if (MissionResultWidget)
	{
		MissionResultWidget->RemoveFromParent();
		MissionResultWidget = nullptr;
	}
	if (MissionDirector)
	{
		MissionDirector->OnMissionFinished.RemoveDynamic(
			this,
			&ADroneMissionPlayerController::HandleMissionFinished);
		MissionDirector = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool ADroneMissionPlayerController::StartSelectedDrone(
	const EDroneControlMode ControlMode,
	const EDroneHandlingPreset HandlingPreset)
{
	UDroneGameFlowSubsystem* Flow = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
		: nullptr;
	UDroneDefinition* Definition = Flow ? Flow->GetSelectedDroneDefinition() : nullptr;
	UDroneMissionDefinition* SelectedMission = Flow
		? Flow->FindMissionDefinition(Flow->GetSnapshot().SelectedMissionId)
		: nullptr;
	if (!Flow
		|| Flow->GetSnapshot().State != EDroneGameFlowState::DroneSelect
		|| !Definition
		|| !SelectedMission
		|| SelectedMission->InitialObjectives.IsEmpty()
		|| IsValid(SpawnedDrone)
		|| SuccessfulDroneSpawnCount > 0)
	{
		return false;
	}

	UClass* PawnClass = Definition->PawnClass.LoadSynchronous();
	if (!PawnClass || !PawnClass->IsChildOf(ADronePrototypePawn::StaticClass()) || !GetWorld())
	{
		UE_LOG(LogDrone, Error, TEXT("Selected Drone '%s' has no compatible Prototype Pawn class."), *Definition->DroneId.ToString());
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADronePrototypePawn* NewDrone = GetWorld()->SpawnActor<ADronePrototypePawn>(
		PawnClass,
		ResolveDroneSpawnTransform(),
		SpawnParameters);
	if (!NewDrone)
	{
		UE_LOG(LogDrone, Error, TEXT("Selected Drone '%s' could not be spawned."), *Definition->DroneId.ToString());
		return false;
	}

	if (!NewDrone->ApplyDroneDefinition(Definition))
	{
		NewDrone->Destroy();
		return false;
	}
	// 기체 Data Asset의 기본값을 먼저 적용한 뒤 이번 출격에서 사용자가 고른 독립 설정으로 덮어쓴다.
	NewDrone->SetControlMode(ControlMode);
	NewDrone->SetHandlingPreset(HandlingPreset);

	// Flow를 바꾸기 전에 Director Actor 생성 가능 여부를 확인해 실패 시 DroneSelect를 그대로 유지한다.
	FActorSpawnParameters DirectorSpawnParameters;
	DirectorSpawnParameters.Owner = this;
	DirectorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADroneMissionDirector* NewDirector = MissionDirectorClass
		? GetWorld()->SpawnActor<ADroneMissionDirector>(
			MissionDirectorClass,
			FTransform::Identity,
			DirectorSpawnParameters)
		: nullptr;
	if (!NewDirector)
	{
		UE_LOG(LogDrone, Error, TEXT("Mission Director could not be spawned."));
		NewDrone->Destroy();
		return false;
	}

	APawn* PreviousPawn = GetPawn();
	Possess(NewDrone);
	if (GetPawn() != NewDrone)
	{
		NewDirector->Destroy();
		NewDrone->Destroy();
		return false;
	}
	if (!Flow->RequestMissionStart())
	{
		UnPossess();
		if (IsValid(PreviousPawn))
		{
			Possess(PreviousPawn);
		}
		NewDirector->Destroy();
		NewDrone->Destroy();
		return false;
	}

	NewDirector->OnMissionFinished.AddUniqueDynamic(
		this,
		&ADroneMissionPlayerController::HandleMissionFinished);
	if (!NewDirector->InitializeMission(Flow, SelectedMission, NewDrone))
	{
		UE_LOG(LogDrone, Error, TEXT("Mission Director could not initialize the selected Mission."));
		NewDirector->OnMissionFinished.RemoveDynamic(
			this,
			&ADroneMissionPlayerController::HandleMissionFinished);
		// 이 경로는 계약 오류다. InMission에 멈추지 않도록 일관된 실패 결과로 닫는다.
		Flow->CompleteMission(EDroneMissionOutcome::Failure);
		NewDirector->Destroy();
		return false;
	}

	if (IsValid(PreviousPawn) && PreviousPawn != NewDrone && PreviousPawn->IsA<ASpectatorPawn>())
	{
		PreviousPawn->Destroy();
	}
	SpawnedDrone = NewDrone;
	MissionDirector = NewDirector;
	++SuccessfulDroneSpawnCount;
	CreateMissionObjectiveWidget(NewDirector);

	if (DroneSelectionWidget)
	{
		DroneSelectionWidget->SetFlowSubsystem(nullptr);
		DroneSelectionWidget->RemoveFromParent();
		DroneSelectionWidget = nullptr;
	}
	if (UDroneFlightHUDWidget* FlightHUD = GetFlightHUDWidget())
	{
		FlightHUD->SetVisibility(ESlateVisibility::Visible);
	}
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	return true;
}

bool ADroneMissionPlayerController::RetrySelectedMission()
{
	UDroneGameFlowSubsystem* Flow = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
		: nullptr;
	UDroneMissionDefinition* SelectedMission = Flow
		? Flow->FindMissionDefinition(Flow->GetSnapshot().SelectedMissionId)
		: nullptr;
	if (!Flow || !SelectedMission || SelectedMission->MissionMap.IsNull() || !Flow->RequestRetry())
	{
		return false;
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(
		this,
		SelectedMission->MissionMap,
		true,
		TEXT("game=/Script/Drone.DroneMissionGameMode"));
	return true;
}

bool ADroneMissionPlayerController::ReturnToFrontEndLobby()
{
	UDroneGameFlowSubsystem* Flow = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>()
		: nullptr;
	if (!Flow || !Flow->RequestReturnToLobby())
	{
		return false;
	}
	UGameplayStatics::OpenLevel(
		this,
		FName(TEXT("/Game/Drone/Maps/Lvl_DroneFrontEnd")),
		true);
	return true;
}

void ADroneMissionPlayerController::CreateDroneSelectionWidget(UDroneGameFlowSubsystem* Flow)
{
	if (DroneSelectionWidget || !DroneSelectionWidgetClass || !Flow)
	{
		return;
	}
	DroneSelectionWidget = CreateWidget<UDroneSelectionWidget>(this, DroneSelectionWidgetClass);
	if (!DroneSelectionWidget || !DroneSelectionWidget->AddToPlayerScreen(20))
	{
		UE_LOG(LogDrone, Error, TEXT("Mission entry could not create or display the Drone Selection Widget."));
		DroneSelectionWidget = nullptr;
		return;
	}
	DroneSelectionWidget->SetFlowSubsystem(Flow);
}

void ADroneMissionPlayerController::CreateMissionObjectiveWidget(
	ADroneMissionDirector* InMissionDirector)
{
	if (MissionObjectiveWidget || !MissionObjectiveWidgetClass || !InMissionDirector)
	{
		return;
	}
	MissionObjectiveWidget = CreateWidget<UDroneMissionObjectiveWidget>(this, MissionObjectiveWidgetClass);
	if (!MissionObjectiveWidget || !MissionObjectiveWidget->AddToPlayerScreen(15))
	{
		UE_LOG(LogDrone, Error, TEXT("Mission Objective Widget could not be displayed."));
		MissionObjectiveWidget = nullptr;
		return;
	}
	MissionObjectiveWidget->SetMissionDirector(InMissionDirector);
	MissionObjectiveWidget->SetDronePawn(SpawnedDrone);
}

void ADroneMissionPlayerController::CreateMissionResultWidget(const EDroneMissionOutcome Outcome)
{
	if (MissionResultWidget || !MissionResultWidgetClass || Outcome == EDroneMissionOutcome::None)
	{
		return;
	}
	MissionResultWidget = CreateWidget<UDroneMissionResultWidget>(this, MissionResultWidgetClass);
	if (!MissionResultWidget || !MissionResultWidget->AddToPlayerScreen(30))
	{
		UE_LOG(LogDrone, Error, TEXT("Mission Result Widget could not be displayed."));
		MissionResultWidget = nullptr;
		return;
	}
	MissionResultWidget->ConfigureResult(this, Outcome);
}

void ADroneMissionPlayerController::HandleMissionFinished(const EDroneMissionOutcome Outcome)
{
	if (MissionObjectiveWidget)
	{
		MissionObjectiveWidget->ClearDronePawn();
		MissionObjectiveWidget->ClearMissionDirector();
		MissionObjectiveWidget->RemoveFromParent();
		MissionObjectiveWidget = nullptr;
	}
	CreateMissionResultWidget(Outcome);
	SetIgnoreMoveInput(true);
	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	if (MissionResultWidget)
	{
		InputMode.SetWidgetToFocus(MissionResultWidget->TakeWidget());
	}
	SetInputMode(InputMode);
}

FTransform ADroneMissionPlayerController::ResolveDroneSpawnTransform() const
{
	if (UWorld* World = GetWorld())
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			if (const AActor* ResolvedStart = GameMode->FindPlayerStart(
				const_cast<ADroneMissionPlayerController*>(this)))
			{
				return ResolvedStart->GetActorTransform();
			}
		}
	}
	if (const APawn* ExistingPawn = GetPawn())
	{
		return ExistingPawn->GetActorTransform();
	}
	return FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 250.0f));
}
