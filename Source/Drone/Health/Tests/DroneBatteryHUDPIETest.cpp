#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionPlayerController.h"
#include "HAL/PlatformTime.h"
#include "Health/DroneBatteryComponent.h"
#include "Mission/DroneDefinition.h"
#include "Mission/DroneMissionDirector.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFlightHUDWidget.h"
#include "UI/DroneSelectionWidget.h"

/**
 * HUD-FIGMA-01: 기체별 배터리와 HUD 기체명·신호 대역.
 * Hover 수업 맵에서 Scout 정의를 메모리에서만 배터리 5초·5.8GHz로 바꿔 출격하고(끝나면 되돌림, 저장 안 함)
 * HUD 표시 → 실제 소모 → 부족 경고 → 소진 시 FailMission 설정이면 실패 보고(Hover는 재출격 설정이라 새 기체 재출격, 배터리 다시 가득)를 본다.
 */
namespace DroneBatteryHUDPIE
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/TestMap/Tutorial/Lvl_Tutorial_Hover_Test");
const FName ScoutId(TEXT("Drone.Scout.Greybox"));

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

class FValidateBatteryHUDCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateBatteryHUDCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual ~FValidateBatteryHUDCommand() override { Restore(); }

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (PhaseStartedAt == 0.0) PhaseStartedAt = Now;
		UWorld* World = FindPIEWorld();
		ADroneMissionPlayerController* Controller = World ? World->GetFirstPlayerController<ADroneMissionPlayerController>() : nullptr;
		UDroneGameFlowSubsystem* Flow = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UDroneGameFlowSubsystem>() : nullptr;
		if (!World || !World->HasBegunPlay() || !Controller || !Flow) return Wait(Now, TEXT("Hover map not ready"));
		UDroneFlightHUDWidget* HUD = Controller->GetFlightHUDWidget();
		ADronePrototypePawn* Drone = Controller->GetSpawnedDrone();
		UDroneBatteryComponent* Battery = Drone ? Drone->GetBatteryComponent() : nullptr;

		switch (Phase)
		{
		case 0:
		{
			UDroneSelectionWidget* Selection = Controller->GetDroneSelectionWidget();
			UDroneDefinition* Scout = Flow->FindDroneDefinition(ScoutId);
			if (!Selection || !Scout || Flow->GetSnapshot().State != EDroneGameFlowState::DroneSelect) return Wait(Now, TEXT("No Drone selection"));
			EditedScout = Scout;
			OriginalBattery = Scout->FlightProfile.BatteryLifeSeconds;
			OriginalBand = Scout->FlightProfile.SignalBandLabel;
			Scout->FlightProfile.BatteryLifeSeconds = 5.0f;
			Scout->FlightProfile.SignalBandLabel = FText::FromString(TEXT("5.8GHz"));
			Test->TestTrue(TEXT("Scout selected"), Selection->SelectDrone(ScoutId));
			Test->TestTrue(TEXT("Scout launched"), Selection->ConfirmAndLaunchSelectedDrone());
			return Next(Now);
		}
		case 1:
			if (!HUD || !Battery || !Battery->IsBatteryEnabled()) return Wait(Now, TEXT("Battery or HUD not bound"));
			Test->TestTrue(TEXT("HUD shows the Drone name"), HUD->GetDroneIdentityDisplayText().ToString().Contains(EditedScout->DisplayName.ToString()));
			Test->TestTrue(TEXT("HUD shows the signal band"), HUD->GetDroneIdentityDisplayText().ToString().Contains(TEXT("5.8GHz")));
			Test->TestTrue(FString::Printf(TEXT("HUD starts at full battery (%s)"), *HUD->GetBatteryDisplayText().ToString()),
				HUD->GetBatteryDisplayText().ToString().StartsWith(TEXT("BATTERY 100%")));
			return Next(Now);
		case 2: // 실제 시간으로 줄어든다
			if (!Battery || Battery->GetRemainingFraction() > 0.85f) return Wait(Now, TEXT("Battery did not drain while flying"));
			Test->TestFalse(TEXT("Battery is not low yet"), Battery->IsLow());
			Battery->ConsumeSeconds(Battery->GetRemainingSeconds() - 0.5f);
			Test->TestTrue(TEXT("Battery reports low under 20%"), Battery->IsLow());
			Test->TestTrue(FString::Printf(TEXT("HUD warns low battery (%s)"), *HUD->GetBatteryDisplayText().ToString()),
				HUD->GetBatteryDisplayText().ToString().Contains(TEXT("부족")));
			// 소진 처리는 현재 미정이라 기본은 경고만. 여기서는 FailMission을 골라 Director 연결을 확인한다.
			Battery->DepletedResponse = EDroneBatteryDepletedResponse::FailMission;
			FirstDrone = Drone;
			return Next(Now);
		case 3: // 소진 → 실패 보고 → Hover는 재출격 설정이므로 새 기체가 가득 찬 배터리로 다시 뜬다
		{
			ADroneMissionDirector* Director = Controller->GetMissionDirector();
			if (!Director || Director->GetSnapshot().RestartCount < 1 || !Drone || Drone == FirstDrone.Get())
			{
				return Wait(Now, TEXT("Depleted battery did not report failure / restart"));
			}
			Test->TestEqual(TEXT("Mission stays in flight after the restart"), Flow->GetSnapshot().State, EDroneGameFlowState::InMission);
			Test->TestTrue(TEXT("Restarted Drone has a fresh battery"), Battery && Battery->GetRemainingFraction() > 0.9f);
			Test->TestEqual(TEXT("Restarted Drone uses the default depleted response again"),
				static_cast<int32>(Battery->DepletedResponse), static_cast<int32>(EDroneBatteryDepletedResponse::WarnOnly));
			Restore();
			return true;
		}
		default:
			return true;
		}
	}

private:
	bool Next(const double Now) { ++Phase; PhaseStartedAt = Now; return false; }
	bool Wait(const double Now, const TCHAR* Message)
	{
		if (Now - PhaseStartedAt > 15.0)
		{
			Test->AddError(FString::Printf(TEXT("[HUD-BATTERY] phase %d: %s"), Phase, Message));
			Restore();
			return true;
		}
		return false;
	}
	void Restore()
	{
		if (UDroneDefinition* Scout = EditedScout.Get())
		{
			Scout->FlightProfile.BatteryLifeSeconds = OriginalBattery;
			Scout->FlightProfile.SignalBandLabel = OriginalBand;
			EditedScout.Reset();
		}
	}

	FAutomationTestBase* Test;
	int32 Phase = 0;
	double PhaseStartedAt = 0.0;
	TWeakObjectPtr<UDroneDefinition> EditedScout;
	float OriginalBattery = 0.0f;
	FText OriginalBand;
	TWeakObjectPtr<ADronePrototypePawn> FirstDrone;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBatteryHUDPIETest,
	"Drone.Health.BatteryHUDPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneBatteryHUDPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneBatteryHUDPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Battery HUD PIE requires an idle Editor"));
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateBatteryHUDCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
