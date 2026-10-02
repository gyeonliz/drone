#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Flow/DroneMissionPlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Mission/DroneMissionDirector.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePawn.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tutorial/DroneTutorialHoverZone.h"
#include "UI/DroneFrontEndRootWidget.h"
#include "UI/DroneSelectionWidget.h"

namespace DroneTutorialHoverPIE
{
constexpr const TCHAR* FrontEndMapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneFrontEnd");

UWorld* FindPIEWorldWithFrontEnd()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType == EWorldType::PIE && World
			&& World->GetFirstPlayerController<ADroneFrontEndPlayerController>())
		{
			return World;
		}
	}
	return nullptr;
}

UWorld* FindPIEWorldWithMission()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType == EWorldType::PIE && World
			&& World->GetFirstPlayerController<ADroneMissionPlayerController>())
		{
			return World;
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

class FStartHoverMissionCommand final : public IAutomationLatentCommand
{
public:
	explicit FStartHoverMissionCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}
		UWorld* World = FindPIEWorldWithFrontEnd();
		ADroneFrontEndPlayerController* Controller = World
			? World->GetFirstPlayerController<ADroneFrontEndPlayerController>() : nullptr;
		UDroneFrontEndRootWidget* Widget = Controller ? Controller->GetFrontEndWidget() : nullptr;
		if (!World || !World->HasBegunPlay() || !Widget)
		{
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Front-end did not become ready for Hover Mission PIE"));
				return true;
			}
			return false;
		}

		const FName MissionId(TEXT("Mission.Tutorial.Hover"));
		// 2026-10 로비 개편 이후 타이틀 [시작]은 Story, [훈련]은 Tutorial 탭으로 들어간다.
		// 둘 다 타이틀(OpeningTrailer)에서만 호출할 수 있으므로 FinishOpeningTrailer 대신 OpenTrainingLobby를 쓴다.
		Test->TestTrue(TEXT("Hover PIE opens the Training lobby from the title"), Widget->OpenTrainingLobby());
		Test->TestTrue(TEXT("Hover PIE selects the Hover lesson"), Widget->SelectLobbyMission(MissionId));
		Test->TestTrue(TEXT("Hover PIE confirms the Hover lesson"), Widget->ConfirmSelectedMission());
		Test->TestTrue(TEXT("Hover PIE finishes the briefing"), Widget->FinishMissionBriefing());
		Test->TestEqual(TEXT("Hover lesson requests its isolated test map"),
			Controller->GetLastRequestedMissionMap(),
			FSoftObjectPath(TEXT("/Game/Drone/Maps/TestMap/Tutorial/Lvl_Tutorial_Hover_Test.Lvl_Tutorial_Hover_Test")));
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
};

class FLaunchAndHoldHoverCommand final : public IAutomationLatentCommand
{
public:
	explicit FLaunchAndHoldHoverCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}
		UWorld* World = FindPIEWorldWithMission();
		ADroneMissionPlayerController* Controller = World
			? World->GetFirstPlayerController<ADroneMissionPlayerController>() : nullptr;
		if (!World || !World->HasBegunPlay() || !Controller)
		{
			if (Now - StartedAt > 30.0)
			{
				Test->AddError(TEXT("Hover Mission map did not become ready"));
				return true;
			}
			return false;
		}

		if (!bLaunched)
		{
			UDroneSelectionWidget* Selection = Controller->GetDroneSelectionWidget();
			if (!Selection)
			{
				if (Now - StartedAt > 30.0)
				{
					Test->AddError(TEXT("Mission Entry did not prepare Drone Selection within 30 seconds"));
					return true;
				}
				return false;
			}
			Test->TestTrue(TEXT("Hover PIE selects Scout"), Selection->SelectDrone(FName(TEXT("Drone.Scout.Greybox"))));
			Test->TestTrue(TEXT("Hover PIE launches Scout"), Selection->ConfirmAndLaunchSelectedDrone());
			Drone = Controller->GetSpawnedDrone();
			Director = Controller->GetMissionDirector();
			for (TActorIterator<ADroneTutorialHoverZone> It(World); It; ++It)
			{
				if (It->ActorHasTag(FName(TEXT("Tutorial.Hover.Zone"))))
				{
					Zone = *It;
					break;
				}
			}
			Test->TestNotNull(TEXT("Hover PIE owns the active Scout"), Drone.Get());
			Test->TestNotNull(TEXT("Hover PIE creates a Mission Director"), Director.Get());
			Test->TestNotNull(TEXT("Hover PIE finds the tagged Hover Zone"), Zone.Get());
			if (!Drone.IsValid() || !Director.IsValid() || !Zone.IsValid())
			{
				return true;
			}
			Test->TestTrue(TEXT("Hover lesson starts with HoverMaintained"),
				Director->GetCurrentObjective().Event == EDroneMissionObjectiveEvent::HoverMaintained);

			if (UPawnMovementComponent* Movement = Drone->GetMovementComponent())
			{
				Movement->StopMovementImmediately();
			}
			Drone->SetActorRotation(FRotator::ZeroRotator, ETeleportType::TeleportPhysics);
			Drone->SetActorLocation(Zone->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			Drone->GetCollisionComponent()->UpdateOverlaps();
			bLaunched = true;
			HoldStartedAt = Now;
			return false;
		}

		if (Zone.IsValid() && Zone->IsHoverCompleted())
		{
			Test->TestTrue(TEXT("Hover Zone overlaps the mission Scout during the hold"),
				Zone->GetHoverBox()->IsOverlappingActor(Drone.Get()));
			Test->TestTrue(TEXT("Three-second hover advances to ReturnToBase"),
				Director.IsValid()
				&& Director->GetCurrentObjective().Event == EDroneMissionObjectiveEvent::ReturnToBase);
			return true;
		}

		if (Now - HoldStartedAt > 6.0)
		{
			Test->AddError(FString::Printf(
				TEXT("Stable Hover did not complete within 6 seconds (overlap=%s hold=%.2f/%.2f stable=%s)"),
				Zone.IsValid() && Zone->GetHoverBox()->IsOverlappingActor(Drone.Get()) ? TEXT("true") : TEXT("false"),
				Zone.IsValid() ? Zone->GetCurrentHoldSeconds() : -1.0f,
				Zone.IsValid() ? Zone->GetRequiredHoldSeconds() : -1.0f,
				Zone.IsValid() && Zone->IsStableHoverCandidate(Drone.Get()) ? TEXT("true") : TEXT("false")));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
	double HoldStartedAt = 0.0;
	bool bLaunched = false;
	TWeakObjectPtr<ADronePrototypePawn> Drone;
	TWeakObjectPtr<ADroneMissionDirector> Director;
	TWeakObjectPtr<ADroneTutorialHoverZone> Zone;
};
} // namespace DroneTutorialHoverPIE

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTutorialHoverPIETest,
	"Drone.Tutorial.HoverMissionPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTutorialHoverPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneTutorialHoverPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress())
	{
		AddError(TEXT("Hover Mission PIE requires an idle Editor"));
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(FrontEndMapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != FrontEndMapPackage)
	{
		AddError(TEXT("Could not load the Front-end map for Hover Mission PIE"));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FStartHoverMissionCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FLaunchAndHoldHoverCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneTutorialDirectMapPIETest,
	"Drone.Tutorial.IndependentMapEntryPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDroneTutorialDirectMapPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneTutorialHoverPIE;
	if (!GEditor || GEditor->IsPlaySessionInProgress())
	{
		AddError(TEXT("Independent map PIE requires an idle Editor"));
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Drone/Maps/TestMap/Tutorial/Lvl_Tutorial_Hover_Test"));
	// 로비를 거치지 않는다. 배치된 Test Entry가 Drone Select를 준비해야 아래 실제 호버 검사가 가능하다.
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FLaunchAndHoldHoverCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
