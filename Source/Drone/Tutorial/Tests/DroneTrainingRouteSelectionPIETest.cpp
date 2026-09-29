#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "PlayInEditorDataTypes.h"
#include "Prototype/DronePrototypePlayerController.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"
#include "Tutorial/DroneTrainingRouteSelector.h"
#include "UI/DroneFlightHUDWidget.h"

namespace DroneTrainingRouteSelectionPIE
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneTrainingRouteSelectionTest");

UWorld* FindPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			return Context.World();
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

class FValidateRouteKeysCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateRouteKeysCommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
		FixedSelections = {
			{EKeys::Two, 2},
			{EKeys::Three, 3},
			{EKeys::Four, 4},
			{EKeys::One, 1},
		};
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}

		UWorld* World = FindPIEWorld();
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
		UEnhancedPlayerInput* PlayerInput = InputSubsystem ? InputSubsystem->GetPlayerInput() : nullptr;
		ADroneTrainingRouteSelector* Selector = nullptr;
		if (World)
		{
			for (TActorIterator<ADroneTrainingRouteSelector> It(World); It; ++It)
			{
				Selector = *It;
				break;
			}
		}

		if (!World || !World->HasBegunPlay() || !Controller || !PlayerInput || !Selector
			|| Selector->GetConfiguredRouteCount() != 4 || Selector->GetActiveRouteNumber() == 0)
		{
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Route selection PIE input stack was not ready within 20 seconds"));
				return true;
			}
			return false;
		}

		if (!bInitialStateChecked)
		{
			TestRouteState(*Selector, 1);
			bInitialStateChecked = true;
		}

		if (FixedSelectionIndex < FixedSelections.Num())
		{
			const FFixedSelection& Selection = FixedSelections[FixedSelectionIndex];
			if (!bWaitingForKeyResult)
			{
				PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(Selection.Key, IE_Pressed, 1.0f));
				bWaitingForKeyResult = true;
				return false;
			}

			TestRouteState(*Selector, Selection.ExpectedRouteNumber);
			PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(Selection.Key, IE_Released, 0.0f));
			++FixedSelectionIndex;
			bWaitingForKeyResult = false;
			return false;
		}

		if (!bRandomKeySent)
		{
			RouteBeforeRandom = Selector->GetActiveRouteNumber();
			PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Five, IE_Pressed, 1.0f));
			bRandomKeySent = true;
			return false;
		}

		const int32 RandomRoute = Selector->GetActiveRouteNumber();
		PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Five, IE_Released, 0.0f));
		Test->TestTrue(TEXT("Key 5 selects one of Routes 1-4"), RandomRoute >= 1 && RandomRoute <= 4);
		Test->TestTrue(TEXT("Key 5 visibly changes Route when four choices exist"), RandomRoute != RouteBeforeRandom);
		TestRouteState(*Selector, RandomRoute);
		return true;
	}

private:
	struct FFixedSelection
	{
		FKey Key;
		int32 ExpectedRouteNumber = 0;
	};

	void TestRouteState(ADroneTrainingRouteSelector& Selector, const int32 ExpectedRouteNumber) const
	{
		Test->TestEqual(TEXT("Pressed number selects the expected Route"),
			Selector.GetActiveRouteNumber(), ExpectedRouteNumber);
		int32 ActiveCount = 0;
		for (int32 RouteNumber = 1; RouteNumber <= 4; ++RouteNumber)
		{
			ActiveCount += Selector.IsRouteActive(RouteNumber) ? 1 : 0;
		}
		Test->TestEqual(TEXT("Exactly one Route is active after keyboard selection"), ActiveCount, 1);

		const ADronePrototypePlayerController* DroneController = Cast<ADronePrototypePlayerController>(
			Selector.GetWorld() ? Selector.GetWorld()->GetFirstPlayerController() : nullptr);
		const UDroneFlightHUDWidget* HUD = DroneController ? DroneController->GetFlightHUDWidget() : nullptr;
		const ADroneTrainingCourse* ActiveCourse = Selector.GetActiveRoute();
		Test->TestTrue(TEXT("HUD follows the active Route recorder"),
			HUD && ActiveCourse && HUD->GetTrainingRecordSource() == ActiveCourse->GetLapRecorderComponent());
	}

	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
	TArray<FFixedSelection> FixedSelections;
	int32 FixedSelectionIndex = 0;
	int32 RouteBeforeRandom = 0;
	bool bInitialStateChecked = false;
	bool bWaitingForKeyResult = false;
	bool bRandomKeySent = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTrainingRouteSelectionPIETest,
	"Drone.Tutorial.TrainingRouteSelectionPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingRouteSelectionPIETest::RunTest(const FString& Parameters)
{
	using namespace DroneTrainingRouteSelectionPIE;
	if (!GEditor)
	{
		AddError(TEXT("GEditor is unavailable"));
		return false;
	}
	if (GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Route selection PIE requires no pre-existing PIE session"));
		return false;
	}

	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapPackage));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(MakePlayParams()));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateRouteKeysCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

