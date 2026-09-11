#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationCommon.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingGate.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTrainingCourseTest,
	"Drone.Tutorial.TrainingCourse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingCourseTest::RunTest(const FString& Parameters)
{
	// 1) CDO 검사는 실수로 Course Tick이나 Actor Collision을 켜는 회귀를 빠르게 잡는다.
	const ADroneTrainingCourse* CourseDefaults = GetDefault<ADroneTrainingCourse>();
	TestNotNull(TEXT("Training Course CDO exists"), CourseDefaults);
	if (CourseDefaults)
	{
		TestFalse(TEXT("Training Course class is concrete"), CourseDefaults->GetClass()->HasAnyClassFlags(CLASS_Abstract));
		TestFalse(TEXT("Training Course avoids per-frame ticking"), CourseDefaults->PrimaryActorTick.bCanEverTick);
		TestFalse(TEXT("Training Course Actor collision is disabled"), CourseDefaults->GetActorEnableCollision());

		const USplineComponent* DefaultSpline = CourseDefaults->GetCourseSpline();
		TestNotNull(TEXT("Training Course owns an editable Spline"), DefaultSpline);
		if (DefaultSpline)
		{
			TestTrue(TEXT("Default Spline has at least two points"), DefaultSpline->GetNumberOfSplinePoints() >= 2);
			TestTrue(TEXT("Default Spline has a positive length"), DefaultSpline->GetSplineLength() > UE_SMALL_NUMBER);
			TestEqual(TEXT("Spline collision is disabled"), DefaultSpline->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Spline overlap generation is disabled"), DefaultSpline->GetGenerateOverlapEvents());
			TestFalse(TEXT("Spline cannot affect Navigation"), DefaultSpline->CanEverAffectNavigation());
		}
	}

	// 2) 실제 World에 Spawn해야 OnConstruction이 만든 SplineMesh와 물리 질의를 함께 검사할 수 있다.
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* TestWorld = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("Transient Course test world exists"), TestWorld);
	if (!TestWorld)
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
	TestNotNull(TEXT("Training Course spawns in a runtime World"), Course);
	if (!Course)
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	const int32 ExpectedSegmentCount = Course->GetExpectedCourseLineSegmentCount();
	TestEqual(
		TEXT("Construction creates the distance-sampled guide Segments"),
		Course->GetCourseLineSegmentCount(),
		ExpectedSegmentCount);
	TestTrue(
		TEXT("Long curved paths use more visual Segments than control-point intervals"),
		Course->GetCourseSpline()
			&& ExpectedSegmentCount > Course->GetCourseSpline()->GetNumberOfSplinePoints() - 1);

	// 모든 Primitive를 포괄 검사해 재구성 중 남은 옛 Segment도 안전 계약을 우회하지 못하게 한다.
	TInlineComponentArray<UPrimitiveComponent*> CoursePrimitives;
	Course->GetComponents(CoursePrimitives);
	int32 GeneratedSegmentCount = 0;
	for (UPrimitiveComponent* Primitive : CoursePrimitives)
	{
		if (!Primitive)
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("%s has no collision"), *Primitive->GetName()),
			Primitive->GetCollisionEnabled(),
			ECollisionEnabled::NoCollision);
		TestFalse(
			*FString::Printf(TEXT("%s creates no overlap events"), *Primitive->GetName()),
			Primitive->GetGenerateOverlapEvents());
		TestFalse(
			*FString::Printf(TEXT("%s does not simulate physics"), *Primitive->GetName()),
			Primitive->IsSimulatingPhysics());
		TestFalse(
			*FString::Printf(TEXT("%s cannot affect Navigation"), *Primitive->GetName()),
			Primitive->CanEverAffectNavigation());

		if (USplineMeshComponent* Segment = Cast<USplineMeshComponent>(Primitive);
			Segment && Segment->ComponentHasTag(ADroneTrainingCourse::GetGeneratedSegmentTag()))
		{
			++GeneratedSegmentCount;
			TestNotNull(TEXT("Generated guide Segment has a visible Static Mesh"), Segment->GetStaticMesh().Get());
			TestTrue(TEXT("Generated guide Segment is visible"), Segment->IsVisible());
			TestFalse(TEXT("Generated guide Segment is not hidden in game"), Segment->bHiddenInGame);
		}
	}
	TestEqual(TEXT("Primitive scan finds every generated guide Segment"), GeneratedSegmentCount, ExpectedSegmentCount);

	// 3) BP/Level에서 안전 값을 잘못 바꿔도 Construction이 복원하며 Segment를 누적하지 않아야 한다.
	Course->SetActorEnableCollision(true);
	if (USplineComponent* MutableSpline = Course->GetCourseSpline())
	{
		MutableSpline->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MutableSpline->SetGenerateOverlapEvents(true);
		MutableSpline->SetCanEverAffectNavigation(true);
	}
	Course->RerunConstructionScripts();
	Course->RerunConstructionScripts();
	TestFalse(TEXT("Construction restores disabled Actor collision"), Course->GetActorEnableCollision());
	if (const USplineComponent* RestoredSpline = Course->GetCourseSpline())
	{
		TestEqual(
			TEXT("Construction restores disabled Spline collision"),
			RestoredSpline->GetCollisionEnabled(),
			ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Construction restores disabled Spline overlaps"), RestoredSpline->GetGenerateOverlapEvents());
		TestFalse(TEXT("Construction restores disabled Spline Navigation impact"), RestoredSpline->CanEverAffectNavigation());
	}
	TestEqual(
		TEXT("Repeated Construction does not duplicate guide Segments"),
		Course->GetCourseLineSegmentCount(),
		ExpectedSegmentCount);

	// 4) Drone Collision 크기로 안내선을 가로질러 Sweep해 실제 Blocking Hit가 없음을 확인한다.
	ADronePrototypePawn* Drone = TestWorld->SpawnActor<ADronePrototypePawn>(
		ADronePrototypePawn::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	TestNotNull(TEXT("Prototype Drone spawns for non-interference sweep"), Drone);
	if (Drone && Course->GetCourseSpline())
	{
		const float ProbeDistance = Course->GetCourseSpline()->GetSplineLength() * 0.12f;
		const FVector ProbeCenter = Course->GetCourseSpline()->GetLocationAtDistanceAlongSpline(
			ProbeDistance,
			ESplineCoordinateSpace::World);
		const FVector ProbeDirection = Course->GetCourseSpline()->GetDirectionAtDistanceAlongSpline(
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
		TestFalse(TEXT("Drone sweep across the guide line has no Blocking Hit"), SweepHit.bBlockingHit);
		TestTrue(
			TEXT("Drone reaches the other side of the guide line"),
			Drone->GetActorLocation().Equals(SweepEnd, 1.0f));
		TestTrue(
			TEXT("Guide line never reports itself as a sweep obstacle"),
			SweepHit.GetActor() != Course);
	}

	// 5) 자동 Gate 모드는 개수와 거리 수치만으로 Ring을 만들고 Spline 접선에 정렬해야 한다.
	Course->ConfigureAutomaticGateLayout(true, 5, true, 100.0f, 900.0f, 150.0f);
	const float TestSplineLength = Course->GetCourseSpline() ? Course->GetCourseSpline()->GetSplineLength() : 0.0f;
	const TArray<float> PerRingDistances = {
		TestSplineLength * 0.05f,
		TestSplineLength * 0.22f,
		TestSplineLength * 0.47f,
		TestSplineLength * 0.71f,
		TestSplineLength * 0.94f,
	};
	const TArray<FVector> PerRingLocalOffsets = {
		FVector(0.0f, -30.0f, 20.0f),
		FVector(0.0f, 45.0f, 65.0f),
		FVector(0.0f, 0.0f, -25.0f),
		FVector(0.0f, -70.0f, 10.0f),
		FVector(0.0f, 25.0f, 35.0f),
	};
	Course->ConfigureAutomaticGateOverrides(PerRingDistances, PerRingLocalOffsets);
	TestTrue(TEXT("Automatic Spline Gate mode is enabled"), Course->IsUsingAutomaticSplineGates());
	TestEqual(TEXT("Automatic mode creates requested Ring count"), Course->GetGeneratedAutomaticGateCount(), 5);
	TestEqual(TEXT("Active Gate order uses all generated Rings"), Course->GetOrderedGates().Num(), 5);
	if (UDroneTrainingGateSequenceComponent* Sequence = Course->GetGateSequenceComponent())
	{
		TestTrue(TEXT("Generated Ring sequence is valid"), Sequence->IsConfigurationValid());
		TestEqual(TEXT("Generated Ring sequence has requested count"), Sequence->GetConfiguredGateCount(), 5);
	}

	for (int32 GateIndex = 0; GateIndex < Course->GetOrderedGates().Num(); ++GateIndex)
	{
		const ADroneTrainingGate* Gate = Course->GetOrderedGates()[GateIndex];
		TestNotNull(*FString::Printf(TEXT("Generated Ring %d exists"), GateIndex), Gate);
		if (!Gate || !Course->GetCourseSpline())
		{
			continue;
		}

		const float ExpectedDistance = Course->GetAutomaticGateDistanceAlongSpline(GateIndex);
		const FVector ExpectedSplineLocation = Course->GetCourseSpline()->GetLocationAtDistanceAlongSpline(
			ExpectedDistance,
			ESplineCoordinateSpace::World);
		const FQuat ExpectedSplineRotation = Course->GetCourseSpline()->GetQuaternionAtDistanceAlongSpline(
			ExpectedDistance,
			ESplineCoordinateSpace::World);
		const FVector ExpectedLocation = ExpectedSplineLocation
			+ ExpectedSplineRotation.RotateVector(PerRingLocalOffsets[GateIndex]);
		const FVector ExpectedDirection = Course->GetCourseSpline()->GetDirectionAtDistanceAlongSpline(
			ExpectedDistance,
			ESplineCoordinateSpace::World);
		TestEqual(*FString::Printf(TEXT("Generated Ring %d receives ordered index"), GateIndex), Gate->GetGateIndex(), GateIndex);
		TestTrue(
			*FString::Printf(TEXT("Generated Ring %d sits on Spline"), GateIndex),
			Gate->GetActorLocation().Equals(ExpectedLocation, 1.0f));
		TestTrue(
			*FString::Printf(TEXT("Generated Ring %d follows Spline tangent"), GateIndex),
			FVector::DotProduct(Gate->GetForwardDirectionWorld(), ExpectedDirection) > 0.999f);
		TestTrue(
			*FString::Printf(TEXT("Generated Ring %d stores Spline distance"), GateIndex),
			FMath::IsNearlyEqual(Gate->GetSegmentDistance(), ExpectedDistance, 0.1f));
		TestTrue(
			*FString::Printf(TEXT("Generated Ring %d uses its absolute Spline distance override"), GateIndex),
			FMath::IsNearlyEqual(ExpectedDistance, PerRingDistances[GateIndex], 0.1f));
	}

	Course->RebuildAutomaticGates();
	Course->RebuildAutomaticGates();
	TestEqual(TEXT("Repeated automatic rebuild does not duplicate Rings"), Course->GetGeneratedAutomaticGateCount(), 5);
	TestEqual(TEXT("Repeated automatic rebuild preserves sequence count"), Course->GetOrderedGates().Num(), 5);

	// 6) 독립 Handle 모드는 Course Spline을 바꾸지 않고 Ring별 위치만 곡선 위에서 조정해야 한다.
	Course->ConfigureAutomaticGateOverrides({}, {});
	Course->InitializeAutomaticGateSplineHandlesFromCurrentLayout();
	USplineComponent* PointDrivenSpline = Course->GetCourseSpline();
	TestNotNull(TEXT("Handle-driven Ring mode keeps an editable Course Spline"), PointDrivenSpline);
	if (PointDrivenSpline)
	{
		const int32 InitialPointCount = PointDrivenSpline->GetNumberOfSplinePoints();
		TArray<FVector> OriginalSplinePointLocations;
		OriginalSplinePointLocations.Reserve(InitialPointCount);
		for (int32 PointIndex = 0; PointIndex < InitialPointCount; ++PointIndex)
		{
			OriginalSplinePointLocations.Add(PointDrivenSpline->GetLocationAtSplinePoint(
				PointIndex,
				ESplineCoordinateSpace::Local));
		}
		TestTrue(TEXT("Independent Ring Handle mode is enabled"), Course->IsUsingIndependentAutomaticGateHandles());
		TestEqual(TEXT("Current five-Ring layout creates five independent Handles"), Course->GetResolvedAutomaticGateCount(), 5);
		TestEqual(TEXT("Handle count drives generated Ring count"), Course->GetGeneratedAutomaticGateCount(), 5);
		TestEqual(TEXT("Handle-driven Rings become the active ordered sequence"), Course->GetOrderedGates().Num(), 5);

		const TArray<FVector> InitialHandleLocations = Course->GetAutomaticGateSplineHandles();
		TestEqual(TEXT("Handle array has one entry per generated Ring"), InitialHandleLocations.Num(), 5);

		const int32 MovedHandleIndex = 1;
		TArray<FVector> EditedHandles = InitialHandleLocations;
		const FVector UnsnappedHandleLocation = EditedHandles[MovedHandleIndex] + FVector(260.0f, 430.0f, 170.0f);
		const FVector UnsnappedHandleWorldLocation = Course->GetActorTransform().TransformPosition(UnsnappedHandleLocation);
		const float ExpectedInputKey = PointDrivenSpline->FindInputKeyClosestToWorldLocation(UnsnappedHandleWorldLocation);
		const FVector ExpectedSnappedWorldLocation = PointDrivenSpline->GetLocationAtSplineInputKey(
			ExpectedInputKey,
			ESplineCoordinateSpace::World);
		EditedHandles[MovedHandleIndex] = UnsnappedHandleLocation;
		Course->ConfigureAutomaticGateSplineHandles(EditedHandles);
		const ADroneTrainingGate* MovedGate = Course->GetOrderedGates().IsValidIndex(MovedHandleIndex)
			? Course->GetOrderedGates()[MovedHandleIndex]
			: nullptr;
		TestNotNull(TEXT("Moving an independent Handle keeps its matching Ring"), MovedGate);
		if (MovedGate)
		{
			TestTrue(
				TEXT("Moving a Handle snaps its matching Ring to the closest Spline location"),
				MovedGate->GetActorLocation().Equals(ExpectedSnappedWorldLocation, 1.0f));
		}

		const FVector AddedHandleWorldLocation = PointDrivenSpline->GetLocationAtDistanceAlongSpline(
			PointDrivenSpline->GetSplineLength() * 0.83f,
			ESplineCoordinateSpace::World);
		EditedHandles = Course->GetAutomaticGateSplineHandles();
		EditedHandles.Add(Course->GetActorTransform().InverseTransformPosition(AddedHandleWorldLocation));
		Course->ConfigureAutomaticGateSplineHandles(EditedHandles);
		TestEqual(TEXT("Adding one independent Handle adds one Ring"), Course->GetGeneratedAutomaticGateCount(), 6);
		TestEqual(TEXT("Added Handle Ring joins the ordered sequence"), Course->GetOrderedGates().Num(), 6);

		EditedHandles.Pop();
		Course->ConfigureAutomaticGateSplineHandles(EditedHandles);
		TestEqual(TEXT("Removing one independent Handle removes its Ring"), Course->GetGeneratedAutomaticGateCount(), 5);
		TestEqual(TEXT("Removing a Handle restores the sequence count"), Course->GetOrderedGates().Num(), 5);

		TestEqual(TEXT("Ring Handle edits do not change Course Spline Point count"), PointDrivenSpline->GetNumberOfSplinePoints(), InitialPointCount);
		for (int32 PointIndex = 0; PointIndex < InitialPointCount; ++PointIndex)
		{
			TestTrue(
				*FString::Printf(TEXT("Ring Handle edits preserve Course Spline Point %d"), PointIndex),
				PointDrivenSpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local)
					.Equals(OriginalSplinePointLocations[PointIndex], 0.1f));
		}
	}

	Course->ConfigureAutomaticGateLayout(false, 5, true, 100.0f, 900.0f, 150.0f);
	TestEqual(TEXT("Disabling automatic mode removes generated Rings"), Course->GetGeneratedAutomaticGateCount(), 0);

	WorldWrapper.TickTestWorld();
	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif
