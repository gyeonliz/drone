#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationCommon.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingGate.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTrainingGateSequenceTest,
	"Drone.Tutorial.TrainingGateSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingGateSequenceTest::RunTest(const FString& Parameters)
{
	// 1) CDO에서 Gate와 Sequence가 매 Frame Tick하지 않는 구조인지 먼저 확인한다.
	const ADroneTrainingGate* GateDefaults = GetDefault<ADroneTrainingGate>();
	const UDroneTrainingGateSequenceComponent* SequenceDefaults =
		GetDefault<UDroneTrainingGateSequenceComponent>();
	TestNotNull(TEXT("Training Gate CDO exists"), GateDefaults);
	TestNotNull(TEXT("Gate Sequence CDO exists"), SequenceDefaults);
	if (GateDefaults)
	{
		TestFalse(TEXT("Training Gate avoids per-frame ticking"), GateDefaults->PrimaryActorTick.bCanEverTick);
		TestEqual(
			TEXT("Training Gate builds the fixed Greybox ring segment count"),
			GateDefaults->GetRingVisualSegmentCount(),
			16);
	}
	if (SequenceDefaults)
	{
		TestFalse(
			TEXT("Gate Sequence avoids per-frame ticking"),
			SequenceDefaults->PrimaryComponentTick.bCanEverTick);
	}

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* TestWorld = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("Gate test World exists"), TestWorld);
	if (!TestWorld)
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	if (!WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADroneTrainingCourse* Course = TestWorld->SpawnActor<ADroneTrainingCourse>(
		ADroneTrainingCourse::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	TestNotNull(TEXT("Training Course spawns for Gate sequence"), Course);
	if (!Course)
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	TArray<ADroneTrainingGate*> Gates;
	for (int32 GateIndex = 0; GateIndex < 3; ++GateIndex)
	{
		ADroneTrainingGate* Gate = TestWorld->SpawnActor<ADroneTrainingGate>(
			ADroneTrainingGate::StaticClass(),
			FTransform(FVector(static_cast<float>(GateIndex) * 1000.0f, 0.0f, 0.0f)),
			SpawnParameters);
		TestNotNull(*FString::Printf(TEXT("Gate %d spawns"), GateIndex), Gate);
		if (!Gate)
		{
			continue;
		}

		Gate->ConfigureGateDefinition(
			Course->GetCourseId(),
			GateIndex,
			static_cast<float>(GateIndex) * 1000.0f);
		Gate->RerunConstructionScripts();
		Gates.Add(Gate);
	}

	TestEqual(TEXT("All three test Gates spawned"), Gates.Num(), 3);
	if (Gates.Num() != 3)
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	// 2) Visual과 Trigger를 분리한 실제 Component 계약을 포괄 검사한다.
	for (ADroneTrainingGate* Gate : Gates)
	{
		UBoxComponent* Trigger = Gate->GetGateTrigger();
		TestNotNull(TEXT("Gate owns a separate Box Trigger"), Trigger);
		if (Trigger)
		{
			TestEqual(TEXT("Trigger is query only"), Trigger->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
			TestTrue(TEXT("Trigger generates overlap events"), Trigger->GetGenerateOverlapEvents());
			TestEqual(TEXT("Trigger overlaps Pawn channel"), Trigger->GetCollisionResponseToChannel(ECC_Pawn), ECR_Overlap);
			TestEqual(TEXT("Trigger ignores Visibility channel"), Trigger->GetCollisionResponseToChannel(ECC_Visibility), ECR_Ignore);
			TestFalse(TEXT("Trigger does not simulate physics"), Trigger->IsSimulatingPhysics());
			TestFalse(TEXT("Trigger cannot affect Navigation"), Trigger->CanEverAffectNavigation());
		}

		TInlineComponentArray<UStaticMeshComponent*> VisualSegments;
		Gate->GetComponents(VisualSegments);
		TestEqual(TEXT("Gate owns sixteen visible Ring segments"), VisualSegments.Num(), 16);
		for (UStaticMeshComponent* Segment : VisualSegments)
		{
			if (!Segment)
			{
				continue;
			}

			TestNotNull(TEXT("Ring segment has a Greybox Mesh"), Segment->GetStaticMesh().Get());
			TestEqual(TEXT("Ring segment has no collision"), Segment->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Ring segment creates no overlaps"), Segment->GetGenerateOverlapEvents());
			TestFalse(TEXT("Ring segment does not simulate physics"), Segment->IsSimulatingPhysics());
			TestFalse(TEXT("Ring segment cannot affect Navigation"), Segment->CanEverAffectNavigation());
			TestTrue(TEXT("Ring segment is visible in game"), Segment->IsVisible() && !Segment->bHiddenInGame);
		}
	}

	Course->ConfigureOrderedGates(Gates);
	UDroneTrainingGateSequenceComponent* Sequence = Course->GetGateSequenceComponent();
	TestNotNull(TEXT("Course owns the non-Primitive Gate Sequence"), Sequence);
	if (!Sequence)
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	// 3) 배열 위치가 단일 순서 기준이고 시작 시 Gate 0만 Current인지 확인한다.
	TestTrue(TEXT("Three-Gate configuration is valid"), Sequence->IsConfigurationValid());
	TestEqual(TEXT("Sequence contains all configured Gates"), Sequence->GetConfiguredGateCount(), 3);
	TestEqual(TEXT("Gate 0 is initially expected"), Sequence->GetCurrentGateIndex(), 0);
	TestTrue(TEXT("Gate 0 is the current Gate"), Sequence->GetCurrentGate() == Gates[0]);
	TestEqual(TEXT("Gate 0 visual is Current"), Gates[0]->GetGateVisualState(), EDroneTrainingGateVisualState::Current);
	TestEqual(TEXT("Gate 1 visual is Inactive"), Gates[1]->GetGateVisualState(), EDroneTrainingGateVisualState::Inactive);
	TestEqual(TEXT("Gate 2 visual is Inactive"), Gates[2]->GetGateVisualState(), EDroneTrainingGateVisualState::Inactive);

	const FLinearColor BeforePassColor(0.15f, 0.20f, 0.25f, 1.0f);
	const FLinearColor CurrentTargetColor(1.0f, 0.65f, 0.05f, 1.0f);
	const FLinearColor AfterPassColor(0.05f, 0.45f, 1.0f, 1.0f);
	Gates[0]->SetGateStateColors(BeforePassColor, CurrentTargetColor, AfterPassColor);
	TestTrue(
		TEXT("Blueprint color API stores the before-pass color"),
		Gates[0]->GetColorForVisualState(EDroneTrainingGateVisualState::Inactive).Equals(BeforePassColor));
	TestTrue(
		TEXT("Blueprint color API stores the current-target color"),
		Gates[0]->GetColorForVisualState(EDroneTrainingGateVisualState::Current).Equals(CurrentTargetColor));
	TestTrue(
		TEXT("Blueprint color API stores the after-pass color"),
		Gates[0]->GetColorForVisualState(EDroneTrainingGateVisualState::Completed).Equals(AfterPassColor));

	ADronePrototypePawn* Drone = TestWorld->SpawnActor<ADronePrototypePawn>(
		ADronePrototypePawn::StaticClass(),
		FTransform(FVector(-300.0f, 0.0f, 0.0f)),
		SpawnParameters);
	ADefaultPawn* WrongPawn = TestWorld->SpawnActor<ADefaultPawn>(
		ADefaultPawn::StaticClass(),
		FTransform(FVector(-300.0f, 0.0f, 0.0f)),
		SpawnParameters);
	TestNotNull(TEXT("Prototype Drone spawns for Gate validation"), Drone);
	TestNotNull(TEXT("Non-Drone Pawn spawns for rejection validation"), WrongPawn);
	if (!Drone || !WrongPawn)
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	auto ForwardEntry = [](const ADroneTrainingGate* Gate)
	{
		return Gate->GetActorLocation() - Gate->GetForwardDirectionWorld() * 300.0f;
	};
	auto ForwardExit = [](const ADroneTrainingGate* Gate)
	{
		return Gate->GetActorLocation() + Gate->GetForwardDirectionWorld() * 300.0f;
	};

	// 4) 잘못된 Actor, 미래 Gate, 역방향, 정지 통과는 상태를 바꾸지 않는다.
	TestEqual(
		TEXT("Non-Drone Actor is rejected"),
		Sequence->TryAcceptTraversal(Gates[0], WrongPawn, ForwardEntry(Gates[0]), ForwardExit(Gates[0])),
		EDroneTrainingGatePassResult::InvalidActor);
	TestEqual(
		TEXT("Future Gate is rejected"),
		Sequence->TryAcceptTraversal(Gates[1], Drone, ForwardEntry(Gates[1]), ForwardExit(Gates[1])),
		EDroneTrainingGatePassResult::WrongOrder);
	TestEqual(
		TEXT("Reverse traversal is rejected"),
		Sequence->TryAcceptTraversal(Gates[0], Drone, ForwardExit(Gates[0]), ForwardEntry(Gates[0])),
		EDroneTrainingGatePassResult::WrongDirection);
	TestEqual(
		TEXT("Zero-distance traversal is rejected"),
		Sequence->TryAcceptTraversal(Gates[0], Drone, Gates[0]->GetActorLocation(), Gates[0]->GetActorLocation()),
		EDroneTrainingGatePassResult::WrongDirection);
	const FVector OutsideCircularAperture =
		Gates[0]->GetActorRightVector() * 170.0f + Gates[0]->GetActorUpVector() * 170.0f;
	TestEqual(
		TEXT("Box corner outside the circular aperture is rejected"),
		Sequence->TryAcceptTraversal(
			Gates[0],
			Drone,
			ForwardEntry(Gates[0]) + OutsideCircularAperture,
			ForwardExit(Gates[0]) + OutsideCircularAperture),
		EDroneTrainingGatePassResult::WrongDirection);
	TestEqual(TEXT("Rejected attempts do not advance"), Sequence->GetAcceptedGateCount(), 0);

	// Actor Scale이 바뀌어도 화면과 Trigger가 커진 만큼 같은 local aperture를 인정한다.
	Gates[0]->SetActorScale3D(FVector(2.0f));
	const FVector ScaledInsideAperture = Gates[0]->GetActorRightVector() * 250.0f;
	TestEqual(
		TEXT("Scaled Gate accepts a point inside its scaled circular aperture"),
		Sequence->TryAcceptTraversal(
			Gates[0],
			Drone,
			ForwardEntry(Gates[0]) + ScaledInsideAperture,
			ForwardExit(Gates[0]) + ScaledInsideAperture),
		EDroneTrainingGatePassResult::Accepted);
	Sequence->ResetSequence();
	Gates[0]->SetActorScale3D(FVector::OneVector);

	// 5) 정상 통과는 한 칸만 진행하고 중복 통과는 다시 기록하지 않는다.
	TestEqual(
		TEXT("Current Gate accepts one forward traversal"),
		Sequence->TryAcceptTraversal(Gates[0], Drone, ForwardEntry(Gates[0]), ForwardExit(Gates[0])),
		EDroneTrainingGatePassResult::Accepted);
	TestEqual(TEXT("One Gate is accepted"), Sequence->GetAcceptedGateCount(), 1);
	TestEqual(TEXT("Gate 0 becomes Completed"), Gates[0]->GetGateVisualState(), EDroneTrainingGateVisualState::Completed);
	TestEqual(TEXT("Gate 1 becomes Current"), Gates[1]->GetGateVisualState(), EDroneTrainingGateVisualState::Current);
	TestEqual(
		TEXT("Completed Gate cannot be accepted twice"),
		Sequence->TryAcceptTraversal(Gates[0], Drone, ForwardEntry(Gates[0]), ForwardExit(Gates[0])),
		EDroneTrainingGatePassResult::AlreadyCompleted);
	TestEqual(TEXT("Duplicate traversal leaves progress unchanged"), Sequence->GetAcceptedGateCount(), 1);

	TestEqual(
		TEXT("Gate 1 accepts in order"),
		Sequence->TryAcceptTraversal(Gates[1], Drone, ForwardEntry(Gates[1]), ForwardExit(Gates[1])),
		EDroneTrainingGatePassResult::Accepted);
	TestEqual(
		TEXT("Gate 2 accepts in order"),
		Sequence->TryAcceptTraversal(Gates[2], Drone, ForwardEntry(Gates[2]), ForwardExit(Gates[2])),
		EDroneTrainingGatePassResult::Accepted);
	TestTrue(TEXT("Sequence completes after the final Gate"), Sequence->IsSequenceComplete());
	TestEqual(TEXT("There is no next Gate after completion"), Sequence->GetCurrentGateIndex(), INDEX_NONE);
	for (ADroneTrainingGate* Gate : Gates)
	{
		TestEqual(TEXT("Every Gate is Completed"), Gate->GetGateVisualState(), EDroneTrainingGateVisualState::Completed);
	}

	Sequence->ResetSequence();
	TestEqual(TEXT("Reset returns to Gate 0"), Sequence->GetCurrentGateIndex(), 0);
	TestEqual(TEXT("Reset clears accepted count"), Sequence->GetAcceptedGateCount(), 0);
	TestEqual(TEXT("Reset restores Gate 0 Current"), Gates[0]->GetGateVisualState(), EDroneTrainingGateVisualState::Current);

	// 6) 실제 Collision World의 Begin/End Overlap으로 실패·Reset·정상·중복 흐름을 검사한다.
	auto TraverseActualTrigger = [&](AActor* PassingActor, ADroneTrainingGate* Gate, bool bForward, const FVector& PlaneOffset)
	{
		const FVector Direction = Gate->GetForwardDirectionWorld();
		const FVector Start = Gate->GetActorLocation() + PlaneOffset + Direction * (bForward ? -300.0f : 300.0f);
		const FVector Inside = Gate->GetActorLocation() + PlaneOffset + Direction * (bForward ? -30.0f : 30.0f);
		const FVector End = Gate->GetActorLocation() + PlaneOffset + Direction * (bForward ? 300.0f : -300.0f);

		PassingActor->SetActorLocation(Start, false);
		WorldWrapper.TickTestWorld();
		PassingActor->SetActorLocation(Inside, false);
		WorldWrapper.TickTestWorld();
		FHitResult SweepHit;
		PassingActor->SetActorLocation(End, true, &SweepHit);
		WorldWrapper.TickTestWorld();
		return SweepHit;
	};

	Sequence->ResetSequence();
	TraverseActualTrigger(WrongPawn, Gates[0], true, FVector::ZeroVector);
	TestEqual(TEXT("Actual non-Drone Pawn overlap does not advance"), Sequence->GetAcceptedGateCount(), 0);
	WrongPawn->SetActorLocation(FVector(0.0f, 5000.0f, 5000.0f), false);
	WorldWrapper.TickTestWorld();

	TraverseActualTrigger(Drone, Gates[1], true, FVector::ZeroVector);
	TestEqual(TEXT("Actual future Gate overlap does not advance"), Sequence->GetAcceptedGateCount(), 0);

	TraverseActualTrigger(Drone, Gates[0], false, FVector::ZeroVector);
	TestEqual(TEXT("Actual reverse Gate overlap does not advance"), Sequence->GetAcceptedGateCount(), 0);

	TraverseActualTrigger(Drone, Gates[0], true, OutsideCircularAperture);
	TestEqual(TEXT("Actual Box corner overlap outside the Ring does not advance"), Sequence->GetAcceptedGateCount(), 0);

	// BeginOverlap 뒤 Reset하면 이전 시도의 EndOverlap이 새 시도로 승인되면 안 된다.
	Drone->SetActorLocation(ForwardEntry(Gates[0]), false);
	WorldWrapper.TickTestWorld();
	Drone->SetActorLocation(
		Gates[0]->GetActorLocation() - Gates[0]->GetForwardDirectionWorld() * 30.0f,
		false);
	WorldWrapper.TickTestWorld();
	Sequence->ResetSequence();
	Drone->SetActorLocation(ForwardExit(Gates[0]), false);
	WorldWrapper.TickTestWorld();
	TestEqual(TEXT("Reset cancels an in-progress Gate overlap"), Sequence->GetAcceptedGateCount(), 0);

	const FHitResult TriggerSweepHit = TraverseActualTrigger(Drone, Gates[0], true, FVector::ZeroVector);
	TestFalse(TEXT("Gate Trigger and Ring do not block the Drone"), TriggerSweepHit.bBlockingHit);
	TestEqual(TEXT("Actual forward Overlap advances exactly once"), Sequence->GetAcceptedGateCount(), 1);

	TraverseActualTrigger(Drone, Gates[0], true, FVector::ZeroVector);
	TestEqual(TEXT("Actual completed Gate overlap cannot advance twice"), Sequence->GetAcceptedGateCount(), 1);

	// 7) 중복 참조, CourseId, GateIndex 오류는 안전하게 구성을 거부한다.
	TArray<ADroneTrainingGate*> DuplicateGates = { Gates[0], Gates[0] };
	Course->ConfigureOrderedGates(DuplicateGates);
	TestFalse(TEXT("Duplicate Gate references invalidate the sequence"), Sequence->IsConfigurationValid());
	TestEqual(
		TEXT("Invalid configuration rejects traversal"),
		Sequence->TryAcceptTraversal(Gates[0], Drone, ForwardEntry(Gates[0]), ForwardExit(Gates[0])),
		EDroneTrainingGatePassResult::InvalidConfiguration);

	Gates[1]->ConfigureGateDefinition(TEXT("OtherCourse"), 7, 1000.0f);
	Course->ConfigureOrderedGates(Gates);
	TestTrue(TEXT("Course array automatically repairs Gate metadata"), Sequence->IsConfigurationValid());
	TestEqual(TEXT("Automatic repair restores CourseId"), Gates[1]->GetCourseId(), Course->GetCourseId());
	TestEqual(TEXT("Automatic repair restores GateIndex"), Gates[1]->GetGateIndex(), 1);

	// 구성 뒤 외부에서 Gate 정의가 변조되면 기존 Sequence는 즉시 무효가 된다.
	Gates[1]->ConfigureGateDefinition(TEXT("OtherCourse"), 7, 1000.0f);
	TestFalse(TEXT("Post-configuration Gate metadata tampering invalidates the sequence"), Sequence->IsConfigurationValid());
	Course->SynchronizeGateDefinitions();
	TestTrue(TEXT("Explicit synchronization recovers the sequence"), Sequence->IsConfigurationValid());

	// Current Gate가 먼저 파괴되면 외부 조회에서도 즉시 Invalid로 보이고 current가 사라진다.
	ADroneTrainingCourse* GateFirstCourse = TestWorld->SpawnActor<ADroneTrainingCourse>(
		ADroneTrainingCourse::StaticClass(),
		FTransform(FVector(0.0f, 3000.0f, 0.0f)),
		SpawnParameters);
	ADroneTrainingGate* GateFirst = TestWorld->SpawnActor<ADroneTrainingGate>(
		ADroneTrainingGate::StaticClass(),
		FTransform(FVector(0.0f, 3000.0f, 0.0f)),
		SpawnParameters);
	TestNotNull(TEXT("Gate-first lifecycle Course spawns"), GateFirstCourse);
	TestNotNull(TEXT("Gate-first lifecycle Gate spawns"), GateFirst);
	if (GateFirstCourse && GateFirst)
	{
		GateFirst->ConfigureGateDefinition(GateFirstCourse->GetCourseId(), 0, 0.0f);
		GateFirstCourse->ConfigureOrderedGates({ GateFirst });
		UDroneTrainingGateSequenceComponent* GateFirstSequence = GateFirstCourse->GetGateSequenceComponent();
		TestNotNull(TEXT("Gate-first lifecycle Sequence exists"), GateFirstSequence);
		if (GateFirstSequence)
		{
			TestTrue(TEXT("Gate-first lifecycle sequence starts valid"), GateFirstSequence->IsConfigurationValid());
			TestTrue(TEXT("Current Gate can be destroyed first"), GateFirst->Destroy());
			WorldWrapper.TickTestWorld();
			TestFalse(TEXT("Destroyed current Gate immediately invalidates the sequence"), GateFirstSequence->IsConfigurationValid());
			TestNull(TEXT("Destroyed current Gate is never returned"), GateFirstSequence->GetCurrentGate());
		}
		GateFirstCourse->Destroy();
		WorldWrapper.TickTestWorld();
	}

	// Overlap 중 Pawn이 파괴되어도 Gate에 stale weak-key가 남지 않는다.
	ADronePrototypePawn* DestroyedInsideDrone = TestWorld->SpawnActor<ADronePrototypePawn>(
		ADronePrototypePawn::StaticClass(),
		FTransform(ForwardEntry(Gates[0])),
		SpawnParameters);
	TestNotNull(TEXT("Destroy-inside Drone spawns"), DestroyedInsideDrone);
	if (DestroyedInsideDrone)
	{
		Sequence->ResetSequence();
		DestroyedInsideDrone->SetActorLocation(
			Gates[0]->GetActorLocation() - Gates[0]->GetForwardDirectionWorld() * 30.0f,
			false);
		WorldWrapper.TickTestWorld();
		TestEqual(TEXT("Gate stores one pending entry before Pawn destruction"), Gates[0]->GetPendingTraversalCount(), 1);
		DestroyedInsideDrone->Destroy();
		WorldWrapper.TickTestWorld();
		TestEqual(TEXT("Pawn destruction clears the pending Gate entry"), Gates[0]->GetPendingTraversalCount(), 0);
	}

	// Course가 먼저 사라져도 남은 Gate가 파괴된 Sequence를 역참조하지 않는다.
	TestTrue(TEXT("Training Course can be destroyed"), Course->Destroy());
	WorldWrapper.TickTestWorld();
	for (ADroneTrainingGate* Gate : Gates)
	{
		TestNull(TEXT("Course EndPlay detaches its Gate Sequence"), Gate->GetAssignedGateSequence());
	}

	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif
