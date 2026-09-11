#include "Tutorial/DroneTrainingGate.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace DroneTrainingGate
{
constexpr int32 RingSegmentCount = 16;
constexpr float EngineCubeSizeCentimeters = 100.0f;
}

ADroneTrainingGate::ADroneTrainingGate()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	GateRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GateRoot"));
	// Runtime Spawn과 상태 Material 갱신에서도 안전하게 재구성할 수 있도록 Movable로 둔다.
	// 실제 이동 기능을 뜻하지 않으며 Tick·Physics·Navigation은 계속 사용하지 않는다.
	GateRoot->SetMobility(EComponentMobility::Movable);
	GateRoot->SetCanEverAffectNavigation(false);
	SetRootComponent(GateRoot);

	GateTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("GateTrigger"));
	GateTrigger->SetupAttachment(GateRoot);
	GateTrigger->SetMobility(EComponentMobility::Movable);
	GateTrigger->SetHiddenInGame(true);
	GateTrigger->OnComponentBeginOverlap.AddDynamic(this, &ADroneTrainingGate::HandleTriggerBeginOverlap);
	GateTrigger->OnComponentEndOverlap.AddDynamic(this, &ADroneTrainingGate::HandleTriggerEndOverlap);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		RingSegmentMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GuideMaterialFinder(
		TEXT("/Game/Drone/Tutorial/Materials/M_DroneTrainingGuide.M_DroneTrainingGuide"));
	if (GuideMaterialFinder.Succeeded())
	{
		RingMaterial = GuideMaterialFinder.Object;
	}

	RingVisualSegments.Reserve(DroneTrainingGate::RingSegmentCount);
	for (int32 SegmentIndex = 0; SegmentIndex < DroneTrainingGate::RingSegmentCount; ++SegmentIndex)
	{
		const FName SegmentName(*FString::Printf(TEXT("RingVisualSegment_%02d"), SegmentIndex));
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(SegmentName);
		Segment->SetupAttachment(GateRoot);
		Segment->SetMobility(EComponentMobility::Movable);
		RingVisualSegments.Add(Segment);
	}

	ApplyComponentRules();
}

void ADroneTrainingGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyComponentRules();
	RefreshRingVisual();
	RefreshStateMaterial();
}

void ADroneTrainingGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PendingEntryLocations.Reset();
	AssignedGateSequence.Reset();
	Super::EndPlay(EndPlayReason);
}

FVector ADroneTrainingGate::GetForwardDirectionWorld() const
{
	// Ring과 Box는 항상 Actor 로컬 YZ 평면에 놓이므로 정방향도 로컬 +X 하나로 고정한다.
	return GetActorForwardVector().GetSafeNormal();
}

void ADroneTrainingGate::ConfigureGateDefinition(
	const FName InCourseId,
	const int32 InGateIndex,
	const float InSegmentDistance)
{
	CourseId = InCourseId;
	GateIndex = FMath::Max(0, InGateIndex);
	SegmentDistance = FMath::Max(0.0f, InSegmentDistance);
}

void ADroneTrainingGate::SetGateVisualState(const EDroneTrainingGateVisualState NewState)
{
	GateVisualState = NewState;
	RefreshStateMaterial();
}

FLinearColor ADroneTrainingGate::GetColorForVisualState(const EDroneTrainingGateVisualState State) const
{
	if (State == EDroneTrainingGateVisualState::Current)
	{
		return CurrentColor;
	}
	if (State == EDroneTrainingGateVisualState::Completed)
	{
		return CompletedColor;
	}
	return InactiveColor;
}

void ADroneTrainingGate::SetGateStateColors(
	const FLinearColor BeforePassColor,
	const FLinearColor CurrentTargetColor,
	const FLinearColor AfterPassColor)
{
	InactiveColor = BeforePassColor;
	CurrentColor = CurrentTargetColor;
	CompletedColor = AfterPassColor;
	RefreshStateMaterial();
}

void ADroneTrainingGate::AssignGateSequence(UDroneTrainingGateSequenceComponent* InSequence)
{
	AssignedGateSequence = InSequence;
	PendingEntryLocations.Reset();
}

void ADroneTrainingGate::CancelPendingTraversals()
{
	PendingEntryLocations.Reset();
}

void ADroneTrainingGate::ApplyComponentRules()
{
	SetActorEnableCollision(true);
	if (GateRoot)
	{
		GateRoot->SetCanEverAffectNavigation(false);
	}

	// Visual은 보이기만 하며 Hit, Overlap, Physics, Nav 어느 쪽에도 참여하지 않는다.
	for (UStaticMeshComponent* Segment : RingVisualSegments)
	{
		if (!Segment)
		{
			continue;
		}

		Segment->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetGenerateOverlapEvents(false);
		Segment->SetSimulatePhysics(false);
		Segment->SetCanEverAffectNavigation(false);
		Segment->SetCastShadow(false);
		Segment->SetReceivesDecals(false);
	}

	// 판정은 이 Box 하나만 담당하며 Pawn 외 채널은 모두 무시한다.
	if (GateTrigger)
	{
		GateTrigger->SetCollisionObjectType(ECC_WorldDynamic);
		GateTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GateTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		GateTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		GateTrigger->SetGenerateOverlapEvents(true);
		GateTrigger->SetSimulatePhysics(false);
		GateTrigger->SetCanEverAffectNavigation(false);
	}
}

void ADroneTrainingGate::RefreshRingVisual()
{
	const float Radius = FMath::Max(GateRadiusCentimeters, 50.0f);
	const float Thickness = FMath::Max(RingThicknessCentimeters, 2.0f);
	const float ChordLength = 2.0f * Radius * FMath::Sin(PI / DroneTrainingGate::RingSegmentCount);
	const FVector SegmentScale(
		ChordLength / DroneTrainingGate::EngineCubeSizeCentimeters,
		Thickness / DroneTrainingGate::EngineCubeSizeCentimeters,
		Thickness / DroneTrainingGate::EngineCubeSizeCentimeters);

	for (int32 SegmentIndex = 0; SegmentIndex < RingVisualSegments.Num(); ++SegmentIndex)
	{
		UStaticMeshComponent* Segment = RingVisualSegments[SegmentIndex];
		if (!Segment)
		{
			continue;
		}

		const float Angle = (2.0f * PI * SegmentIndex) / DroneTrainingGate::RingSegmentCount;
		const FVector RingPosition(0.0f, Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle));
		const FVector RingTangent(0.0f, -FMath::Sin(Angle), FMath::Cos(Angle));
		const FQuat RingRotation = FQuat::FindBetweenNormals(FVector::ForwardVector, RingTangent);

		Segment->SetStaticMesh(RingSegmentMesh);
		Segment->SetRelativeLocationAndRotation(RingPosition, RingRotation);
		Segment->SetRelativeScale3D(SegmentScale);
		Segment->SetVisibility(true);
		Segment->SetHiddenInGame(false);
	}

	if (GateTrigger)
	{
		GateTrigger->SetBoxExtent(FVector(
			FMath::Max(TriggerHalfDepthCentimeters, 1.0f),
			FMath::Max(TriggerHalfSizeCentimeters, 1.0f),
			FMath::Max(TriggerHalfSizeCentimeters, 1.0f)));
	}
}

void ADroneTrainingGate::RefreshStateMaterial()
{
	if (!RingMaterial)
	{
		DynamicRingMaterial = nullptr;
		return;
	}

	DynamicRingMaterial = UMaterialInstanceDynamic::Create(RingMaterial, this);
	if (!DynamicRingMaterial)
	{
		return;
	}

	DynamicRingMaterial->SetVectorParameterValue(TEXT("Color"), GetColorForVisualState(GateVisualState));
	for (UStaticMeshComponent* Segment : RingVisualSegments)
	{
		if (Segment)
		{
			Segment->SetMaterial(0, DynamicRingMaterial);
		}
	}
}

void ADroneTrainingGate::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// Destroy된 Pawn의 weak key가 남아 있더라도 다음 Overlap에서 즉시 정리한다.
	for (auto EntryIt = PendingEntryLocations.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (!EntryIt.Key().IsValid())
		{
			EntryIt.RemoveCurrent();
		}
	}

	if (!IsValid(OtherActor) || !OtherActor->IsA<ADronePrototypePawn>())
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(OtherActor);
	if (!PendingEntryLocations.Contains(ActorKey))
	{
		PendingEntryLocations.Add(ActorKey, OtherActor->GetActorLocation());
	}
}

void ADroneTrainingGate::HandleTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	// Pending-kill 상태에서도 먼저 같은 weak key를 제거한 뒤 Actor 유효성을 판정한다.
	FVector EntryWorldLocation;
	const bool bHadPendingEntry = OtherActor
		&& PendingEntryLocations.RemoveAndCopyValue(TWeakObjectPtr<AActor>(OtherActor), EntryWorldLocation);
	for (auto EntryIt = PendingEntryLocations.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (!EntryIt.Key().IsValid())
		{
			EntryIt.RemoveCurrent();
		}
	}

	if (!bHadPendingEntry || !IsValid(OtherActor))
	{
		return;
	}

	if (UDroneTrainingGateSequenceComponent* Sequence = AssignedGateSequence.Get(); IsValid(Sequence))
	{
		Sequence->TryAcceptTraversal(
			this,
			OtherActor,
			EntryWorldLocation,
			OtherActor->GetActorLocation());
	}
}
