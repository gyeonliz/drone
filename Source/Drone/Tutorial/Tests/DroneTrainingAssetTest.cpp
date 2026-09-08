#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/DroneDroppedPayload.h"
#include "Abilities/DroneRoleTestTarget.h"
#include "Prototype/DronePrototypeGameMode.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tutorial/DroneTrainingCourse.h"
#include "Tutorial/DroneTrainingGate.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

namespace DroneTrainingAssets
{
constexpr const TCHAR* CourseClassPath =
	TEXT("/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingCourse.BP_DroneTrainingCourse_C");
constexpr const TCHAR* GateClassPath =
	TEXT("/Game/Drone/Tutorial/Blueprints/BP_DroneTrainingGate.BP_DroneTrainingGate_C");
constexpr const TCHAR* GameModeClassPath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
constexpr const TCHAR* TrainingMapObjectPath =
	TEXT("/Game/Drone/Maps/Lvl_DroneTraining.Lvl_DroneTraining");
constexpr const TCHAR* GuideMaterialObjectPath =
	TEXT("/Game/Drone/Tutorial/Materials/M_DroneTrainingGuide.M_DroneTrainingGuide");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTrainingAssetTest,
	"Drone.Tutorial.TrainingAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneTrainingAssetTest::RunTest(const FString& Parameters)
{
	// 1) 실제 BP가 native 안전 규칙을 상속하는지 확인한다.
	UClass* CourseClass = LoadClass<ADroneTrainingCourse>(nullptr, DroneTrainingAssets::CourseClassPath);
	TestNotNull(TEXT("BP_DroneTrainingCourse generated Class loads"), CourseClass);
	if (CourseClass)
	{
		TestTrue(
			TEXT("BP_DroneTrainingCourse derives from the native Training Course"),
			CourseClass->IsChildOf(ADroneTrainingCourse::StaticClass()));
	}

	UClass* GameModeClass = LoadClass<ADronePrototypeGameMode>(nullptr, DroneTrainingAssets::GameModeClassPath);
	TestNotNull(TEXT("Prototype BP GameMode used by the Training Map loads"), GameModeClass);

	UClass* GateClass = LoadClass<ADroneTrainingGate>(nullptr, DroneTrainingAssets::GateClassPath);
	TestNotNull(TEXT("BP_DroneTrainingGate generated Class loads"), GateClass);
	if (GateClass)
	{
		TestTrue(
			TEXT("BP_DroneTrainingGate derives from the native Training Gate"),
			GateClass->IsChildOf(ADroneTrainingGate::StaticClass()));
	}

	UMaterialInterface* GuideMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		DroneTrainingAssets::GuideMaterialObjectPath);
	TestNotNull(TEXT("M_DroneTrainingGuide Material loads"), GuideMaterial);
	if (GuideMaterial)
	{
		const UMaterial* BaseMaterial = GuideMaterial->GetMaterial();
		TestNotNull(TEXT("Guide Material has a base Material"), BaseMaterial);
		TestTrue(
			TEXT("Guide Material compiles SplineMesh shaders"),
			BaseMaterial && BaseMaterial->GetUsageByFlag(MATUSAGE_SplineMesh));
		TestTrue(
			TEXT("Guide Material is Unlit"),
			BaseMaterial && BaseMaterial->GetShadingModels().HasShadingModel(MSM_Unlit));
		TestEqual(TEXT("Guide Material is opaque"), GuideMaterial->GetBlendMode(), BLEND_Opaque);
	}

	// 2) 정확한 Map 경로와 WorldSettings 계약을 검사해 전역 ThirdPerson 설정 의존을 막는다.
	UWorld* TrainingWorld = LoadObject<UWorld>(nullptr, DroneTrainingAssets::TrainingMapObjectPath);
	TestNotNull(TEXT("Training Map loads from the central Drone Maps path"), TrainingWorld);
	if (!TrainingWorld)
	{
		return false;
	}

	const AWorldSettings* WorldSettings = TrainingWorld->GetWorldSettings();
	TestNotNull(TEXT("Training Map has WorldSettings"), WorldSettings);
	if (WorldSettings && GameModeClass)
	{
		TestEqual(
			TEXT("Training Map overrides GameMode with BP_DronePrototypeGameMode"),
			WorldSettings->DefaultGameMode.Get(),
			GameModeClass);
	}

	int32 PlayerStartCount = 0;
	int32 PlacedPrototypePawnCount = 0;
	int32 CourseCount = 0;
	int32 GateCount = 0;
	int32 ReconRoleTargetCount = 0;
	int32 ImpactRoleTargetCount = 0;
	int32 PayloadRoleTargetCount = 0;
	int32 CarryablePayloadCount = 0;
	ADroneTrainingCourse* PlacedCourse = nullptr;
	TArray<ADroneTrainingGate*> PlacedGates;
	for (TActorIterator<AActor> ActorIt(TrainingWorld); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor)
		{
			continue;
		}

		PlayerStartCount += Actor->IsA<APlayerStart>() ? 1 : 0;
		PlacedPrototypePawnCount += Actor->IsA<ADronePrototypePawn>() ? 1 : 0;
		if (Actor->IsA<ADroneTrainingCourse>())
		{
			++CourseCount;
			PlacedCourse = Cast<ADroneTrainingCourse>(Actor);
		}
		if (ADroneTrainingGate* Gate = Cast<ADroneTrainingGate>(Actor))
		{
			++GateCount;
			PlacedGates.Add(Gate);
		}
		ReconRoleTargetCount += Actor->IsA<ADroneReconRoleTestTarget>() ? 1 : 0;
		ImpactRoleTargetCount += Actor->IsA<ADroneImpactRoleTestTarget>() ? 1 : 0;
		PayloadRoleTargetCount += Actor->IsA<ADronePayloadRoleTestTarget>() ? 1 : 0;
		if (const ADroneDroppedPayload* CarryablePayload = Cast<ADroneDroppedPayload>(Actor))
		{
			CarryablePayloadCount += CarryablePayload->DoesStartAsCarryablePickup() ? 1 : 0;
		}

		// 새 Map에 기존 Template/Variant Actor를 직접 배치하는 회귀를 잡는다.
		const FString ActorClassPath = Actor->GetClass()->GetPathName();
		TestFalse(
			*FString::Printf(TEXT("%s is not a placed ThirdPerson/Variant Actor"), *Actor->GetName()),
			ActorClassPath.StartsWith(TEXT("/Game/ThirdPerson"))
				|| ActorClassPath.StartsWith(TEXT("/Game/Variant_")));
	}

	TestEqual(TEXT("Training Map has exactly one PlayerStart"), PlayerStartCount, 1);
	TestEqual(TEXT("Training Map has no pre-placed Prototype Pawn"), PlacedPrototypePawnCount, 0);
	TestEqual(TEXT("Training Map has exactly one Training Course"), CourseCount, 1);
	TestEqual(TEXT("Training Map has exactly four Greybox Gates"), GateCount, 4);
	TestEqual(TEXT("Training Map has one Recon role test target"), ReconRoleTargetCount, 1);
	TestEqual(TEXT("Training Map has one Impact role test target"), ImpactRoleTargetCount, 1);
	TestEqual(TEXT("Training Map has one Payload role test target"), PayloadRoleTargetCount, 1);
	TestEqual(TEXT("Training Map has one placeable carryable Payload"), CarryablePayloadCount, 1);
	TestNotNull(TEXT("Training Map contains a Training Course instance"), PlacedCourse);

	if (PlacedCourse)
	{
		TestTrue(TEXT("Placed Course uses BP_DroneTrainingCourse"), PlacedCourse->GetClass() == CourseClass);
		TestTrue(
			TEXT("Placed Course uses M_DroneTrainingGuide"),
			PlacedCourse->GetCourseLineMaterial() == GuideMaterial);
		// 로드 직후 transient Sequence 상태와 표시 Segment를 실제 Construction 경로로 복원한다.
		PlacedCourse->RerunConstructionScripts();

		// TUT-02는 Level Actor 검색이 아니라 Course의 명시적 배열을 순서 기준으로 사용한다.
		const TArray<TObjectPtr<ADroneTrainingGate>>& OrderedGates = PlacedCourse->GetOrderedGates();
		TestEqual(TEXT("Placed Course explicitly references all four Gates"), OrderedGates.Num(), 4);
		TSet<const ADroneTrainingGate*> UniqueOrderedGates;
		for (int32 GatePosition = 0; GatePosition < OrderedGates.Num(); ++GatePosition)
		{
			ADroneTrainingGate* Gate = OrderedGates[GatePosition];
			TestNotNull(*FString::Printf(TEXT("Ordered Gate %d exists"), GatePosition), Gate);
			if (!Gate)
			{
				continue;
			}

			TestTrue(
				*FString::Printf(TEXT("Ordered Gate %d is uniquely referenced"), GatePosition),
				!UniqueOrderedGates.Contains(Gate));
			UniqueOrderedGates.Add(Gate);
			TestTrue(
				*FString::Printf(TEXT("Ordered Gate %d is placed in the loaded Map"), GatePosition),
				PlacedGates.Contains(Gate));
			if (GateClass)
			{
				TestTrue(
					*FString::Printf(TEXT("Ordered Gate %d uses BP_DroneTrainingGate"), GatePosition),
					Gate->GetClass() == GateClass);
			}
			TestEqual(
				*FString::Printf(TEXT("Ordered Gate %d shares the CourseId"), GatePosition),
				Gate->GetCourseId(),
				PlacedCourse->GetCourseId());
			TestEqual(
				*FString::Printf(TEXT("Ordered Gate %d mirrors its array position"), GatePosition),
				Gate->GetGateIndex(),
				GatePosition);
			TestTrue(
				*FString::Printf(TEXT("Ordered Gate %d stores non-negative SegmentDistance metadata"), GatePosition),
				Gate->GetSegmentDistance() >= 0.0f);
		}

		UDroneTrainingGateSequenceComponent* GateSequence = PlacedCourse->GetGateSequenceComponent();
		UDroneTrainingLapRecorderComponent* LapRecorder = PlacedCourse->GetLapRecorderComponent();
		TestNotNull(TEXT("Placed Course owns a Gate Sequence Component"), GateSequence);
		TestNotNull(TEXT("Placed Course owns a Lap Recorder Component"), LapRecorder);
		if (LapRecorder)
		{
			TestFalse(TEXT("Lap Recorder adds no per-frame Tick"), LapRecorder->PrimaryComponentTick.bCanEverTick);
			TestFalse(TEXT("Loaded Map has no in-progress Lap"), LapRecorder->IsLapRecording());
		}
		if (GateSequence)
		{
			TestTrue(TEXT("Placed Gate configuration is valid"), GateSequence->IsConfigurationValid());
			TestEqual(TEXT("Placed Gate Sequence contains four Gates"), GateSequence->GetConfiguredGateCount(), 4);
			TestEqual(TEXT("Placed Gate Sequence starts at Gate 0"), GateSequence->GetCurrentGateIndex(), 0);
		}

		// 로드된 Map Actor의 전체 Primitive 안전 계약을 검사한다.
		const USplineComponent* CourseSpline = PlacedCourse->GetCourseSpline();
		TestNotNull(TEXT("Placed Course owns its Spline"), CourseSpline);
		const int32 ExpectedSegmentCount = PlacedCourse->GetExpectedCourseLineSegmentCount();
		TestTrue(TEXT("Placed Course Spline has a positive length"), CourseSpline && CourseSpline->GetSplineLength() > 0.0f);
		TestEqual(
			TEXT("Placed Course creates every distance-sampled guide Segment"),
			PlacedCourse->GetCourseLineSegmentCount(),
			ExpectedSegmentCount);

		TInlineComponentArray<UPrimitiveComponent*> CoursePrimitives;
		PlacedCourse->GetComponents(CoursePrimitives);
		for (UPrimitiveComponent* Primitive : CoursePrimitives)
		{
			if (!Primitive)
			{
				continue;
			}

			TestEqual(
				*FString::Printf(TEXT("Map component %s has no collision"), *Primitive->GetName()),
				Primitive->GetCollisionEnabled(),
				ECollisionEnabled::NoCollision);
			TestFalse(
				*FString::Printf(TEXT("Map component %s creates no overlaps"), *Primitive->GetName()),
				Primitive->GetGenerateOverlapEvents());
			TestFalse(
				*FString::Printf(TEXT("Map component %s cannot affect Navigation"), *Primitive->GetName()),
				Primitive->CanEverAffectNavigation());

			if (const USplineMeshComponent* Segment = Cast<USplineMeshComponent>(Primitive);
				Segment && Segment->ComponentHasTag(ADroneTrainingCourse::GetGeneratedSegmentTag()))
			{
				TestNotNull(TEXT("Map guide Segment has a runtime Mesh"), Segment->GetStaticMesh().Get());
				const UMaterialInterface* SegmentMaterial = Segment->GetMaterial(0);
				TestNotNull(TEXT("Map guide Segment has a Material"), SegmentMaterial);
				TestTrue(
					TEXT("Map guide Segment resolves to M_DroneTrainingGuide"),
					SegmentMaterial && SegmentMaterial->GetMaterial() == GuideMaterial);
				TestTrue(TEXT("Map guide Segment is runtime visible"), Segment->IsVisible() && !Segment->bHiddenInGame);
			}
		}
	}

	// Gate Visual과 Trigger가 서로 다른 Collision 계약을 유지하는지 실제 Map Actor에서 검사한다.
	for (ADroneTrainingGate* Gate : PlacedGates)
	{
		if (!Gate)
		{
			continue;
		}

		TestTrue(TEXT("Placed Gate Actor collision stays enabled for its Trigger"), Gate->GetActorEnableCollision());
		UBoxComponent* Trigger = Gate->GetGateTrigger();
		TestNotNull(TEXT("Placed Gate owns its Box Trigger"), Trigger);
		if (Trigger)
		{
			TestEqual(TEXT("Placed Gate Trigger is QueryOnly"), Trigger->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
			TestTrue(TEXT("Placed Gate Trigger generates overlaps"), Trigger->GetGenerateOverlapEvents());
			TestEqual(TEXT("Placed Gate Trigger overlaps Pawn"), Trigger->GetCollisionResponseToChannel(ECC_Pawn), ECR_Overlap);
			TestFalse(TEXT("Placed Gate Trigger cannot affect Navigation"), Trigger->CanEverAffectNavigation());
		}

		TInlineComponentArray<UStaticMeshComponent*> RingSegments;
		Gate->GetComponents(RingSegments);
		TestEqual(TEXT("Placed Gate has sixteen Ring segments"), RingSegments.Num(), 16);
		for (UStaticMeshComponent* Segment : RingSegments)
		{
			if (!Segment)
			{
				continue;
			}

			TestNotNull(TEXT("Placed Ring segment has a Mesh"), Segment->GetStaticMesh().Get());
			TestEqual(TEXT("Placed Ring segment has no collision"), Segment->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Placed Ring segment creates no overlaps"), Segment->GetGenerateOverlapEvents());
			TestFalse(TEXT("Placed Ring segment cannot affect Navigation"), Segment->CanEverAffectNavigation());
			TestTrue(TEXT("Placed Ring segment is visible"), Segment->IsVisible() && !Segment->bHiddenInGame);
		}
	}

	return !HasAnyErrors();
}

#endif
