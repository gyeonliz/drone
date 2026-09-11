#include "Tutorial/DroneTrainingCourse.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Tutorial/DroneTrainingGate.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"
#include "Tutorial/DroneTrainingLapRecorderComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace DroneTrainingCourse
{
const FName GeneratedSegmentTag(TEXT("DroneTrainingCourse.GeneratedLineSegment"));
const FName GeneratedGateComponentTag(TEXT("DroneTrainingCourse.GeneratedSplineGate"));

// Spline 점을 지나치게 많이 만들었을 때 Editor가 멈추는 실수를 방지하는 안전 상한이다.
constexpr int32 MaximumGeneratedSegmentCount = 256;
constexpr int32 MaximumGeneratedGateCount = 64;
}

ADroneTrainingCourse::ADroneTrainingCourse()
{
	// Course는 정적인 경로 데이터이므로 매 Frame Tick하지 않는다.
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);

	CourseRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CourseRoot"));
	CourseRoot->SetMobility(EComponentMobility::Static);
	CourseRoot->SetCanEverAffectNavigation(false);
	SetRootComponent(CourseRoot);

	CourseSpline = CreateDefaultSubobject<USplineComponent>(TEXT("CourseSpline"));
	CourseSpline->SetupAttachment(CourseRoot);
	CourseSpline->SetMobility(EComponentMobility::Static);
	CourseSpline->SetClosedLoop(false);
	CourseSpline->SetDrawDebug(true);
	CourseSpline->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CourseSpline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CourseSpline->SetGenerateOverlapEvents(false);
	CourseSpline->SetCanEverAffectNavigation(false);

	// Gate 순서 상태는 Primitive가 아니므로 Course의 비간섭 규칙과 충돌하지 않는다.
	GateSequenceComponent = CreateDefaultSubobject<UDroneTrainingGateSequenceComponent>(TEXT("GateSequenceComponent"));
	LapRecorderComponent = CreateDefaultSubobject<UDroneTrainingLapRecorderComponent>(TEXT("LapRecorderComponent"));
	AutomaticGateClass = ADroneTrainingGate::StaticClass();

	// 처음 Map에 배치하자마자 비행 가능한 S자형 Greybox 경로가 보이게 한다.
	// 이 좌표는 최종 코스가 아니며 Level/BP Viewport에서 자유롭게 수정한다.
	const TArray<FVector> InitialCoursePoints = {
		FVector(0.0f, 0.0f, 250.0f),
		FVector(1200.0f, 0.0f, 350.0f),
		FVector(2400.0f, 700.0f, 500.0f),
		FVector(3600.0f, -700.0f, 400.0f),
		FVector(5000.0f, 0.0f, 300.0f)
	};
	CourseSpline->SetSplinePoints(InitialCoursePoints, ESplineCoordinateSpace::Local, true);

	// 구매 에셋 전에도 재현할 수 있도록 표시 Mesh는 Engine 기본 Cube를 사용한다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		CourseLineMesh = CubeMeshFinder.Object;
	}

	// SplineMesh 사용 Flag가 저장된 프로젝트 전용 Unlit 발광 재질을 사용한다.
	// Flag가 없는 일반 Material은 런타임에서 World 기본 Material로 교체되므로 경로를 임의로 바꾸지 않는다.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GuideMaterialFinder(
		TEXT("/Game/Drone/Tutorial/Materials/M_DroneTrainingGuide.M_DroneTrainingGuide"));
	if (GuideMaterialFinder.Succeeded())
	{
		CourseLineMaterial = GuideMaterialFinder.Object;
	}
}

void ADroneTrainingCourse::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildAutomaticGateComponents();
	ApplyNonInterferenceRules();
	RebuildCourseLineSegments();
	ConfigureGateSequence();
}

void ADroneTrainingCourse::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// BeginPlay의 BP Event나 초기 Overlap보다 먼저 정상 Gate Event를 받을 준비를 끝낸다.
	if (GetWorld() && GetWorld()->IsGameWorld() && LapRecorderComponent)
	{
		LapRecorderComponent->InitializeRecorder(GateSequenceComponent);
	}
}

void ADroneTrainingCourse::BeginPlay()
{
	Super::BeginPlay();

	// BP/Level 직렬화 값이나 Cook 방식이 달라도 런타임 비간섭 계약을 다시 고정한다.
	ApplyNonInterferenceRules();
	RebuildCourseLineSegments();
	RefreshActiveOrderedGates();
	if (bUseAutomaticSplineGates && ActiveOrderedGates.IsEmpty())
	{
		// PIE World 복제 방식에 따라 Construction 생성 Component가 복사되지 않을 수 있다.
		// Recorder는 PostInitializeComponents에서 이미 Event를 구독하므로 여기서 복구해도 안전하다.
		RebuildAutomaticGateComponents();
	}
	ConfigureGateSequence();
}

void ADroneTrainingCourse::ConfigureOrderedGates(const TArray<ADroneTrainingGate*>& InOrderedGates)
{
	OrderedGates.Reset(InOrderedGates.Num());
	for (ADroneTrainingGate* Gate : InOrderedGates)
	{
		OrderedGates.Add(Gate);
	}

	RefreshActiveOrderedGates();
	ConfigureGateSequence();
}

void ADroneTrainingCourse::SynchronizeGateDefinitions()
{
	RefreshActiveOrderedGates();
	for (int32 GatePosition = 0; GatePosition < ActiveOrderedGates.Num(); ++GatePosition)
	{
		ADroneTrainingGate* Gate = ActiveOrderedGates[GatePosition];
		if (IsValid(Gate))
		{
			// 배열만 정확히 구성하면 CourseId/GateIndex의 이중 수동 입력이 필요하지 않다.
			Gate->ConfigureGateDefinition(CourseId, GatePosition, Gate->GetSegmentDistance());
		}
	}

	ConfigureGateSequence();
}

void ADroneTrainingCourse::RebuildAutomaticGates()
{
	RebuildAutomaticGateComponents();
	ConfigureGateSequence();
}

void ADroneTrainingCourse::ConfigureAutomaticGateLayout(
	const bool bEnabled,
	const int32 GateCount,
	const bool bEvenlyDistribute,
	const float StartDistanceCentimeters,
	const float SpacingCentimeters,
	const float EndPaddingCentimeters)
{
	bUseAutomaticSplineGates = bEnabled;
	// 이 API는 개수/간격 기반 배치를 명시하므로 Point 직접 편집 모드를 해제한다.
	bUseSplinePointsAsAutomaticGatePositions = false;
	AutomaticGateCount = FMath::Clamp(GateCount, 2, DroneTrainingCourse::MaximumGeneratedGateCount);
	bEvenlyDistributeAutomaticGates = bEvenlyDistribute;
	AutomaticGateStartDistanceCentimeters = FMath::Max(StartDistanceCentimeters, 0.0f);
	AutomaticGateSpacingCentimeters = FMath::Max(SpacingCentimeters, 1.0f);
	AutomaticGateEndPaddingCentimeters = FMath::Max(EndPaddingCentimeters, 0.0f);
	RebuildAutomaticGates();
}

void ADroneTrainingCourse::ConfigureAutomaticGateOverrides(
	const TArray<float>& SplineDistancesCentimeters,
	const TArray<FVector>& LocalOffsets)
{
	AutomaticGateSplineDistancesCentimeters = SplineDistancesCentimeters;
	AutomaticGateLocalOffsets = LocalOffsets;
	RebuildAutomaticGates();
}

void ADroneTrainingCourse::ConfigureAutomaticGateSplinePointPlacement(const bool bEnabled)
{
	bUseSplinePointsAsAutomaticGatePositions = bEnabled;
	if (bEnabled)
	{
		bUseAutomaticSplineGates = true;
	}
	RebuildAutomaticGates();
}

int32 ADroneTrainingCourse::GetResolvedAutomaticGateCount() const
{
	if (bUseSplinePointsAsAutomaticGatePositions)
	{
		return CourseSpline
			? FMath::Min(CourseSpline->GetNumberOfSplinePoints(), DroneTrainingCourse::MaximumGeneratedGateCount)
			: 0;
	}

	return FMath::Clamp(
		AutomaticGateCount,
		2,
		DroneTrainingCourse::MaximumGeneratedGateCount);
}

int32 ADroneTrainingCourse::GetGeneratedAutomaticGateCount() const
{
	int32 ValidGateCount = 0;
	TInlineComponentArray<UChildActorComponent*> ChildActorComponents;
	GetComponents(ChildActorComponents);
	for (const UChildActorComponent* GateComponent : ChildActorComponents)
	{
		if (GateComponent
			&& GateComponent->ComponentHasTag(DroneTrainingCourse::GeneratedGateComponentTag)
			&& IsValid(Cast<ADroneTrainingGate>(GateComponent->GetChildActor())))
		{
			++ValidGateCount;
		}
	}
	return ValidGateCount;
}

float ADroneTrainingCourse::GetAutomaticGateDistanceAlongSpline(const int32 GateIndex) const
{
	if (!CourseSpline || GateIndex < 0)
	{
		return 0.0f;
	}

	const float SplineLength = FMath::Max(CourseSpline->GetSplineLength(), 0.0f);
	if (SplineLength <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const int32 SafeGateCount = GetResolvedAutomaticGateCount();
	if (SafeGateCount <= 0)
	{
		return 0.0f;
	}
	const int32 SafeGateIndex = FMath::Clamp(GateIndex, 0, SafeGateCount - 1);
	const float SafeStartDistance = FMath::Clamp(
		AutomaticGateStartDistanceCentimeters,
		0.0f,
		SplineLength);

	const bool bHasSplineDistanceOverride =
		AutomaticGateSplineDistancesCentimeters.IsValidIndex(SafeGateIndex)
		&& FMath::IsFinite(AutomaticGateSplineDistancesCentimeters[SafeGateIndex])
		&& AutomaticGateSplineDistancesCentimeters[SafeGateIndex] >= 0.0f;

	float BaseDistance = SafeStartDistance;
	if (bUseSplinePointsAsAutomaticGatePositions)
	{
		// 제어점의 누적 Spline 거리를 사용해 Ring이 곡선 위에 정확히 놓이도록 한다.
		BaseDistance = CourseSpline->GetDistanceAlongSplineAtSplinePoint(SafeGateIndex);
	}
	else if (bHasSplineDistanceOverride)
	{
		BaseDistance = AutomaticGateSplineDistancesCentimeters[SafeGateIndex];
	}
	else if (bEvenlyDistributeAutomaticGates)
	{
		const float LastGateDistance = FMath::Max(
			SafeStartDistance,
			SplineLength - FMath::Max(AutomaticGateEndPaddingCentimeters, 0.0f));
		const float Alpha = static_cast<float>(SafeGateIndex) / static_cast<float>(SafeGateCount - 1);
		BaseDistance = FMath::Lerp(SafeStartDistance, LastGateDistance, Alpha);
	}
	else
	{
		BaseDistance = SafeStartDistance
			+ FMath::Max(AutomaticGateSpacingCentimeters, 1.0f) * static_cast<float>(SafeGateIndex);
	}

	const float PerGateOffset = AutomaticGateDistanceOffsetsCentimeters.IsValidIndex(SafeGateIndex)
		? AutomaticGateDistanceOffsetsCentimeters[SafeGateIndex]
		: 0.0f;
	return FMath::Clamp(
		BaseDistance + AutomaticGateDistanceOffsetCentimeters + PerGateOffset,
		0.0f,
		SplineLength);
}

int32 ADroneTrainingCourse::GetCourseLineSegmentCount() const
{
	TInlineComponentArray<USplineMeshComponent*> SplineMeshComponents;
	GetComponents(SplineMeshComponents);

	int32 GeneratedSegmentCount = 0;
	for (const USplineMeshComponent* SplineMeshComponent : SplineMeshComponents)
	{
		if (SplineMeshComponent
			&& SplineMeshComponent->ComponentHasTag(DroneTrainingCourse::GeneratedSegmentTag))
		{
			++GeneratedSegmentCount;
		}
	}

	return GeneratedSegmentCount;
}

int32 ADroneTrainingCourse::GetExpectedCourseLineSegmentCount() const
{
	if (!CourseSpline || CourseSpline->GetNumberOfSplinePoints() < 2)
	{
		return 0;
	}

	const float SplineLength = CourseSpline->GetSplineLength();
	if (!FMath::IsFinite(SplineLength) || SplineLength <= UE_SMALL_NUMBER)
	{
		return 0;
	}

	const float TargetSegmentLength = FMath::Max(CourseLineSegmentLengthCentimeters, 25.0f);
	const int32 RequestedSegmentCount = FMath::Max(1, FMath::CeilToInt(SplineLength / TargetSegmentLength));
	return FMath::Min(RequestedSegmentCount, DroneTrainingCourse::MaximumGeneratedSegmentCount);
}

FName ADroneTrainingCourse::GetGeneratedSegmentTag()
{
	return DroneTrainingCourse::GeneratedSegmentTag;
}

FName ADroneTrainingCourse::GetGeneratedGateComponentTag()
{
	return DroneTrainingCourse::GeneratedGateComponentTag;
}

void ADroneTrainingCourse::ApplyNonInterferenceRules()
{
	// 생성자 뒤에 BP CDO와 Level Instance 값이 역직렬화될 수 있으므로 매 Construction/Play 때 복원한다.
	SetActorEnableCollision(false);
	if (CourseRoot)
	{
		CourseRoot->SetCanEverAffectNavigation(false);
	}

	// Course에 나중에 표시용 Primitive가 추가되더라도 모두 같은 안전 계약을 따르게 한다.
	TInlineComponentArray<UPrimitiveComponent*> CoursePrimitives;
	GetComponents(CoursePrimitives);
	for (UPrimitiveComponent* Primitive : CoursePrimitives)
	{
		if (!Primitive)
		{
			continue;
		}

		Primitive->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetGenerateOverlapEvents(false);
		Primitive->SetSimulatePhysics(false);
		Primitive->SetCanEverAffectNavigation(false);
	}
}

void ADroneTrainingCourse::ConfigureGateSequence()
{
	if (GateSequenceComponent)
	{
		RefreshActiveOrderedGates();
		// 자동/수동 중 선택된 ActiveOrderedGates만 순서의 원본으로 사용한다.
		// GateIndex를 모두 0으로 남기는 흔한 실수도 Construction/Play 때 자동 복구한다.
		for (int32 GatePosition = 0; GatePosition < ActiveOrderedGates.Num(); ++GatePosition)
		{
			ADroneTrainingGate* Gate = ActiveOrderedGates[GatePosition];
			if (IsValid(Gate))
			{
				Gate->ConfigureGateDefinition(CourseId, GatePosition, Gate->GetSegmentDistance());
			}
		}
		GateSequenceComponent->ConfigureSequence(CourseId, ActiveOrderedGates);
	}
}

void ADroneTrainingCourse::DestroyGeneratedAutomaticGateComponents()
{
	TInlineComponentArray<UChildActorComponent*> ExistingChildActorComponents;
	GetComponents(ExistingChildActorComponents);
	for (UChildActorComponent* ExistingComponent : ExistingChildActorComponents)
	{
		if (!ExistingComponent
			|| !ExistingComponent->ComponentHasTag(DroneTrainingCourse::GeneratedGateComponentTag))
		{
			continue;
		}

		BlueprintCreatedComponents.Remove(ExistingComponent);
		ExistingComponent->DestroyComponent();
	}
	GeneratedAutomaticGateComponents.Reset();
}

void ADroneTrainingCourse::RebuildAutomaticGateComponents()
{
	DestroyGeneratedAutomaticGateComponents();
	ActiveOrderedGates.Reset();

	if (!bUseAutomaticSplineGates || !CourseSpline || !AutomaticGateClass)
	{
		RefreshActiveOrderedGates();
		return;
	}

	const int32 GateCount = GetResolvedAutomaticGateCount();
	// Tutorial Lap에는 시작/종료 Ring이 모두 필요하다. Point를 실수로 하나만 남기면 생성하지 않는다.
	if (GateCount < 2)
	{
		RefreshActiveOrderedGates();
		return;
	}
	GeneratedAutomaticGateComponents.Reserve(GateCount);
	ActiveOrderedGates.Reserve(GateCount);

	for (int32 GateIndex = 0; GateIndex < GateCount; ++GateIndex)
	{
		const float DistanceAlongSpline = GetAutomaticGateDistanceAlongSpline(GateIndex);
		const FVector SplineLocation = CourseSpline->GetLocationAtDistanceAlongSpline(
			DistanceAlongSpline,
			ESplineCoordinateSpace::Local);
		const FQuat SplineRotation = CourseSpline->GetQuaternionAtDistanceAlongSpline(
			DistanceAlongSpline,
			ESplineCoordinateSpace::Local);
		const FVector PerGateLocalOffset = AutomaticGateLocalOffsets.IsValidIndex(GateIndex)
			? AutomaticGateLocalOffsets[GateIndex]
			: FVector::ZeroVector;
		const FVector GateLocation = SplineLocation
			+ SplineRotation.RotateVector(AutomaticGateLocalOffset + PerGateLocalOffset);
		const FQuat GateRotation = SplineRotation * AutomaticGateRotationOffset.Quaternion();
		const FVector GateScale(
			FMath::Max(FMath::Abs(AutomaticGateScale.X), 0.01f),
			FMath::Max(FMath::Abs(AutomaticGateScale.Y), 0.01f),
			FMath::Max(FMath::Abs(AutomaticGateScale.Z), 0.01f));

		const FName ComponentName = MakeUniqueObjectName(
			this,
			UChildActorComponent::StaticClass(),
			*FString::Printf(TEXT("AutomaticSplineGate_%02d"), GateIndex));
		UChildActorComponent* GateComponent = NewObject<UChildActorComponent>(
			this,
			ComponentName,
			RF_Transactional);
		if (!GateComponent)
		{
			continue;
		}

		PostCreateBlueprintComponent(GateComponent);
		GateComponent->ComponentTags.Add(DroneTrainingCourse::GeneratedGateComponentTag);
		GateComponent->SetupAttachment(CourseSpline);
		GateComponent->SetMobility(EComponentMobility::Static);
		GateComponent->SetRelativeTransform(FTransform(GateRotation, GateLocation, GateScale));
		GateComponent->SetChildActorOwnerOnCreation(true);
		GateComponent->SetChildActorName(*FString::Printf(TEXT("TrainingGate_%02d"), GateIndex));
		GateComponent->SetChildActorClass(AutomaticGateClass);
		GateComponent->RegisterComponent();

		ADroneTrainingGate* GeneratedGate = Cast<ADroneTrainingGate>(GateComponent->GetChildActor());
		if (!IsValid(GeneratedGate))
		{
			BlueprintCreatedComponents.Remove(GateComponent);
			GateComponent->DestroyComponent();
			continue;
		}

		GeneratedGate->ConfigureGateDefinition(CourseId, GateIndex, DistanceAlongSpline);
		GeneratedAutomaticGateComponents.Add(GateComponent);
		ActiveOrderedGates.Add(GeneratedGate);
	}
}

void ADroneTrainingCourse::RefreshActiveOrderedGates()
{
	ActiveOrderedGates.Reset();
	if (bUseAutomaticSplineGates)
	{
		// GeneratedAutomaticGateComponents는 Transient라 PIE World 복제에서 비어 있을 수 있다.
		// Tag로 실제 소유 Component를 다시 찾아 저장 Map, PIE, Runtime Spawn이 같은 경로를 사용한다.
		GeneratedAutomaticGateComponents.Reset();
		TInlineComponentArray<UChildActorComponent*> ChildActorComponents;
		GetComponents(ChildActorComponents);
		for (UChildActorComponent* GateComponent : ChildActorComponents)
		{
			if (GateComponent
				&& GateComponent->ComponentHasTag(DroneTrainingCourse::GeneratedGateComponentTag))
			{
				GeneratedAutomaticGateComponents.Add(GateComponent);
			}
		}
		GeneratedAutomaticGateComponents.Sort(
			[](const UChildActorComponent& Left, const UChildActorComponent& Right)
			{
				const ADroneTrainingGate* LeftGate = Cast<ADroneTrainingGate>(Left.GetChildActor());
				const ADroneTrainingGate* RightGate = Cast<ADroneTrainingGate>(Right.GetChildActor());
				if (LeftGate && RightGate && LeftGate->GetGateIndex() != RightGate->GetGateIndex())
				{
					return LeftGate->GetGateIndex() < RightGate->GetGateIndex();
				}
				return Left.GetName().Compare(Right.GetName()) < 0;
			});

		for (UChildActorComponent* GateComponent : GeneratedAutomaticGateComponents)
		{
			if (GateComponent)
			{
				if (ADroneTrainingGate* Gate = Cast<ADroneTrainingGate>(GateComponent->GetChildActor()); IsValid(Gate))
				{
					ActiveOrderedGates.Add(Gate);
				}
			}
		}
		return;
	}

	for (ADroneTrainingGate* Gate : OrderedGates)
	{
		if (IsValid(Gate))
		{
			ActiveOrderedGates.Add(Gate);
		}
	}
}

void ADroneTrainingCourse::RebuildCourseLineSegments()
{
	// Construction을 여러 번 실행해도 옛 Segment가 겹쳐 남지 않도록 Tag 기준으로 먼저 제거한다.
	TInlineComponentArray<USplineMeshComponent*> ExistingSplineMeshes;
	GetComponents(ExistingSplineMeshes);
	for (USplineMeshComponent* ExistingSplineMesh : ExistingSplineMeshes)
	{
		if (ExistingSplineMesh
			&& ExistingSplineMesh->ComponentHasTag(DroneTrainingCourse::GeneratedSegmentTag))
		{
			BlueprintCreatedComponents.Remove(ExistingSplineMesh);
			ExistingSplineMesh->DestroyComponent();
		}
	}

	DynamicCourseLineMaterial = nullptr;

	if (!CourseSpline || !CourseLineMesh)
	{
		return;
	}

	// 제어점 한 쌍에 긴 Cube 하나를 늘이면 곡률이 큰 곳이 각져 보인다. 전체 길이를
	// 일정 간격으로 다시 표본화해 짧은 SplineMesh 여러 개가 같은 곡선을 잇게 한다.
	const int32 SegmentCount = GetExpectedCourseLineSegmentCount();
	if (SegmentCount == 0)
	{
		return;
	}
	const float SplineLength = CourseSpline->GetSplineLength();
	const float SegmentArcLength = SplineLength / static_cast<float>(SegmentCount);

	UMaterialInterface* MaterialToUse = CreateCourseLineMaterial();
	const FVector MeshSize = CourseLineMesh->GetBounds().BoxExtent * 2.0f;
	const float MeshWidth = FMath::Max(MeshSize.Y, 1.0f);
	const float MeshThickness = FMath::Max(MeshSize.Z, 1.0f);
	const FVector2D CourseLineScale(
		FMath::Max(CourseLineWidthCentimeters, 1.0f) / MeshWidth,
		FMath::Max(CourseLineThicknessCentimeters, 1.0f) / MeshThickness);

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const FName SegmentName = MakeUniqueObjectName(
			this,
			USplineMeshComponent::StaticClass(),
			*FString::Printf(TEXT("CourseLineSegment_%02d"), SegmentIndex));
		USplineMeshComponent* CourseLineSegment = NewObject<USplineMeshComponent>(
			this,
			SegmentName,
			RF_Transactional);
		if (!CourseLineSegment)
		{
			continue;
		}

		// UserConstructionScript 방식으로 등록하면 Map 저장과 Construction 재실행 수명주기를 따른다.
		PostCreateBlueprintComponent(CourseLineSegment);
		CourseLineSegment->ComponentTags.Add(DroneTrainingCourse::GeneratedSegmentTag);
		CourseLineSegment->SetupAttachment(CourseSpline);
		CourseLineSegment->SetMobility(EComponentMobility::Static);
		CourseLineSegment->SetStaticMesh(CourseLineMesh);
		CourseLineSegment->SetForwardAxis(ESplineMeshAxis::X, false);
		CourseLineSegment->SetSplineUpDir(FVector::UpVector, false);
		CourseLineSegment->SetStartScale(CourseLineScale, false);
		CourseLineSegment->SetEndScale(CourseLineScale, false);
		CourseLineSegment->bSmoothInterpRollScale = true;

		// 표시선은 비행 판정 대상이 아니다. 네 항목을 Segment마다 강제로 고정한다.
		CourseLineSegment->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		CourseLineSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CourseLineSegment->SetGenerateOverlapEvents(false);
		CourseLineSegment->SetSimulatePhysics(false);
		CourseLineSegment->SetCanEverAffectNavigation(false);
		CourseLineSegment->SetbNeverNeedsCookedCollisionData(true);
		CourseLineSegment->SetCastShadow(false);
		CourseLineSegment->SetReceivesDecals(false);
		CourseLineSegment->SetVisibility(true);
		CourseLineSegment->SetHiddenInGame(false);

		if (MaterialToUse)
		{
			CourseLineSegment->SetMaterial(0, MaterialToUse);
		}

		const float StartDistance = SegmentArcLength * static_cast<float>(SegmentIndex);
		const float EndDistance = SegmentIndex + 1 == SegmentCount
			? SplineLength
			: SegmentArcLength * static_cast<float>(SegmentIndex + 1);
		const float TangentLength = FMath::Max(EndDistance - StartDistance, 1.0f);
		const FVector StartPosition = CourseSpline->GetLocationAtDistanceAlongSpline(
			StartDistance,
			ESplineCoordinateSpace::Local);
		const FVector EndPosition = CourseSpline->GetLocationAtDistanceAlongSpline(
			EndDistance,
			ESplineCoordinateSpace::Local);
		const FVector StartTangent = CourseSpline->GetDirectionAtDistanceAlongSpline(
			StartDistance,
			ESplineCoordinateSpace::Local) * TangentLength;
		const FVector EndTangent = CourseSpline->GetDirectionAtDistanceAlongSpline(
			EndDistance,
			ESplineCoordinateSpace::Local) * TangentLength;

		const FVector VerticalOffset(0.0f, 0.0f, CourseLineVerticalOffsetCentimeters);
		CourseLineSegment->SetStartAndEnd(
			StartPosition + VerticalOffset,
			StartTangent,
			EndPosition + VerticalOffset,
			EndTangent,
			false);
		CourseLineSegment->RegisterComponent();
		CourseLineSegment->UpdateMesh();
	}
}

UMaterialInterface* ADroneTrainingCourse::CreateCourseLineMaterial()
{
	if (!CourseLineMaterial)
	{
		return nullptr;
	}

	// M_DroneTrainingGuide의 Color Parameter를 인스턴스마다 바꿔 BP 노출 색을 반영한다.
	DynamicCourseLineMaterial = UMaterialInstanceDynamic::Create(CourseLineMaterial, this);
	if (DynamicCourseLineMaterial)
	{
		DynamicCourseLineMaterial->SetVectorParameterValue(TEXT("Color"), CourseLineColor);
		return DynamicCourseLineMaterial;
	}

	return CourseLineMaterial;
}
