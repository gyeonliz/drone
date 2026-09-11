#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Prototype/DronePrototypePawn.h"
#include "Prototype/DronePrototypePlayerController.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingGate.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/DroneFlightHUDWidget.h"

namespace DroneTrainingPIESmoke
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Drone/Maps/Lvl_DroneTraining");
constexpr const TCHAR* CourseClassPath =
	TEXT("/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingCourse.BP_DroneTrainingCourse_C");
constexpr const TCHAR* GateClassPath =
	TEXT("/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingGate.BP_DroneTrainingGate_C");
constexpr const TCHAR* PawnClassPath =
	TEXT("/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration.BP_DroneFPVIntegration_C");
constexpr const TCHAR* ControllerClassPath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypePlayerController.BP_DronePrototypePlayerController_C");
constexpr const TCHAR* FlightHUDClassPath =
	TEXT("/Game/Drone/Prototype/UI/WBP_DroneFlightHUD.WBP_DroneFlightHUD_C");

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
	// 실제 BP GameMode/Pawn/Controller/WBP 경로를 새 1인 PIE World에서 검증한다.
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(1);
	Settings->bLaunchSeparateServer = false;
	Settings->AddToRoot(); // FStartPIEForAutomationCommand가 시작 뒤 Root에서 제거한다.

	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.EditorPlaySettings = Settings;
	Params.bAllowOnlineSubsystem = false;
	return Params;
}

class FValidateTrainingPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateTrainingPIECommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt == 0.0)
		{
			StartedAt = Now;
		}

		UWorld* PIEWorld = FindPIEWorld();
		if (!PIEWorld || !PIEWorld->HasBegunPlay())
		{
			if (Now - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Training PIE World did not begin play within 20 seconds"));
				return true;
			}
			return false;
		}

		UClass* CourseClass = LoadClass<ADroneTrainingCourse>(nullptr, CourseClassPath);
		UClass* GateClass = LoadClass<ADroneTrainingGate>(nullptr, GateClassPath);
		UClass* PawnClass = LoadClass<ADronePrototypePawn>(nullptr, PawnClassPath);
		UClass* ControllerClass = LoadClass<ADronePrototypePlayerController>(nullptr, ControllerClassPath);
		UClass* FlightHUDClass = LoadClass<UDroneFlightHUDWidget>(nullptr, FlightHUDClassPath);
		Test->TestNotNull(TEXT("Expected BP_DroneTrainingCourse Class loads"), CourseClass);
		Test->TestNotNull(TEXT("Expected BP_DroneTrainingGate Class loads"), GateClass);
		Test->TestNotNull(TEXT("Expected BP_DroneFPVIntegration Class loads"), PawnClass);
		Test->TestNotNull(TEXT("Expected BP_DronePrototypePlayerController Class loads"), ControllerClass);
		Test->TestNotNull(TEXT("Expected WBP_DroneFlightHUD Class loads"), FlightHUDClass);

		ADronePrototypePlayerController* Controller = Cast<ADronePrototypePlayerController>(
			PIEWorld->GetFirstPlayerController());
		Test->TestNotNull(TEXT("Training PIE spawns the Prototype PlayerController"), Controller);
		if (Controller && ControllerClass)
		{
			Test->TestTrue(TEXT("Training PIE uses the BP Prototype Controller"), Controller->GetClass() == ControllerClass);
		}

		ADronePrototypePawn* Drone = Controller ? Cast<ADronePrototypePawn>(Controller->GetPawn()) : nullptr;
		Test->TestNotNull(TEXT("Training PIE spawns and possesses the Prototype Drone"), Drone);
		if (Drone && PawnClass)
		{
			Test->TestTrue(TEXT("Training PIE uses BP_DroneFPVIntegration"), Drone->GetClass() == PawnClass);
		}

		UDroneFlightHUDWidget* FlightHUD = Controller ? Controller->GetFlightHUDWidget() : nullptr;
		Test->TestNotNull(TEXT("Training PIE creates the Flight HUD"), FlightHUD);
		if (FlightHUD && FlightHUDClass)
		{
			Test->TestTrue(TEXT("Training PIE uses WBP_DroneFlightHUD"), FlightHUD->GetClass() == FlightHUDClass);
			Test->TestFalse(TEXT("Training PIE does not use the native fallback HUD layout"), FlightHUD->IsUsingNativeFallbackLayout());
		}

		ADroneTrainingCourse* Course = nullptr;
		int32 CourseCount = 0;
		for (TActorIterator<ADroneTrainingCourse> CourseIt(PIEWorld); CourseIt; ++CourseIt)
		{
			Course = *CourseIt;
			++CourseCount;
		}
		Test->TestEqual(TEXT("Training PIE has exactly one Course"), CourseCount, 1);
		Test->TestNotNull(TEXT("Training PIE contains the Course"), Course);

		TArray<ADroneTrainingGate*> PlacedGates;
		for (TActorIterator<ADroneTrainingGate> GateIt(PIEWorld); GateIt; ++GateIt)
		{
			ADroneTrainingGate* Gate = *GateIt;
			if (Gate)
			{
				PlacedGates.Add(Gate);
				if (GateClass)
				{
					Test->TestTrue(TEXT("Training PIE Gate uses BP_DroneTrainingGate"), Gate->GetClass() == GateClass);
				}
			}
		}
		Test->TestTrue(TEXT("Training PIE has at least a start and finish Gate"), PlacedGates.Num() >= 2);

		int32 RecastNavMeshCount = 0;
		for (TActorIterator<AActor> ActorIt(PIEWorld); ActorIt; ++ActorIt)
		{
			const AActor* Actor = *ActorIt;
			if (Actor && Actor->GetClass()->GetPathName() == TEXT("/Script/NavigationSystem.RecastNavMesh"))
			{
				++RecastNavMeshCount;
			}
		}
		Test->TestTrue(
			TEXT("Training PIE contains a saved RecastNavMesh Actor"),
			RecastNavMeshCount > 0);

		if (Course)
		{
			if (CourseClass)
			{
				Test->TestTrue(TEXT("Training PIE uses BP_DroneTrainingCourse"), Course->GetClass() == CourseClass);
			}

			const USplineComponent* CourseSpline = Course->GetCourseSpline();
			Test->TestNotNull(TEXT("Runtime Course owns its Spline"), CourseSpline);
			const int32 ExpectedSegmentCount = Course->GetExpectedCourseLineSegmentCount();
			Test->TestEqual(
				TEXT("Runtime Course displays every distance-sampled Spline segment"),
				Course->GetCourseLineSegmentCount(),
				ExpectedSegmentCount);

			TInlineComponentArray<UPrimitiveComponent*> CoursePrimitives;
			Course->GetComponents(CoursePrimitives);
			for (UPrimitiveComponent* Primitive : CoursePrimitives)
			{
				if (!Primitive)
				{
					continue;
				}

				Test->TestEqual(
					*FString::Printf(TEXT("PIE component %s has no collision"), *Primitive->GetName()),
					Primitive->GetCollisionEnabled(),
					ECollisionEnabled::NoCollision);
				Test->TestFalse(
					*FString::Printf(TEXT("PIE component %s creates no overlaps"), *Primitive->GetName()),
					Primitive->GetGenerateOverlapEvents());
				Test->TestFalse(
					*FString::Printf(TEXT("PIE component %s cannot affect Navigation"), *Primitive->GetName()),
					Primitive->CanEverAffectNavigation());
			}

			// 실제 PIE Pawn을 안내선 가로 방향으로 Sweep해 표시 Mesh가 이동을 막지 않는지 확인한다.
			if (Drone && CourseSpline)
			{
				const float ProbeDistance = CourseSpline->GetSplineLength() * 0.12f;
				const FVector ProbeCenter = CourseSpline->GetLocationAtDistanceAlongSpline(
					ProbeDistance,
					ESplineCoordinateSpace::World);
				const FVector ProbeDirection = CourseSpline->GetDirectionAtDistanceAlongSpline(
					ProbeDistance,
					ESplineCoordinateSpace::World);
				FVector CrossDirection = FVector::CrossProduct(FVector::UpVector, ProbeDirection).GetSafeNormal();
				if (CrossDirection.IsNearlyZero())
				{
					CrossDirection = FVector::RightVector;
				}

				const FVector SweepStart = ProbeCenter - CrossDirection * 250.0f;
				const FVector SweepEnd = ProbeCenter + CrossDirection * 250.0f;
				Drone->SetActorLocation(SweepStart, false);
				FHitResult SweepHit;
				Drone->SetActorLocation(SweepEnd, true, &SweepHit);
				Test->TestFalse(TEXT("Training PIE Drone is not blocked by the guide line"), SweepHit.bBlockingHit);
				Test->TestTrue(
					TEXT("Training PIE Drone reaches the other side of the guide line"),
					Drone->GetActorLocation().Equals(SweepEnd, 1.0f));
				Test->TestTrue(TEXT("Training PIE guide line is never a Hit Actor"), SweepHit.GetActor() != Course);
			}

			// 실제 BP Gate 목록에서 순서·방향·중복 판정과 Visual 상태 전환을 함께 검사한다.
			UDroneTrainingGateSequenceComponent* GateSequence = Course->GetGateSequenceComponent();
			UDroneTrainingLapRecorderComponent* LapRecorder = Course->GetLapRecorderComponent();
			Test->TestNotNull(TEXT("Training PIE Course owns the Gate Sequence"), GateSequence);
			Test->TestNotNull(TEXT("Training PIE Course owns the Lap Recorder"), LapRecorder);
			if (LapRecorder)
			{
				Test->TestTrue(
					TEXT("Training PIE Flight HUD is connected to the Course Lap Recorder"),
					FlightHUD && FlightHUD->GetTrainingRecordSource() == LapRecorder);
				Test->TestTrue(TEXT("Training PIE Lap Recorder is ready"), LapRecorder->IsRecordingReady());
				Test->TestFalse(TEXT("Training PIE has not started a Lap yet"), LapRecorder->IsLapRecording());
			}
			if (GateSequence)
			{
				Test->TestTrue(TEXT("Training PIE Gate configuration is valid"), GateSequence->IsConfigurationValid());
				Test->TestEqual(
					TEXT("Training PIE Gate Sequence contains every spawned Gate"),
					GateSequence->GetConfiguredGateCount(),
					PlacedGates.Num());
				Test->TestEqual(TEXT("Training PIE initially expects Gate 0"), GateSequence->GetCurrentGateIndex(), 0);

				const TArray<TObjectPtr<ADroneTrainingGate>>& OrderedGates = GateSequence->GetOrderedGates();
				if (Drone && OrderedGates.Num() >= 4)
				{
					auto Entry = [](const ADroneTrainingGate* Gate)
					{
						return Gate->GetActorLocation() - Gate->GetForwardDirectionWorld() * 300.0f;
					};
					auto Exit = [](const ADroneTrainingGate* Gate)
					{
						return Gate->GetActorLocation() + Gate->GetForwardDirectionWorld() * 300.0f;
					};

					ADroneTrainingGate* Gate0 = OrderedGates[0];
					ADroneTrainingGate* Gate1 = OrderedGates[1];
					ADroneTrainingGate* Gate2 = OrderedGates[2];
					ADroneTrainingGate* Gate3 = OrderedGates[3];
					Test->TestNotNull(TEXT("Training PIE Gate 0 exists"), Gate0);
					Test->TestNotNull(TEXT("Training PIE Gate 1 exists"), Gate1);
					Test->TestNotNull(TEXT("Training PIE Gate 2 exists"), Gate2);
					Test->TestNotNull(TEXT("Training PIE Gate 3 exists"), Gate3);

					if (Gate0 && Gate1 && Gate2 && Gate3)
					{
						Test->TestEqual(
							TEXT("Training PIE rejects a future Gate"),
							GateSequence->TryAcceptTraversal(Gate1, Drone, Entry(Gate1), Exit(Gate1)),
							EDroneTrainingGatePassResult::WrongOrder);
						Test->TestEqual(
							TEXT("Training PIE rejects reverse traversal"),
							GateSequence->TryAcceptTraversal(Gate0, Drone, Exit(Gate0), Entry(Gate0)),
							EDroneTrainingGatePassResult::WrongDirection);
						Test->TestEqual(
							TEXT("Training PIE accepts current Gate forward"),
							GateSequence->TryAcceptTraversal(Gate0, Drone, Entry(Gate0), Exit(Gate0)),
							EDroneTrainingGatePassResult::Accepted);
						if (LapRecorder)
						{
							Test->TestTrue(TEXT("Gate 0 starts the Training Lap"), LapRecorder->IsLapRecording());
							Test->TestEqual(TEXT("Gate 0 creates no Segment"), LapRecorder->GetRecordedSegmentCount(), 0);
						}
						Test->TestEqual(
							TEXT("Training PIE rejects duplicate completed Gate"),
							GateSequence->TryAcceptTraversal(Gate0, Drone, Entry(Gate0), Exit(Gate0)),
							EDroneTrainingGatePassResult::AlreadyCompleted);
						Test->TestEqual(
							TEXT("Training PIE accepts next Gate forward"),
							GateSequence->TryAcceptTraversal(Gate1, Drone, Entry(Gate1), Exit(Gate1)),
							EDroneTrainingGatePassResult::Accepted);
						if (LapRecorder)
						{
							// 이 직접 호출 두 번은 같은 PIE Frame이므로 0초 기록을 성공 Segment로 남기지 않는다.
							Test->TestFalse(TEXT("Same-frame traversal is not retained as a timed Lap"), LapRecorder->IsLapRecording());
							Test->TestEqual(TEXT("Same-frame traversal creates no timed Segment"), LapRecorder->GetRecordedSegmentCount(), 0);
						}
						Test->TestEqual(TEXT("Training PIE advances exactly two Gates"), GateSequence->GetAcceptedGateCount(), 2);
						Test->TestEqual(TEXT("Gate 0 visual is Completed"), Gate0->GetGateVisualState(), EDroneTrainingGateVisualState::Completed);
						Test->TestEqual(TEXT("Gate 1 visual is Completed"), Gate1->GetGateVisualState(), EDroneTrainingGateVisualState::Completed);
						Test->TestEqual(TEXT("Gate 2 visual is Current"), Gate2->GetGateVisualState(), EDroneTrainingGateVisualState::Current);
						Test->TestEqual(TEXT("Gate 3 visual remains Inactive"), Gate3->GetGateVisualState(), EDroneTrainingGateVisualState::Inactive);
						GateSequence->ResetSequence();
						if (LapRecorder)
						{
							Test->TestFalse(TEXT("Sequence Reset discards the partial Lap"), LapRecorder->IsLapRecording());
							Test->TestEqual(TEXT("Partial PIE Lap is not successful history"), LapRecorder->GetSuccessfulLapCount(), 0);
						}
					}
				}
			}
		}

		return true;
	}

private:
	FAutomationTestBase* Test;
	double StartedAt = 0.0;
};

/** 실제 저장된 BP Gate의 Delegate와 Begin/End Overlap까지 PIE Frame 사이에서 통합 검증한다. */
class FValidateTrainingPIEActualGateOverlapCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateTrainingPIEActualGateOverlapCommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = FindPIEWorld();
		ADronePrototypePlayerController* Controller = PIEWorld
			? Cast<ADronePrototypePlayerController>(PIEWorld->GetFirstPlayerController())
			: nullptr;
		ADronePrototypePawn* Drone = Controller ? Cast<ADronePrototypePawn>(Controller->GetPawn()) : nullptr;
		ADroneTrainingCourse* Course = nullptr;
		if (PIEWorld)
		{
			for (TActorIterator<ADroneTrainingCourse> CourseIt(PIEWorld); CourseIt; ++CourseIt)
			{
				Course = *CourseIt;
				break;
			}
		}

		UDroneTrainingGateSequenceComponent* Sequence = Course ? Course->GetGateSequenceComponent() : nullptr;
		UDroneTrainingLapRecorderComponent* LapRecorder = Course ? Course->GetLapRecorderComponent() : nullptr;
		const TArray<TObjectPtr<ADroneTrainingGate>>* OrderedGates = Sequence
			? &Sequence->GetOrderedGates()
			: nullptr;
		ADroneTrainingGate* Gate0 = OrderedGates && OrderedGates->IsValidIndex(0)
			? (*OrderedGates)[0].Get()
			: nullptr;
		if (!PIEWorld || !Drone || !Sequence || !LapRecorder || !Gate0)
		{
			Test->AddError(TEXT("Actual BP Gate overlap validation could not resolve PIE Drone, Sequence, or Gate 0"));
			return true;
		}

		const FVector Forward = Gate0->GetForwardDirectionWorld();
		if (Phase == 0)
		{
			Sequence->ResetSequence();
			Drone->SetActorLocation(Gate0->GetActorLocation() - Forward * 300.0f, false);
			Phase = 1;
			return false;
		}

		if (Phase == 1)
		{
			Drone->SetActorLocation(Gate0->GetActorLocation() - Forward * 30.0f, false);
			Phase = 2;
			return false;
		}

		if (Phase == 2)
		{
			FHitResult SweepHit;
			Drone->SetActorLocation(Gate0->GetActorLocation() + Forward * 300.0f, true, &SweepHit);
			Test->TestFalse(TEXT("Actual BP Gate Trigger and Ring do not block the PIE Drone"), SweepHit.bBlockingHit);
			Phase = 3;
			return false;
		}

		Test->TestEqual(TEXT("Actual BP Gate Begin/End Overlap advances once"), Sequence->GetAcceptedGateCount(), 1);
		Test->TestEqual(TEXT("Actual BP Gate 0 becomes Completed"), Gate0->GetGateVisualState(), EDroneTrainingGateVisualState::Completed);
		Test->TestEqual(TEXT("Actual BP Gate 1 becomes Current"), Sequence->GetCurrentGateIndex(), 1);
		Test->TestTrue(TEXT("Actual BP Gate 0 overlap starts Lap recording"), LapRecorder->IsLapRecording());
		Test->TestEqual(TEXT("Actual BP Gate 0 overlap still has zero Segments"), LapRecorder->GetRecordedSegmentCount(), 0);
		return true;
	}

private:
	FAutomationTestBase* Test;
	int32 Phase = 0;
};
} // namespace DroneTrainingPIESmoke

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTrainingPIESmokeTest,
	"Drone.Tutorial.TrainingPIESmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingPIESmokeTest::RunTest(const FString& Parameters)
{
	using namespace DroneTrainingPIESmoke;

	// Commandlet은 Editor Map load 또는 in-process PIE에서 저장된 Recast Actor를 등록하기 직전
	// CrowdManager를 먼저 만들 수 있다. 전체 Suite의 선행 Map에 따라 경고 횟수가 달라지므로 1회 이상을 허용한다.
	// 검증 본문은 저장된 Recast Actor의 존재와 Course Component의 Navigation 비간섭 Flag를 별도로 검사한다.
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains,
		0);

	if (!GEditor)
	{
		AddError(TEXT("GEditor is unavailable"));
		return false;
	}

	if (GEditor->IsPlaySessionInProgress() || FindPIEWorld())
	{
		AddError(TEXT("Training PIE smoke test requires no pre-existing PIE session"));
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
	ADD_LATENT_AUTOMATION_COMMAND(FValidateTrainingPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateTrainingPIEActualGateOverlapCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
