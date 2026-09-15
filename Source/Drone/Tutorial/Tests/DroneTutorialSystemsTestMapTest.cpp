#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/DroneDroppedPayload.h"
#include "Abilities/DroneRoleTestTarget.h"
#include "Prototype/DronePrototypeGameMode.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingGate.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"

namespace DroneTutorialSystemsTestMap
{
constexpr const TCHAR* MapObjectPath =
	TEXT("/Game/Drone/Maps/TestMap/Lvl_DroneTutorialSystemsTest.Lvl_DroneTutorialSystemsTest");
constexpr const TCHAR* CourseClassPath =
	TEXT("/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingCourse.BP_DroneTrainingCourse_C");
constexpr const TCHAR* GateClassPath =
	TEXT("/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingGate.BP_DroneTrainingGate_C");
constexpr const TCHAR* GameModeClassPath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
constexpr const TCHAR* ReconTargetClassPath =
	TEXT("/Game/Drone/Abilities/RoleTargets/BP_RoleTest_ReconTarget.BP_RoleTest_ReconTarget_C");
constexpr const TCHAR* ImpactTargetClassPath =
	TEXT("/Game/Drone/Abilities/RoleTargets/BP_RoleTest_ImpactTarget.BP_RoleTest_ImpactTarget_C");
constexpr const TCHAR* PayloadTargetClassPath =
	TEXT("/Game/Drone/Abilities/RoleTargets/BP_RoleTest_PayloadTarget.BP_RoleTest_PayloadTarget_C");
constexpr int32 ExpectedGateCount = 5;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTutorialSystemsTestMapTest,
	"Drone.Tutorial.TutorialSystemsTestMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTutorialSystemsTestMapTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = LoadObject<UWorld>(nullptr, DroneTutorialSystemsTestMap::MapObjectPath);
	TestNotNull(TEXT("Lightweight Tutorial Systems Test Map loads"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	UClass* CourseClass = LoadClass<ADroneTrainingCourse>(
		nullptr,
		DroneTutorialSystemsTestMap::CourseClassPath);
	UClass* GateClass = LoadClass<ADroneTrainingGate>(nullptr, DroneTutorialSystemsTestMap::GateClassPath);
	UClass* GameModeClass = LoadClass<ADronePrototypeGameMode>(
		nullptr,
		DroneTutorialSystemsTestMap::GameModeClassPath);
	UClass* ReconTargetClass = LoadClass<ADroneReconRoleTestTarget>(
		nullptr,
		DroneTutorialSystemsTestMap::ReconTargetClassPath);
	UClass* ImpactTargetClass = LoadClass<ADroneImpactRoleTestTarget>(
		nullptr,
		DroneTutorialSystemsTestMap::ImpactTargetClassPath);
	UClass* PayloadTargetClass = LoadClass<ADronePayloadRoleTestTarget>(
		nullptr,
		DroneTutorialSystemsTestMap::PayloadTargetClassPath);

	TestNotNull(TEXT("Course Blueprint Class loads"), CourseClass);
	TestNotNull(TEXT("Gate Blueprint Class loads"), GateClass);
	TestNotNull(TEXT("Prototype GameMode Blueprint Class loads"), GameModeClass);
	TestNotNull(TEXT("Recon target Blueprint Class loads"), ReconTargetClass);
	TestNotNull(TEXT("Impact target Blueprint Class loads"), ImpactTargetClass);
	TestNotNull(TEXT("Payload target Blueprint Class loads"), PayloadTargetClass);

	const AWorldSettings* WorldSettings = TestWorld->GetWorldSettings();
	TestNotNull(TEXT("Test Map has WorldSettings"), WorldSettings);
	if (WorldSettings && GameModeClass)
	{
		TestEqual(
			TEXT("Test Map uses Prototype GameMode override"),
			WorldSettings->DefaultGameMode.Get(),
			GameModeClass);
	}

	int32 PlayerStartCount = 0;
	int32 PrototypePawnCount = 0;
	int32 CourseCount = 0;
	int32 ReconTargetCount = 0;
	int32 ImpactTargetCount = 0;
	int32 PayloadTargetCount = 0;
	int32 CarryableCount = 0;
	int32 OwnedFloorCount = 0;
	int32 VisibleRoleTargetInstructionCount = 0;
	int32 VisibleCarryablePickupLabelCount = 0;
	ADroneTrainingCourse* Course = nullptr;

	for (TActorIterator<AActor> ActorIt(TestWorld); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor)
		{
			continue;
		}

		PlayerStartCount += Actor->IsA<APlayerStart>() ? 1 : 0;
		PrototypePawnCount += Actor->IsA<ADronePrototypePawn>() ? 1 : 0;
		if (ADroneTrainingCourse* Candidate = Cast<ADroneTrainingCourse>(Actor))
		{
			++CourseCount;
			Course = Candidate;
		}
		ReconTargetCount += ReconTargetClass && Actor->GetClass() == ReconTargetClass ? 1 : 0;
		ImpactTargetCount += ImpactTargetClass && Actor->GetClass() == ImpactTargetClass ? 1 : 0;
		PayloadTargetCount += PayloadTargetClass && Actor->GetClass() == PayloadTargetClass ? 1 : 0;
		if (const ADroneRoleTestTarget* RoleTarget = Cast<ADroneRoleTestTarget>(Actor))
		{
			const UTextRenderComponent* InstructionText = RoleTarget->GetInstructionText();
			TestNotNull(
				*FString::Printf(TEXT("%s owns its optional Instruction Text"), *RoleTarget->GetName()),
				InstructionText);
			VisibleRoleTargetInstructionCount += InstructionText && InstructionText->IsVisible() ? 1 : 0;
		}
		if (ADroneDroppedPayload* Carryable = Cast<ADroneDroppedPayload>(Actor))
		{
			if (Carryable->DoesStartAsCarryablePickup())
			{
				++CarryableCount;
				Carryable->ActivateCarryablePickup();
				const UTextRenderComponent* PickupLabel = Carryable->GetPickupLabel();
				TestNotNull(TEXT("Carryable Payload owns its optional Pickup Label"), PickupLabel);
				VisibleCarryablePickupLabelCount += PickupLabel && PickupLabel->IsVisible() ? 1 : 0;
			}
		}
#if WITH_EDITOR
		OwnedFloorCount += Actor->IsA<AStaticMeshActor>()
			&& Actor->GetActorLabel() == TEXT("TutorialSystemsTest_Floor")
			? 1
			: 0;
#endif

		const FString ClassPath = Actor->GetClass()->GetPathName();
		TestFalse(
			*FString::Printf(TEXT("%s does not use frozen template content"), *Actor->GetName()),
			ClassPath.StartsWith(TEXT("/Game/ThirdPerson")) || ClassPath.StartsWith(TEXT("/Game/Variant_")));
	}

	TestEqual(TEXT("Test Map has exactly one PlayerStart"), PlayerStartCount, 1);
	TestEqual(TEXT("Test Map has no pre-placed Drone Pawn"), PrototypePawnCount, 0);
	TestEqual(TEXT("Test Map has exactly one Course"), CourseCount, 1);
	TestEqual(TEXT("Test Map has one Recon target"), ReconTargetCount, 1);
	TestEqual(TEXT("Test Map has one Impact target"), ImpactTargetCount, 1);
	TestEqual(TEXT("Test Map has one Payload target"), PayloadTargetCount, 1);
	TestEqual(
		TEXT("Role test targets hide unsupported overhead instructions by default"),
		VisibleRoleTargetInstructionCount,
		0);
	TestEqual(
		TEXT("Carryable Payload hides unsupported overhead pickup text by default"),
		VisibleCarryablePickupLabelCount,
		0);
	TestEqual(TEXT("Test Map has one carryable Payload"), CarryableCount, 1);
	TestEqual(TEXT("Test Map has one owned floor"), OwnedFloorCount, 1);
	TestNotNull(TEXT("Test Map contains its Course"), Course);

	if (Course)
	{
		TestTrue(TEXT("Test Map uses Course Blueprint"), CourseClass && Course->GetClass() == CourseClass);
		Course->RerunConstructionScripts();
		TestTrue(TEXT("Test Map uses automatic Gates"), Course->IsUsingAutomaticSplineGates());
		TestTrue(
			TEXT("Test Map exposes independent Ring Handles"),
			Course->IsUsingIndependentAutomaticGateHandles());
		TestEqual(
			TEXT("Test Map resolves five independent Rings"),
			Course->GetResolvedAutomaticGateCount(),
			DroneTutorialSystemsTestMap::ExpectedGateCount);
		TestEqual(
			TEXT("Test Map generates five Ring Actors"),
			Course->GetGeneratedAutomaticGateCount(),
			DroneTutorialSystemsTestMap::ExpectedGateCount);

		const USplineComponent* Spline = Course->GetCourseSpline();
		TestNotNull(TEXT("Test Map Course owns its Spline"), Spline);
		TestTrue(TEXT("Test Map Course has a useful Spline length"), Spline && Spline->GetSplineLength() > 5000.0f);
		TestTrue(
			TEXT("Test Map Course line is distance-sampled into smooth segments"),
			Course->GetCourseLineSegmentCount() >= 50);

		const TArray<TObjectPtr<ADroneTrainingGate>>& OrderedGates = Course->GetOrderedGates();
		TestEqual(
			TEXT("Test Map Course owns every generated Gate in order"),
			OrderedGates.Num(),
			DroneTutorialSystemsTestMap::ExpectedGateCount);
		for (int32 GateIndex = 0; GateIndex < OrderedGates.Num(); ++GateIndex)
		{
			const ADroneTrainingGate* Gate = OrderedGates[GateIndex];
			TestNotNull(*FString::Printf(TEXT("Gate %d exists"), GateIndex), Gate);
			if (Gate)
			{
				TestTrue(
					*FString::Printf(TEXT("Gate %d uses BP_DroneTrainingGate"), GateIndex),
					GateClass && Gate->GetClass() == GateClass);
				TestEqual(
					*FString::Printf(TEXT("Gate %d mirrors sequence index"), GateIndex),
					Gate->GetGateIndex(),
					GateIndex);

				TInlineComponentArray<UStaticMeshComponent*> GateVisualSegments;
				Gate->GetComponents(GateVisualSegments);
				int32 VisibleFrameSegmentCount = 0;
				for (const UStaticMeshComponent* Segment : GateVisualSegments)
				{
					VisibleFrameSegmentCount += Segment
						&& Segment->IsVisible()
						&& !Segment->bHiddenInGame
						? 1
						: 0;
				}
				TestEqual(
					*FString::Printf(TEXT("Gate %d renders exactly four frame bars"), GateIndex),
					VisibleFrameSegmentCount,
					4);
			}
		}

		const UDroneTrainingGateSequenceComponent* Sequence = Course->GetGateSequenceComponent();
		TestNotNull(TEXT("Test Map Course owns its Gate Sequence"), Sequence);
		if (Sequence)
		{
			TestTrue(TEXT("Test Map Gate Sequence is valid"), Sequence->IsConfigurationValid());
			TestEqual(
				TEXT("Test Map Gate Sequence contains every Ring"),
				Sequence->GetConfiguredGateCount(),
				DroneTutorialSystemsTestMap::ExpectedGateCount);
		}
	}

	return !HasAnyErrors();
}

#endif
