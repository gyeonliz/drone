#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingGate.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"
#include "Prototype/DronePrototypePawn.h"
#include "Components/ChildActorComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWave.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneTrainingGatePresentationTest,
	"Drone.Tutorial.TrainingGatePresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingGatePresentationTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADroneTrainingCourse* Course = World->SpawnActor<ADroneTrainingCourse>(
		ADroneTrainingCourse::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("Presentation Course spawns"), Course);
	if (!Course) { return false; }
	Course->ConfigureAutomaticGateLayout(true, 3, true, 200.0f, 1000.0f, 200.0f);
	USplineComponent* Spline = Course->GetCourseSpline();
	const float OriginalSplineLength = Spline->GetSplineLength();
	TArray<FVector> OriginalPoints;
	for (int32 Index = 0; Index < Spline->GetNumberOfSplinePoints(); ++Index)
	{
		OriginalPoints.Add(Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local));
	}
	TInlineComponentArray<USplineMeshComponent*> Lines;
	Course->GetComponents(Lines);
	TestTrue(TEXT("Course has generated visible lines"), !Lines.IsEmpty());
	if (Lines.IsEmpty()) { return false; }
	USplineMeshComponent* FirstLine = Lines[0];
	const FVector2D OriginalLineWidth = FirstLine->GetStartScale();
	const FVector OriginalLineStart = FirstLine->GetStartPosition();
	const FVector OriginalLineEnd = FirstLine->GetEndPosition();
	const int32 OriginalLineCount = Course->GetCourseLineSegmentCount();

	auto CheckHeightFraction = [&](float ExpectedFraction)
	{
		for (const ADroneTrainingGate* Gate : Course->GetOrderedGates())
		{
			const FVector Anchor = Spline->GetLocationAtDistanceAlongSpline(
				Gate->GetSegmentDistance(), ESplineCoordinateSpace::World);
			const FVector LocalAnchor = Gate->GetActorTransform().InverseTransformPosition(Anchor);
			const float HalfHeight = Gate->GetTriggerApertureHalfSizeCentimeters();
			TestTrue(TEXT("Guide line crosses the Gate at the requested fraction above its inner bottom"),
				FMath::IsNearlyEqual((LocalAnchor.Z + HalfHeight) / (2.0f * HalfHeight), ExpectedFraction, 0.001f));
			TestTrue(TEXT("Guide line remains horizontally centered within the Gate"),
				FMath::IsNearlyZero(LocalAnchor.Y, 0.01f));
		}
	};
	CheckHeightFraction(1.0f / 6.0f);
	Course->ConfigureAutomaticGatePresentation(FVector::ZeroVector,
		FVector(1.0f, 2.0f, 1.5f), 1.0f / 6.0f, { FVector::OneVector, FVector(1.0f, 0.75f, 2.0f) });
	CheckHeightFraction(1.0f / 6.0f);
	TestTrue(TEXT("Common Gate scale changes Gate 0 only, not the Course"),
		Course->GetOrderedGates()[0]->GetActorScale3D().Equals(FVector(1.0f, 2.0f, 1.5f), 0.001f));
	TestTrue(TEXT("Per-Gate scale multiplies common scale"),
		Course->GetOrderedGates()[1]->GetActorScale3D().Equals(FVector(1.0f, 1.5f, 3.0f), 0.001f));
	TestTrue(TEXT("Missing per-Gate entry falls back to common scale"),
		Course->GetOrderedGates()[2]->GetActorScale3D().Equals(FVector(1.0f, 2.0f, 1.5f), 0.001f));
	TestTrue(TEXT("Gate scale never changes Course Actor scale"), Course->GetActorScale3D().Equals(FVector::OneVector));
	TestEqual(TEXT("Gate scale never changes spline length"), Spline->GetSplineLength(), OriginalSplineLength);
	TestEqual(TEXT("Gate changes never rebuild or duplicate course lines"), Course->GetCourseLineSegmentCount(), OriginalLineCount);
	TestTrue(TEXT("Gate size leaves glowing line width and thickness untouched"), FirstLine->GetStartScale().Equals(OriginalLineWidth));
	TestTrue(TEXT("Gate size leaves glowing line start untouched"), FirstLine->GetStartPosition().Equals(OriginalLineStart));
	TestTrue(TEXT("Gate size leaves glowing line end untouched"), FirstLine->GetEndPosition().Equals(OriginalLineEnd));
	for (int32 Index = 0; Index < OriginalPoints.Num(); ++Index)
	{
		TestTrue(TEXT("Gate presentation never moves Spline control points"),
			Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local).Equals(OriginalPoints[Index]));
	}

	// 과거 맵의 저장된 중앙 배치도 BeginPlay에서 최신 높이로 복구해야 한다.
	TInlineComponentArray<UChildActorComponent*> Children;
	Course->GetComponents(Children);
	for (UChildActorComponent* Child : Children)
	{
		if (const ADroneTrainingGate* Gate = Cast<ADroneTrainingGate>(Child->GetChildActor()))
		{
			Child->SetRelativeLocation(Spline->GetLocationAtDistanceAlongSpline(
				Gate->GetSegmentDistance(), ESplineCoordinateSpace::Local));
		}
	}
	if (!WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	CheckHeightFraction(1.0f / 6.0f);

	ADroneTrainingGate* Gate = Course->GetOrderedGates()[0];
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("Replacement test mesh loads"), Cube);
	UMaterialInterface* Guide = Course->GetCourseLineMaterial();
	TestNotNull(TEXT("Project Color-parameter material loads"), Guide);
	if (!Cube || !Guide) { return false; }
	UStaticMesh* TwoSlotMesh = DuplicateObject<UStaticMesh>(Cube, GetTransientPackage());
	TwoSlotMesh->GetStaticMaterials().Add(FStaticMaterial(Cube->GetMaterial(0)));
	const FTransform AssetTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(0.0f, 0.0f, 15.0f), FVector(0.25f, 4.0f, 4.0f));
	Gate->SetGateAssetMesh(TwoSlotMesh, AssetTransform);
	UStaticMeshComponent* AssetVisual = Gate->GetGateAssetVisual();
	TestTrue(TEXT("Complete Gate asset uses its own mesh slot"), AssetVisual->GetStaticMesh() == TwoSlotMesh);
	TestTrue(TEXT("Complete Gate asset preserves editable pivot/rotation/scale correction"), AssetVisual->GetRelativeTransform().Equals(AssetTransform));
	TestEqual(TEXT("Replacement mesh remains non-colliding"), AssetVisual->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TInlineComponentArray<UStaticMeshComponent*> AllVisuals;
	Gate->GetComponents(AllVisuals);
	int32 VisibleVisualCount = 0;
	for (const UStaticMeshComponent* Visual : AllVisuals)
	{
		VisibleVisualCount += Visual->IsVisible() && !Visual->bHiddenInGame ? 1 : 0;
	}
	TestEqual(TEXT("Replacement hides every Greybox frame bar"), VisibleVisualCount, 1);

	UMaterialInstanceConstant* StateMaterials[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		StateMaterials[Index] = NewObject<UMaterialInstanceConstant>(Gate);
		StateMaterials[Index]->SetParentEditorOnly(Guide);
	}
	Gate->SetGateStateMaterials(StateMaterials[0], StateMaterials[1], StateMaterials[2]);
	const EDroneTrainingGateVisualState States[] = {
		EDroneTrainingGateVisualState::Inactive, EDroneTrainingGateVisualState::Current, EDroneTrainingGateVisualState::Completed };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Gate->SetGateVisualState(States[Index]);
		UMaterialInstanceDynamic* ActualMaterial = Cast<UMaterialInstanceDynamic>(AssetVisual->GetMaterial(0));
		TestNotNull(TEXT("Replacement mesh receives a dynamic state material"), ActualMaterial);
		if (ActualMaterial)
		{
			TestTrue(TEXT("Before/current/after each use the explicitly selected material slot"), ActualMaterial->Parent == StateMaterials[Index]);
			FLinearColor Color;
			TestTrue(TEXT("Replacement state material exposes its Color parameter"),
				ActualMaterial->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), Color));
			TestTrue(TEXT("Replacement receives the matching state color"), Color.Equals(Gate->GetColorForVisualState(States[Index])));
			Gate->SetGateVisualState(States[Index]);
			TestTrue(TEXT("Repeated same-state updates reuse the material instance"), AssetVisual->GetMaterial(0) == ActualMaterial);
		}
		TestTrue(TEXT("Unselected material slot keeps the original mesh material"), AssetVisual->GetMaterial(1) == Cube->GetMaterial(0));
	}
	Gate->SetGateAssetMesh(nullptr, FTransform::Identity);
	TestTrue(TEXT("Clearing replacement slot hides asset visual"), AssetVisual->bHiddenInGame);
	Gate->SetGateAssetMesh(TwoSlotMesh, AssetTransform);

	// 음성 가청성은 수동 확인한다. 성공만 1회, Reset/오순서/역방향/중복은 피드백을 호출하지 않는다.
	USoundWave* VoiceSlotSound = NewObject<USoundWave>(Gate);
	Gate->SetGatePassSound(VoiceSlotSound);
	TestTrue(TEXT("Blueprint voice/SoundWave slot accepts a sound"), Gate->GetGatePassSound() == VoiceSlotSound);
	// 음원 데이터가 없는 시험 Wave는 재생하지 않는다. 실제 음원은 BP에서 지정한다.
	Gate->SetGatePassSound(nullptr);
	UDroneTrainingGateSequenceComponent* Sequence = Course->GetGateSequenceComponent();
	Sequence->ResetSequence();
	ADronePrototypePawn* Drone = World->SpawnActor<ADronePrototypePawn>(
		ADronePrototypePawn::StaticClass(), FTransform(FVector(-1000.0f, -1000.0f, 0.0f)), SpawnParameters);
	TestNotNull(TEXT("Feedback test Drone spawns"), Drone);
	if (!Drone) { return false; }
	const FVector Entry = Gate->GetActorLocation() - Gate->GetActorForwardVector() * 300.0f;
	const FVector Exit = Gate->GetActorLocation() + Gate->GetActorForwardVector() * 300.0f;
	Sequence->TryAcceptTraversal(Gate, Drone, Exit, Entry);
	Sequence->TryAcceptTraversal(Course->GetOrderedGates()[1], Drone, Entry, Exit);
	TestEqual(TEXT("Wrong direction/order and reset never trigger pass feedback"), Gate->GetAcceptedPassFeedbackCount(), 0);
	TestEqual(TEXT("Valid traversal is accepted"), Sequence->TryAcceptTraversal(Gate, Drone, Entry, Exit), EDroneTrainingGatePassResult::Accepted);
	TestEqual(TEXT("Accepted traversal triggers feedback once"), Gate->GetAcceptedPassFeedbackCount(), 1);
	Sequence->TryAcceptTraversal(Gate, Drone, Entry, Exit);
	TestEqual(TEXT("Duplicate traversal cannot replay sound/voice feedback"), Gate->GetAcceptedPassFeedbackCount(), 1);
	Sequence->ResetSequence();
	TestEqual(TEXT("Reset is silent"), Gate->GetAcceptedPassFeedbackCount(), 1);
	Sequence->TryAcceptTraversal(Gate, Drone, Entry, Exit);
	TestEqual(TEXT("A new lap allows a new pass sound"), Gate->GetAcceptedPassFeedbackCount(), 2);
	// Wrapper가 World Context를 없애기 전에 살아 있는 Child Actor를 정상 종료한다.
	Course->Destroy();
	WorldWrapper.TickTestWorld();
	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroneTrainingGateBlueprintTest,
	"Drone.Tutorial.TrainingGateBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingGateBlueprintTest::RunTest(const FString& Parameters)
{
	// Production 맵은 열지 않고 두 BP만 메모리에서 Compile한다. 패키지를 저장하지 않는다.
	const TCHAR* Paths[] = {
		TEXT("/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingGate.BP_DroneTrainingGate"),
		TEXT("/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingCourse.BP_DroneTrainingCourse") };
	for (const TCHAR* Path : Paths)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
		TestNotNull(Path, Blueprint);
		if (!Blueprint) { continue; }
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
		TestEqual(TEXT("Training Blueprint compiles without errors"), Results.NumErrors, 0);
		TestEqual(TEXT("Training Blueprint compiles without warnings"), Results.NumWarnings, 0);
		TestNotNull(TEXT("Training Blueprint generates a class"), Blueprint->GeneratedClass.Get());
		if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(ADroneTrainingGate::StaticClass()))
		{
			const ADroneTrainingGate* Defaults = Blueprint->GeneratedClass->GetDefaultObject<ADroneTrainingGate>();
			TestNotNull(TEXT("Gate BP inherits complete mesh component"), Defaults->GetGateAssetVisual());
			for (const FName Property : { FName(TEXT("GatePassSound")), FName(TEXT("InactiveMaterial")),
				FName(TEXT("CurrentMaterial")), FName(TEXT("CompletedMaterial")), FName(TEXT("GateAssetMesh")) })
			{
				const FProperty* Slot = Blueprint->GeneratedClass->FindPropertyByName(Property);
				TestTrue(*FString::Printf(TEXT("Gate BP exposes editable slot %s"), *Property.ToString()),
					Slot && Slot->HasAnyPropertyFlags(CPF_Edit));
			}
		}
	}
	return !HasAnyErrors();
}

#endif
