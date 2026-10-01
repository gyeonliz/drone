#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Mission/DroneMissionDefinition.h"
#include "Mission/DroneMissionDirector.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationCommon.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingGate.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneCircularCourseMissionTest, "Drone.Mission.CircularCourse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneCircularCourseMissionTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game)) return false;
	UWorld* World = Wrapper.GetTestWorld();
	UGameInstance* Instance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* Flow = NewObject<UDroneGameFlowSubsystem>(Instance);
	if (!TestTrue(TEXT("Catalog loads"), Flow->EnsureDefaultCatalog())) return false;
	UDroneMissionDefinition* Mission = Flow->FindMissionDefinition(FName(TEXT("Mission.Tutorial.Heading")));
	if (!TestNotNull(TEXT("Existing Heading ID now resolves the Orbit lesson"), Mission)) return false;
	TestEqual(TEXT("Rotation lesson uses Lap, not HeadingAligned"), Mission->ObjectiveRules[0].Event,
		EDroneMissionObjectiveEvent::TrainingLap);
	TestTrue(TEXT("Opening"), Flow->BeginOpeningTrailer());
	TestTrue(TEXT("Lobby"), Flow->EnterLobbyFromOpeningTrailer());
	TestTrue(TEXT("Select Orbit"), Flow->SelectMission(Mission->MissionId));
	TestTrue(TEXT("Briefing"), Flow->ConfirmMissionSelection());
	TestTrue(TEXT("Loading"), Flow->NotifyMissionTrailerFinished());
	TestTrue(TEXT("Drone selection"), Flow->NotifyMissionMapReady());
	TestTrue(TEXT("Choose Drone"), Flow->SelectDrone(Mission->DefaultDroneId));
	TestTrue(TEXT("Request start"), Flow->RequestMissionStart());

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Spawn the wrong course FIRST to catch the old first-Actor binding bug.
	ADroneTrainingCourse* Other = World->SpawnActor<ADroneTrainingCourse>(Params);
	ADroneTrainingCourse* Orbit = World->SpawnActor<ADroneTrainingCourse>(Params);
	ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>(Params);
	ADroneMissionDirector* Director = World->SpawnActor<ADroneMissionDirector>(Params);
	if (!Other || !Orbit || !Drone || !Director) return false;
	Other->Tags.Add(FName(TEXT("Tutorial.GateFlight.Course")));
	Orbit->Tags.Add(Mission->ObjectiveRules[0].TargetId);
	TArray<ADroneTrainingGate*> Gates;
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const float Angle = 2.f * PI * Index / 8.f;
		const FVector Location(1000.f * FMath::Cos(Angle), 1000.f * FMath::Sin(Angle), 500.f);
		const FTransform Transform(FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f), Location);
		ADroneTrainingGate* Gate = World->SpawnActor<ADroneTrainingGate>(ADroneTrainingGate::StaticClass(), Transform, Params);
		if (!Gate) return false;
		Gate->ConfigureGateDefinition(Orbit->GetCourseId(), Index, Index * 800.f);
		Gates.Add(Gate);
	}
	Orbit->ConfigureOrderedGates(Gates);
	if (!Wrapper.BeginPlayInTestWorld()) return false;
	TestTrue(TEXT("Initialize Orbit Mission"), Director->InitializeMission(Flow, Mission, Drone));
	TestTrue(TEXT("Director binds tagged Orbit, not first course"),
		Director->GetTrainingLapRecorder() == Orbit->GetLapRecorderComponent());
	TestFalse(TEXT("Unselected course is disabled"), Other->IsCourseRuntimeActive());
	TestTrue(TEXT("Selected Orbit course is active"), Orbit->IsCourseRuntimeActive());
	TestFalse(TEXT("Turning in place cannot complete Orbit"),
		Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::HeadingAligned, Orbit));
	TestFalse(TEXT("Another course cannot report Orbit completion"),
		Director->ReportObjectiveEvent(EDroneMissionObjectiveEvent::TrainingLap, Other));
	UDroneTrainingGateSequenceComponent* Sequence = Orbit->GetGateSequenceComponent();
	auto Pass = [&](ADroneTrainingGate* Gate, const bool bForward)
	{
		const FVector Delta = Gate->GetForwardDirectionWorld() * (bForward ? 300.f : -300.f);
		const FVector Exit = Gate->GetActorLocation() + Delta;
		Drone->SetActorLocation(Exit, false);
		return Sequence->TryAcceptTraversal(Gate, Drone, Gate->GetActorLocation() - Delta, Exit);
	};
	TestEqual(TEXT("Cannot jump to finish"), Pass(Gates[8], true), EDroneTrainingGatePassResult::WrongOrder);
	TestEqual(TEXT("Reverse start does not count"), Pass(Gates[0], false), EDroneTrainingGatePassResult::WrongDirection);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Wrapper.TickTestWorld(0.1f);
		TestEqual(TEXT("Ordered circular checkpoint accepted"), Pass(Gates[Index], true), EDroneTrainingGatePassResult::Accepted);
	}
	TestEqual(TEXT("Seven eighths of the circle is not a lap"), Orbit->GetLapRecorderComponent()->GetSuccessfulLapCount(), 0);
	TestEqual(TEXT("Mission still waits for full lap"), Director->GetSnapshot().CurrentObjectiveIndex, 0);
	Wrapper.TickTestWorld(0.1f);
	TestEqual(TEXT("Finish after full circuit accepted"), Pass(Gates[8], true), EDroneTrainingGatePassResult::Accepted);
	TestEqual(TEXT("Full lap recorded once"), Orbit->GetLapRecorderComponent()->GetSuccessfulLapCount(), 1);
	TestEqual(TEXT("Lap event advances Mission automatically to Return"), Director->GetCurrentObjective().Event,
		EDroneMissionObjectiveEvent::ReturnToBase);
	TestEqual(TEXT("Not yet Mission Success until Return"), Flow->GetSnapshot().State, EDroneGameFlowState::InMission);
	Wrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}
#endif
