#include "Prototype/DronePrototypePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Drone.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Health/DroneHealthComponent.h"
#include "Telemetry/DroneTelemetryComponent.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"
#include "UI/DroneFlightHUDWidget.h"

ADronePrototypePlayerController::ADronePrototypePlayerController()
{
	// BP에서 WBP를 지정하지 않아도 자동화와 Greybox 실행이 가능한 native 안전망이다.
	FlightHUDWidgetClass = UDroneFlightHUDWidget::StaticClass();
}

void ADronePrototypePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 서버/원격 Controller에는 화면이 없으므로 로컬 Player만 UMG를 생성한다.
	if (!IsLocalPlayerController())
	{
		return;
	}

	OnPossessedPawnChanged.AddUniqueDynamic(
		this,
		&ADronePrototypePlayerController::HandlePossessedPawnChanged);
	CreateFlightHUD();
	// BeginPlay 전에 GameMode가 Pawn을 이미 Possess했을 수 있어 현재 Pawn도 즉시 연결한다.
	SyncFlightHUDToPawn(GetPawn());
	SyncTrainingHUDToWorld();
}

void ADronePrototypePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Possession Event와 Telemetry Event를 모두 명시적으로 해제해 PIE 반복 실행 누수를 막는다.
	OnPossessedPawnChanged.RemoveDynamic(
		this,
		&ADronePrototypePlayerController::HandlePossessedPawnChanged);

	if (FlightHUDWidget)
	{
		FlightHUDWidget->ClearTrainingRecordSource();
		FlightHUDWidget->ClearHealthSource();
		FlightHUDWidget->ClearTelemetrySource();
		FlightHUDWidget->RemoveFromParent();
		FlightHUDWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ADronePrototypePlayerController::HandlePossessedPawnChanged(APawn* /*PreviousPawn*/, APawn* NewPawn)
{
	// Widget을 새로 만들지 않고 기존 HUD의 데이터 Source만 교체한다.
	SyncFlightHUDToPawn(NewPawn);
}

void ADronePrototypePlayerController::CreateFlightHUD()
{
	if (FlightHUDWidget || !FlightHUDWidgetClass)
	{
		return;
	}

	FlightHUDWidget = CreateWidget<UDroneFlightHUDWidget>(this, FlightHUDWidgetClass);
	if (!FlightHUDWidget)
	{
		UE_LOG(LogDrone, Error, TEXT("Prototype PlayerController could not create its Flight HUD Widget."));
		return;
	}

	// AddToPlayerScreen은 이 Controller의 Local Player Layer에만 추가한다. 10은 초기 HUD ZOrder다.
	if (!FlightHUDWidget->AddToPlayerScreen(10))
	{
		UE_LOG(LogDrone, Error, TEXT("Prototype PlayerController could not add its Flight HUD Widget to the local Player screen."));
		FlightHUDWidget->ClearTelemetrySource();
		FlightHUDWidget = nullptr;
	}
}

void ADronePrototypePlayerController::SyncFlightHUDToPawn(APawn* NewPawn)
{
	if (!FlightHUDWidget)
	{
		return;
	}

	// DronePrototypePawn에 하드 캐스팅하지 않아 향후 다른 Drone Pawn도 Component만 있으면 연결된다.
	UDroneTelemetryComponent* Telemetry = NewPawn
		? NewPawn->FindComponentByClass<UDroneTelemetryComponent>()
		: nullptr;
	UDroneHealthComponent* Health = NewPawn
		? NewPawn->FindComponentByClass<UDroneHealthComponent>()
		: nullptr;
	FlightHUDWidget->SetTelemetrySource(Telemetry);
	FlightHUDWidget->SetHealthSource(Health);
}

void ADronePrototypePlayerController::SyncTrainingHUDToWorld()
{
	if (!FlightHUDWidget)
	{
		return;
	}

	UDroneTrainingLapRecorderComponent* Recorder = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADroneTrainingCourse> CourseIt(World); CourseIt; ++CourseIt)
		{
			ADroneTrainingCourse* Course = *CourseIt;
			if (IsValid(Course))
			{
				Recorder = Course->GetLapRecorderComponent();
				break;
			}
		}
	}

	FlightHUDWidget->SetTrainingRecordSource(Recorder);
}
