#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionPlayerController.h"
#include "HAL/PlatformTime.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneMissionCheckpoint.h"
#include "Mission/DroneMissionDefinition.h"
#include "Mission/DroneMissionDirector.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneSelectionWidget.h"

/**
 * MISSION-CHECKPOINT-01: 실패를 재출격으로 처리하는 미션에서
 * - 체크포인트(조건·1회) 갱신,
 * - 기체 파괴 → 같은 미션을 유지한 채 체크포인트 위치에 새 기체 출격·빙의·Director 재연결,
 * - 허용 횟수를 다 쓰면 기존처럼 실패 결과로 닫힘을 확인한다.
 * Hover 수업 맵을 직접 연다. Hover DA의 실패 처리 설정은 테스트 동안 메모리에서만 바꾸고 끝나면 되돌린다(저장하지 않음).
 */
namespace DroneCheckpointRestartPIE
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/TestMap/Tutorial/Lvl_Tutorial_Hover_Test");
const FName ScoutId(TEXT("Drone.Scout.Greybox"));
constexpr double StepTimeoutSeconds = 10.0;

UWorld* FindPIEWorld()
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World()) return Context.World();
		}
	}
	return nullptr;
}

FRequestPlaySessionParams MakePlayParams()
{
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(1);
	Settings->bLaunchSeparateServer = false;
	Settings->AddToRoot();
	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.EditorPlaySettings = Settings;
	Params.bAllowOnlineSubsystem = false;
	return Params;
}

class FValidateCheckpointRestartCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateCheckpointRestartCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual ~FValidateCheckpointRestartCommand() override { RestoreDefinition(); }

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (PhaseStartedAt == 0.0) PhaseStartedAt = Now;
		UWorld* World = FindPIEWorld();
		ADroneMissionPlayerController* Controller = World ? World->GetFirstPlayerController<ADroneMissionPlayerController>() : nullptr;
		UDroneGameFlowSubsystem* Flow = World && World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>() : nullptr;
		if (!World || !World->HasBegunPlay() || !Controller || !Flow)
		{
			return Timeout(Now, TEXT("Hover map PIE did not become ready"));
		}
		ADroneMissionDirector* Director = Controller->GetMissionDirector();

		switch (Phase)
		{
		case 0: // 기체 선택 화면에서 Hover DA를 재출격 모드(최대 2회)로 바꾸고 Scout으로 출격한다.
		{
			UDroneSelectionWidget* Selection = Controller->GetDroneSelectionWidget();
			UDroneMissionDefinition* Mission = Flow->FindMissionDefinition(Flow->GetSnapshot().SelectedMissionId);
			if (!Selection || !Mission || Flow->GetSnapshot().State != EDroneGameFlowState::DroneSelect)
			{
				return Timeout(Now, TEXT("Hover map did not reach Drone selection"));
			}
			EditedMission = Mission;
			OriginalResponse = Mission->FailureResponse;
			OriginalMaxRestarts = Mission->MaxCheckpointRestarts;
			Mission->FailureResponse = EDroneMissionFailureResponse::RestartFromCheckpoint;
			Mission->MaxCheckpointRestarts = 2;
			Test->TestTrue(TEXT("Scout can be selected"), Selection->SelectDrone(ScoutId));
			Test->TestTrue(TEXT("Scout launches"), Selection->ConfirmAndLaunchSelectedDrone());
			return Next(Now);
		}
		case 1: // 체크포인트: 조건 불일치는 무시, 조건 없는 체크포인트는 1회만 갱신.
		{
			ADronePrototypePawn* Drone = Controller->GetSpawnedDrone();
			if (!Director || !Director->IsMissionActive() || !Drone)
			{
				return Timeout(Now, TEXT("Mission did not start"));
			}
			Test->TestTrue(TEXT("Restart mode is armed (DA + Controller binding)"), Director->CanRestartFromCheckpoint());
			const FVector Away = Drone->GetActorLocation() + Drone->GetActorForwardVector() * 1500.0f + FVector(0, 0, 800.0f);

			ADroneMissionCheckpoint* Gated = World->SpawnActor<ADroneMissionCheckpoint>(Away, FRotator::ZeroRotator);
			Gated->CheckpointId = TEXT("Test.Gated");
			Gated->RequiredObjectiveId = TEXT("Objective.NotCurrent");
			Test->TestFalse(TEXT("Checkpoint for another objective is ignored"), Gated->TryActivate(Drone));

			Checkpoint = World->SpawnActor<ADroneMissionCheckpoint>(Away, FRotator(0.0f, 90.0f, 0.0f));
			Checkpoint->CheckpointId = TEXT("Test.Checkpoint");
			Test->TestTrue(TEXT("Checkpoint activates for the player Drone"), Checkpoint->TryActivate(Drone));
			Test->TestFalse(TEXT("Activate-once checkpoint ignores a second pass"), Checkpoint->TryActivate(Drone));
			Test->TestEqual(TEXT("Snapshot records the checkpoint"), Director->GetSnapshot().LastCheckpointId, FName(TEXT("Test.Checkpoint")));
			return Next(Now);
		}
		case 2: // 1번째 파괴 → 재출격
		case 4: // 2번째 파괴 → 재출격
		{
			ADronePrototypePawn* Drone = Controller->GetSpawnedDrone();
			if (!Drone) return Timeout(Now, TEXT("No Drone to destroy"));
			KilledDrone = Drone;
			Drone->GetHealthComponent()->ApplyHealthDamage(100000.0f, Controller, Drone);
			return Next(Now);
		}
		case 3:
		case 5:
		{
			const int32 ExpectedRestarts = Phase == 3 ? 1 : 2;
			ADronePrototypePawn* NewDrone = Controller->GetSpawnedDrone();
			if (!Director || Director->GetSnapshot().RestartCount != ExpectedRestarts || !NewDrone || NewDrone == KilledDrone.Get())
			{
				return Timeout(Now, *FString::Printf(TEXT("Restart %d did not spawn a new Drone"), ExpectedRestarts));
			}
			Test->TestEqual(*FString::Printf(TEXT("Restart %d keeps the Flow in mission"), ExpectedRestarts),
				Flow->GetSnapshot().State, EDroneGameFlowState::InMission);
			Test->TestTrue(*FString::Printf(TEXT("Restart %d keeps the Director active"), ExpectedRestarts), Director->IsMissionActive());
			Test->TestTrue(*FString::Printf(TEXT("Restart %d possesses the new Drone"), ExpectedRestarts), Controller->GetPawn() == NewDrone);
			Test->TestTrue(*FString::Printf(TEXT("Restart %d rebinds the Director to the new Drone"), ExpectedRestarts),
				Director->GetActiveDrone() == NewDrone);
			Test->TestFalse(*FString::Printf(TEXT("Restart %d removes the destroyed Drone"), ExpectedRestarts), IsValid(KilledDrone.Get()));
			const float Distance = FVector::Dist(NewDrone->GetActorLocation(), Checkpoint->GetRestartTransform().GetLocation());
			Test->TestTrue(*FString::Printf(TEXT("Restart %d spawns at the checkpoint (%.0f cm)"), ExpectedRestarts, Distance), Distance < 300.0f);
			const float Yaw = NewDrone->GetActorRotation().Yaw;
			Test->TestTrue(FString::Printf(TEXT("Restart %d faces the checkpoint arrow (yaw %.1f)"), ExpectedRestarts, Yaw),
				FMath::IsNearlyEqual(Yaw, 90.0f, 1.0f));
			return Next(Now);
		}
		case 6: // 허용 횟수(2)를 다 쓴 뒤 파괴 → 기존 실패 결과
		{
			ADronePrototypePawn* Drone = Controller->GetSpawnedDrone();
			if (!Drone) return Timeout(Now, TEXT("No Drone for the final failure"));
			Test->TestFalse(TEXT("Restart budget is spent"), Director && Director->CanRestartFromCheckpoint());
			Drone->GetHealthComponent()->ApplyHealthDamage(100000.0f, Controller, Drone);
			return Next(Now);
		}
		case 7:
		{
			if (Flow->GetSnapshot().State != EDroneGameFlowState::MissionResult)
			{
				return Timeout(Now, TEXT("Final failure did not open the result"));
			}
			Test->TestEqual(TEXT("Final failure records Failure"), Flow->GetSnapshot().LastMissionOutcome, EDroneMissionOutcome::Failure);
			RestoreDefinition();
			return true;
		}
		default:
			return true;
		}
	}

private:
	bool Next(const double Now)
	{
		++Phase;
		PhaseStartedAt = Now;
		return false;
	}

	bool Timeout(const double Now, const TCHAR* Message)
	{
		if (Now - PhaseStartedAt > StepTimeoutSeconds)
		{
			Test->AddError(FString::Printf(TEXT("[MISSION-CHECKPOINT] phase %d: %s"), Phase, Message));
			RestoreDefinition();
			return true;
		}
		return false;
	}

	void RestoreDefinition()
	{
		if (UDroneMissionDefinition* Mission = EditedMission.Get())
		{
			Mission->FailureResponse = OriginalResponse;
			Mission->MaxCheckpointRestarts = OriginalMaxRestarts;
			EditedMission.Reset();
		}
	}

	FAutomationTestBase* Test;
	int32 Phase = 0;
	double PhaseStartedAt = 0.0;
	TWeakObjectPtr<UDroneMissionDefinition> EditedMission;
	EDroneMissionFailureResponse OriginalResponse = EDroneMissionFailureResponse::ShowResult;
	int32 OriginalMaxRestarts = 0;
	ADroneMissionCheckpoint* Checkpoint = nullptr;
	TWeakObjectPtr<ADronePrototypePawn> KilledDrone;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCheckpointRestartPIETest,
	"Drone.Mission.CheckpointRestartPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneCheckpointRestartPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneCheckpointRestartPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Checkpoint restart PIE requires an idle Editor"));
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateCheckpointRestartCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
