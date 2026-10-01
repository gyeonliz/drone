#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Opt-in rendered diagnostic: baseline currently fails for the known lobby reflow.
// Wrap probes change transient PIE widgets only; they are not a production fix.
#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/DroneFrontEndPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFrontEndRootWidget.h"

namespace DroneLobbyLayoutDiagnostic
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneFrontEnd");
constexpr const TCHAR* WidgetClassPath = TEXT("/Game/Drone/FrontEnd/UI/WBP_DroneFrontEndRoot.WBP_DroneFrontEndRoot_C");
constexpr int32 FramesPerPhase = 6;
constexpr double AllowedMovement = 2.0; // Pixels in the native 1920x1080 design, independent of viewport DPI.

const TArray<FName> SampleNames = {
	TEXT("MissionWorkspace"), TEXT("MissionListPanel"), TEXT("MissionCardPanel"), TEXT("MissionDetailPanel"),
	TEXT("MissionNameText"), TEXT("MissionMetaText"), TEXT("MissionDescriptionText"), TEXT("MissionObjectiveSummaryText"),
	TEXT("MissionButtonsColumn"), TEXT("MissionSelectButtonText_0"), TEXT("StartMissionButton")
};
const TArray<FName> ColumnNames = {TEXT("MissionListPanel"), TEXT("MissionCardPanel"), TEXT("MissionDetailPanel")};

struct FBounds
{
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;

	double Difference(const FBounds& Other) const
	{
		return FMath::Max(FMath::Max(FMath::Abs(Position.X - Other.Position.X), FMath::Abs(Position.Y - Other.Position.Y)),
			FMath::Max(FMath::Abs(Size.X - Other.Size.X), FMath::Abs(Size.Y - Other.Size.Y)));
	}
};

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

class FProbeLobbyGeometryCommand final : public IAutomationLatentCommand
{
public:
	explicit FProbeLobbyGeometryCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual ~FProbeLobbyGeometryCommand() override
	{
		if (PostTickHandle.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().OnPostTick().Remove(PostTickHandle);
		}
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0) StartedAt = Now;
		if (!Widget.IsValid())
		{
			UWorld* World = FindPIEWorld();
			ADroneFrontEndPlayerController* Controller = World && World->HasBegunPlay()
				? World->GetFirstPlayerController<ADroneFrontEndPlayerController>() : nullptr;
			UDroneFrontEndRootWidget* Root = Controller ? Controller->GetFrontEndWidget() : nullptr;
			if (!Root || !Root->IsInViewport())
			{
				if (Now - StartedAt < 20.0) return false;
				Test->AddError(TEXT("[DEBUG-lobby-layout] FrontEnd PIE Root did not become ready within 20 seconds"));
				return true;
			}
			if (!FSlateApplication::IsInitialized() || !Root->WidgetTree || !Root->IsUsingNativeFallbackLayout()
				|| Root->GetClass()->GetPathName() != WidgetClassPath)
			{
				Test->AddError(TEXT("[DEBUG-lobby-layout] Probe requires the actual native-fallback FrontEnd BP Root and Slate"));
				return true;
			}
			Widget = Root;
			UWidget* Workspace = Root->WidgetTree->FindWidget(TEXT("MissionWorkspace"));
			for (UWidget* Ancestor = Workspace ? Workspace->GetParent() : nullptr; Ancestor; Ancestor = Ancestor->GetParent())
			{
				if (Cast<USizeBox>(Ancestor)) { DesignCanvas = Ancestor; break; }
			}
			if (!DesignCanvas.IsValid())
			{
				Test->AddError(TEXT("[DEBUG-lobby-layout] Native workspace has no design SizeBox for coordinate normalization"));
				return true;
			}
			for (const FName Name : SampleNames)
			{
				Test->AddInfo(FString::Printf(TEXT("[DEBUG-lobby-layout] sample-target=%s"), *Name.ToString()));
			}
			PostTickHandle = FSlateApplication::Get().OnPostTick().AddRaw(this, &FProbeLobbyGeometryCommand::OnSlatePostTick);
			if (FParse::Param(FCommandLine::Get(), TEXT("LobbyLayoutHoverOnly")))
			{
				if (!Root->OpenTrainingLobby()) return RejectAction(TEXT("EnterTraining"));
				TutorialMissionIds = {FName(TEXT("Mission.Tutorial.Hover"))};
				BeginPhase(TEXT("EnterTraining"));
			}
			else
			{
				if (!Root->FinishOpeningTrailer()) return RejectAction(TEXT("EnterStory"));
				BeginPhase(TEXT("EnterStory"));
			}
			return false;
		}
		if (bWaitingForTitle)
		{
			if (TitleFrames < 2) return false;
			bWaitingForTitle = false;
			if (!Widget->OpenTrainingLobby()) return RejectAction(TEXT("EnterTraining"));
			TutorialMissionIds = Widget->GetVisibleMissionIds();
			if (TutorialMissionIds.IsEmpty()) return RejectAction(TEXT("Training has no visible Tutorial missions"));
			BeginPhase(TEXT("EnterTraining"));
			return false;
		}
		if (!bPhaseComplete)
		{
			if (Now - PhaseStartedAt < 10.0) return false;
			Test->AddError(FString::Printf(TEXT("[DEBUG-lobby-layout] phase=%s could not sample six arranged visible Slate frames; samples=%d. An unpainted viewport cannot establish this diagnostic verdict."), *PhaseName, Samples));
			return true;
		}
		if (PhaseName == TEXT("EnterStory"))
		{
			if (!Widget->NavigateBack()) return RejectAction(TEXT("StoryBackToTitle"));
			bWaitingForTitle = true;
			TitleFrames = 0;
			return false;
		}
		if (PhaseName == TEXT("EnterTraining")) TrainingColumnBounds = LastBounds;
		if (TutorialIndex >= TutorialMissionIds.Num())
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("LobbyLayoutWrapProbe")) && ExperimentPolicy < 2)
			{
				++ExperimentPolicy;
				if (!Widget->SelectLobbyMission(FName(TEXT("Mission.Tutorial.Hover")))) return RejectAction(TEXT("WrapProbeSelectHover"));
				BeginPhase(ExperimentPolicy == 1 ? TEXT("ProbeExplicitWrap") : TEXT("ProbeExplicitWrapReservedScrollbar"));
				return false;
			}
			return true;
		}
		const FName MissionId = TutorialMissionIds[TutorialIndex++];
		if (!Widget->SelectLobbyMission(MissionId)) return RejectAction(*MissionId.ToString());
		BeginPhase(FString::Printf(TEXT("SelectTutorial:%s"), *MissionId.ToString()));
		return false;
	}

private:
	bool RejectAction(const TCHAR* Action)
	{
		Test->AddError(FString::Printf(TEXT("[DEBUG-lobby-layout] Actual Root action failed: %s"), Action));
		return true;
	}

	void BeginPhase(const FString& InPhaseName)
	{
		PhaseName = InPhaseName;
		PhaseStartedAt = FPlatformTime::Seconds();
		Samples = 0;
		MaxMovement = 0.0;
		WorstWidget = NAME_None;
		FirstBounds.Reset();
		LastBounds.Reset();
		bPhaseComplete = false;
		FreshFirstLabel = Widget->WidgetTree->FindWidget(TEXT("MissionSelectButtonText_0"));
		ActiveSampleNames = SampleNames;
		const int32 VisibleCount = Widget->GetVisibleMissionIds().Num();
		for (int32 Index = 1; Index < VisibleCount; ++Index)
		{
			ActiveSampleNames.Add(FName(*FString::Printf(TEXT("MissionSelectButtonText_%d"), Index)));
		}
		// Controlled diagnostic only: alter the freshly rebuilt transient PIE labels, never production defaults/assets.
		if (ExperimentPolicy > 0)
		{
			for (const FName Name : ActiveSampleNames)
			{
				if (!Name.ToString().StartsWith(TEXT("MissionSelectButtonText_"))) continue;
				if (UTextBlock* Label = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(Name)))
				{
					Label->SetAutoWrapText(false);
					Label->SetWrapTextAt(160.0f);
				}
			}
			if (ExperimentPolicy == 2)
			{
				if (UWidget* List = Widget->WidgetTree->FindWidget(TEXT("MissionButtonsColumn")))
					if (UScrollBox* Scroll = Cast<UScrollBox>(List->GetParent())) Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
			}
		}
		Test->AddInfo(FString::Printf(TEXT("[DEBUG-lobby-layout] phase=%s begin threshold=%.1f design-px"), *PhaseName, AllowedMovement));
	}

	void OnSlatePostTick(float DeltaSeconds)
	{
		if (LastEngineFrame == GFrameCounter) return;
		LastEngineFrame = GFrameCounter;
		if (bWaitingForTitle) { ++TitleFrames; return; }
		if (bPhaseComplete || !Widget.IsValid() || !DesignCanvas.IsValid() || !FreshFirstLabel.IsValid()) return;
		if (Widget->GetDisplayedState() != EDroneGameFlowState::LobbyMissionSelect) return;
		const FGeometry& Reference = DesignCanvas->GetCachedGeometry();
		const FVector2D ReferenceSize = Reference.GetLocalSize();
		const FVector2D FreshLabelSize = FreshFirstLabel->GetCachedGeometry().GetLocalSize();
		// Each actual lobby refresh replaces this label. Its nonzero cached geometry proves the changed tree was arranged.
		if (ReferenceSize.X <= 0.0 || ReferenceSize.Y <= 0.0 || FreshLabelSize.X <= 0.0 || FreshLabelSize.Y <= 0.0) return;
		TMap<FName, FBounds> Current;
		for (const FName Name : ActiveSampleNames)
		{
			UWidget* Target = Widget->WidgetTree->FindWidget(Name);
			if (!Target)
			{
				Test->AddError(FString::Printf(TEXT("[DEBUG-lobby-layout] phase=%s missing sample widget=%s"), *PhaseName, *Name.ToString()));
				bPhaseComplete = true;
				return;
			}
			const FGeometry& Geometry = Target->GetCachedGeometry();
			const FVector2D AbsoluteTopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D AbsoluteBottomRight = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
			FBounds Bounds;
			Bounds.Position = Reference.AbsoluteToLocal(AbsoluteTopLeft);
			Bounds.Size = FVector2D(Reference.AbsoluteToLocal(AbsoluteBottomRight)) - Bounds.Position;
			if (ColumnNames.Contains(Name) && (Bounds.Size.X <= 0.0 || Bounds.Size.Y <= 0.0)) return;
			Current.Add(Name, Bounds);
		}
		++Samples;
		if (Samples == 1) FirstBounds = Current;
		for (const FName Name : ActiveSampleNames)
		{
			const FBounds& Bounds = Current.FindChecked(Name);
			const double Movement = Bounds.Difference(FirstBounds.FindChecked(Name));
			if (Movement > MaxMovement) { MaxMovement = Movement; WorstWidget = Name; }
			// First/second/third presented frames are retained, plus the final settling frame.
			if (Samples <= 3 || Samples == FramesPerPhase)
			{
				Test->AddInfo(FString::Printf(TEXT("[DEBUG-lobby-layout] phase=%s frame=%llu sample=%d widget=%s x=%.3f y=%.3f w=%.3f h=%.3f delta-from-first=%.3f"),
					*PhaseName, static_cast<unsigned long long>(GFrameCounter), Samples, *Name.ToString(), Bounds.Position.X, Bounds.Position.Y, Bounds.Size.X, Bounds.Size.Y, Movement));
			}
		}
		LastBounds = Current;
		if (Samples < FramesPerPhase) return;
		Test->AddInfo(FString::Printf(TEXT("[DEBUG-lobby-layout] phase=%s max-within-phase=%.3f worst-widget=%s"), *PhaseName, MaxMovement, *WorstWidget.ToString()));
		if (MaxMovement > AllowedMovement)
		{
			Test->AddError(FString::Printf(TEXT("[DEBUG-lobby-layout] phase=%s visible geometry jumped %.3f design-px after the first arranged frame; widget=%s threshold=%.1f"), *PhaseName, MaxMovement, *WorstWidget.ToString(), AllowedMovement));
		}
		if (PhaseName.StartsWith(TEXT("SelectTutorial:")))
		{
			double MaxColumnMovement = 0.0;
			for (const FName Name : ColumnNames)
			{
				const FBounds* Baseline = TrainingColumnBounds.Find(Name);
				if (!Baseline) continue;
				const double Movement = LastBounds.FindChecked(Name).Difference(*Baseline);
				MaxColumnMovement = FMath::Max(MaxColumnMovement, Movement);
				if (Movement > AllowedMovement)
				{
					Test->AddError(FString::Printf(TEXT("[DEBUG-lobby-layout] phase=%s column=%s moved/resized %.3f design-px compared with the settled Training list; threshold=%.1f"), *PhaseName, *Name.ToString(), Movement, AllowedMovement));
				}
			}
			Test->AddInfo(FString::Printf(TEXT("[DEBUG-lobby-layout] phase=%s max-between-selections-columns=%.3f"), *PhaseName, MaxColumnMovement));
		}
		bPhaseComplete = true;
	}

	FAutomationTestBase* Test;
	TWeakObjectPtr<UDroneFrontEndRootWidget> Widget;
	TWeakObjectPtr<UWidget> DesignCanvas;
	TWeakObjectPtr<UWidget> FreshFirstLabel;
	FDelegateHandle PostTickHandle;
	TArray<FName> TutorialMissionIds;
	TArray<FName> ActiveSampleNames;
	TMap<FName, FBounds> FirstBounds;
	TMap<FName, FBounds> LastBounds;
	TMap<FName, FBounds> TrainingColumnBounds;
	FString PhaseName;
	FName WorstWidget;
	double StartedAt = 0.0;
	double PhaseStartedAt = 0.0;
	double MaxMovement = 0.0;
	uint64 LastEngineFrame = MAX_uint64;
	int32 Samples = 0;
	int32 TutorialIndex = 0;
	int32 ExperimentPolicy = 0;
	int32 TitleFrames = 0;
	bool bPhaseComplete = false;
	bool bWaitingForTitle = false;
};

class FReleasePlaySettingsCommand final : public IAutomationLatentCommand
{
public:
	explicit FReleasePlaySettingsCommand(ULevelEditorPlaySettings* InSettings) : Settings(InSettings) {}
	virtual bool Update() override
	{
		if (Settings.IsValid()) Settings->RemoveFromRoot();
		return true;
	}
private:
	TWeakObjectPtr<ULevelEditorPlaySettings> Settings;
};
} // namespace DroneLobbyLayoutDiagnostic

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneLobbyLayoutStabilityDiagnosticTest,
	"Drone.Flow.Diagnostic.LobbyLayoutStabilityPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneLobbyLayoutStabilityDiagnosticTest::RunTest(const FString& Parameters)
{
	using namespace DroneLobbyLayoutDiagnostic;
	if (!GEditor || GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("[DEBUG-lobby-layout] Geometry diagnostic requires an idle Editor"));
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(MapPackage);
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World || World->GetOutermost()->GetName() != MapPackage)
	{
		AddError(FString::Printf(TEXT("[DEBUG-lobby-layout] Could not open %s"), MapPackage));
		return false;
	}
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(1);
	Settings->bLaunchSeparateServer = false;
	Settings->NewWindowWidth = 1280;
	Settings->NewWindowHeight = 720;
	Settings->AddToRoot();
	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.EditorPlaySettings = Settings;
	Params.bAllowOnlineSubsystem = false;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(Params));
	ADD_LATENT_AUTOMATION_COMMAND(FProbeLobbyGeometryCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FReleasePlaySettingsCommand(Settings));
	return true;
}

#endif
