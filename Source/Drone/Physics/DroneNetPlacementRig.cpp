#include "Physics/DroneNetPlacementRig.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "UObject/ConstructorHelpers.h"

namespace DroneNetPlacementRig
{
void ConfigureMeshComponent(UStaticMeshComponent* Component, UStaticMesh* Mesh)
{
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
}
}

ADroneNetPlacementRig::ADroneNetPlacementRig()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	NetStrands = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("NetStrands"));
	NetStrands->SetupAttachment(SceneRoot);
	NetStrands->SetCollisionProfileName(TEXT("BlockAll"));
	NetStrands->SetGenerateOverlapEvents(false);
	NetStrands->SetNotifyRigidBodyCollision(true);
	NetStrands->OnComponentHit.AddDynamic(this, &ADroneNetPlacementRig::HandleStrandHit);

	TopLeftAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopLeftAnchor"));
	TopRightAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopRightAnchor"));
	BottomLeftAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BottomLeftAnchor"));
	BottomRightAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BottomRightAnchor"));
	TopLeftAnchor->SetupAttachment(SceneRoot);
	TopRightAnchor->SetupAttachment(SceneRoot);
	BottomLeftAnchor->SetupAttachment(SceneRoot);
	BottomRightAnchor->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		NetStrands->SetStaticMesh(CubeFinder.Object);
		DroneNetPlacementRig::ConfigureMeshComponent(TopLeftAnchor, CubeFinder.Object);
		DroneNetPlacementRig::ConfigureMeshComponent(TopRightAnchor, CubeFinder.Object);
		DroneNetPlacementRig::ConfigureMeshComponent(BottomLeftAnchor, CubeFinder.Object);
		DroneNetPlacementRig::ConfigureMeshComponent(BottomRightAnchor, CubeFinder.Object);
	}
}

void ADroneNetPlacementRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildNet();
}

float ADroneNetPlacementRig::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (AppliedDamage <= 0.0f)
	{
		return AppliedDamage;
	}

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID) && AppliedDamage >= FMath::Max(0.0f, MinimumPointDamage))
	{
		const FPointDamageEvent& PointDamage = static_cast<const FPointDamageEvent&>(DamageEvent);
		BreakNetAtWorldLocationInternal(
			PointDamage.HitInfo.ImpactPoint,
			FMath::Max(1.0f, LocalBreakRadiusCentimeters),
			PointDamage.ShotDirection);
	}
	else if (!bNetBroken)
	{
		AccumulatedDamage += AppliedDamage;
		if (AccumulatedDamage >= FMath::Max(1.0f, BreakDamageThreshold))
		{
			BreakNetGreybox();
		}
	}
	return AppliedDamage;
}

void ADroneNetPlacementRig::RebuildNet()
{
	if (!NetStrands)
	{
		return;
	}
	NetStrands->ClearInstances();
	DestroyDetachedPhysicsSegments();
	StrandSegmentTransforms.Reset();
	BrokenSegmentIndices.Reset();
	AccumulatedDamage = 0.0f;
	bNetBroken = false;
	RefreshAnchorVisuals();

	const int32 SafeSegments = FMath::Clamp(SegmentsPerStrand, 1, 16);
	const int32 SafeHorizontalCount = FMath::Clamp(HorizontalStrandCount, 2, 32);
	const int32 SafeVerticalCount = FMath::Clamp(VerticalStrandCount, 2, 32);
	for (int32 HorizontalIndex = 0; HorizontalIndex < SafeHorizontalCount; ++HorizontalIndex)
	{
		const float V = static_cast<float>(HorizontalIndex) / static_cast<float>(SafeHorizontalCount - 1);
		for (int32 SegmentIndex = 0; SegmentIndex < SafeSegments; ++SegmentIndex)
		{
			const float U0 = static_cast<float>(SegmentIndex) / static_cast<float>(SafeSegments);
			const float U1 = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SafeSegments);
			AddStrandSegment(EvaluateNetPoint(U0, V), EvaluateNetPoint(U1, V));
		}
	}
	for (int32 VerticalIndex = 0; VerticalIndex < SafeVerticalCount; ++VerticalIndex)
	{
		const float U = static_cast<float>(VerticalIndex) / static_cast<float>(SafeVerticalCount - 1);
		for (int32 SegmentIndex = 0; SegmentIndex < SafeSegments; ++SegmentIndex)
		{
			const float V0 = static_cast<float>(SegmentIndex) / static_cast<float>(SafeSegments);
			const float V1 = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SafeSegments);
			AddStrandSegment(EvaluateNetPoint(U, V0), EvaluateNetPoint(U, V1));
		}
	}

	RebuildVisibleSegments();
}

void ADroneNetPlacementRig::BreakNetGreybox()
{
	bNetBroken = true;
	BrokenSegmentIndices.Reset();
	for (int32 Index = 0; Index < StrandSegmentTransforms.Num(); ++Index)
	{
		BrokenSegmentIndices.Add(Index);
	}
	RebuildVisibleSegments();
}

int32 ADroneNetPlacementRig::BreakNetAtWorldLocation(const FVector WorldLocation, const float RadiusCentimeters)
{
	return BreakNetAtWorldLocationInternal(WorldLocation, RadiusCentimeters, FVector::ZeroVector);
}

int32 ADroneNetPlacementRig::BreakNetAtWorldLocationInternal(
	const FVector WorldLocation,
	const float RadiusCentimeters,
	const FVector ImpulseDirection)
{
	if (StrandSegmentTransforms.IsEmpty())
	{
		return 0;
	}

	const FVector LocalLocation = GetActorTransform().InverseTransformPosition(WorldLocation);
	const float RadiusSquared = FMath::Square(FMath::Max(1.0f, RadiusCentimeters));
	TArray<int32> NewlyBrokenIndices;
	for (int32 Index = 0; Index < StrandSegmentTransforms.Num(); ++Index)
	{
		if (!BrokenSegmentIndices.Contains(Index)
			&& FVector::DistSquared(StrandSegmentTransforms[Index].GetLocation(), LocalLocation) <= RadiusSquared)
		{
			BrokenSegmentIndices.Add(Index);
			NewlyBrokenIndices.Add(Index);
		}
	}

	if (!NewlyBrokenIndices.IsEmpty())
	{
		bNetBroken = BrokenSegmentIndices.Num() >= StrandSegmentTransforms.Num();
		RebuildVisibleSegments();
		const int32 PhysicsSegmentLimit = FMath::Clamp(MaximumDetachedPhysicsSegmentsPerBreak, 1, 64);
		for (int32 NewlyBrokenIndex = 0;
			NewlyBrokenIndex < NewlyBrokenIndices.Num() && NewlyBrokenIndex < PhysicsSegmentLimit;
			++NewlyBrokenIndex)
		{
			SpawnDetachedPhysicsSegment(NewlyBrokenIndices[NewlyBrokenIndex], ImpulseDirection);
		}
	}
	return NewlyBrokenIndices.Num();
}

void ADroneNetPlacementRig::ResetNetGreybox()
{
	AccumulatedDamage = 0.0f;
	bNetBroken = false;
	RebuildNet();
}

void ADroneNetPlacementRig::SpawnDetachedPhysicsSegment(
	const int32 SegmentIndex,
	const FVector& ImpulseDirection)
{
	if (!bSpawnDetachedStrandPhysics || !NetStrands || !NetStrands->GetStaticMesh()
		|| !StrandSegmentTransforms.IsValidIndex(SegmentIndex) || !GetWorld())
	{
		return;
	}

	UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
	Segment->SetMobility(EComponentMobility::Movable);
	Segment->SetStaticMesh(NetStrands->GetStaticMesh());
	Segment->SetCollisionProfileName(TEXT("PhysicsActor"));
	Segment->SetGenerateOverlapEvents(false);
	Segment->SetupAttachment(SceneRoot);
	Segment->RegisterComponent();
	AddInstanceComponent(Segment);
	Segment->SetRelativeTransform(StrandSegmentTransforms[SegmentIndex]);
	Segment->SetSimulatePhysics(true);

	const FVector BaseDirection = ImpulseDirection.IsNearlyZero()
		? FVector(0.35f, 0.0f, -1.0f).GetSafeNormal()
		: ImpulseDirection.GetSafeNormal();
	const FVector Impulse = BaseDirection * FMath::Max(0.0f, DetachedStrandImpulseStrength)
		+ FMath::VRand() * FMath::Max(0.0f, DetachedStrandRandomImpulseStrength);
	Segment->AddImpulse(Impulse, NAME_None, true);
	Segment->AddAngularImpulseInDegrees(
		FMath::VRand() * FMath::Max(0.0f, DetachedStrandRandomImpulseStrength), NAME_None, true);
	DetachedPhysicsSegments.Add(Segment);

	if (DetachedStrandLifetimeSeconds > 0.0f)
	{
		FTimerHandle& TimerHandle = DetachedSegmentTimerHandles.AddDefaulted_GetRef();
		FTimerDelegate ExpireDelegate = FTimerDelegate::CreateUObject(
			this, &ADroneNetPlacementRig::ExpireDetachedPhysicsSegment, Segment);
		GetWorldTimerManager().SetTimer(TimerHandle, ExpireDelegate, DetachedStrandLifetimeSeconds, false);
	}
}

void ADroneNetPlacementRig::ExpireDetachedPhysicsSegment(UStaticMeshComponent* Segment)
{
	if (IsValid(Segment))
	{
		Segment->DestroyComponent();
	}
	DetachedPhysicsSegments.RemoveSingleSwap(Segment);
}

void ADroneNetPlacementRig::DestroyDetachedPhysicsSegments()
{
	for (FTimerHandle& TimerHandle : DetachedSegmentTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	DetachedSegmentTimerHandles.Reset();
	for (UStaticMeshComponent* Segment : DetachedPhysicsSegments)
	{
		if (IsValid(Segment))
		{
			Segment->DestroyComponent();
		}
	}
	DetachedPhysicsSegments.Reset();
}

int32 ADroneNetPlacementRig::GetStrandInstanceCount() const
{
	return NetStrands ? NetStrands->GetInstanceCount() : 0;
}

int32 ADroneNetPlacementRig::GetIntactStrandSegmentCount() const
{
	return FMath::Max(0, StrandSegmentTransforms.Num() - BrokenSegmentIndices.Num());
}

FVector ADroneNetPlacementRig::EvaluateNetPoint(const float HorizontalAlpha, const float VerticalAlpha) const
{
	const FVector Left = FMath::Lerp(BottomLeftCorner, TopLeftCorner, VerticalAlpha);
	const FVector Right = FMath::Lerp(BottomRightCorner, TopRightCorner, VerticalAlpha);
	FVector Point = FMath::Lerp(Left, Right, HorizontalAlpha);
	const float HorizontalSag = 4.0f * HorizontalAlpha * (1.0f - HorizontalAlpha);
	const float VerticalSag = 4.0f * VerticalAlpha * (1.0f - VerticalAlpha);
	Point.X += FMath::Max(0.0f, SagDepthCentimeters) * HorizontalSag * VerticalSag;
	return Point;
}

void ADroneNetPlacementRig::AddStrandSegment(const FVector& Start, const FVector& End)
{
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (!NetStrands || Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Midpoint = (Start + End) * 0.5f;
	const FRotator Rotation = FRotationMatrix::MakeFromX(Delta).Rotator();
	const float ThicknessScale = FMath::Max(0.5f, StrandThicknessCentimeters) / 100.0f;
	StrandSegmentTransforms.Emplace(FTransform(
		Rotation,
		Midpoint,
		FVector(Length / 100.0f, ThicknessScale, ThicknessScale)));
}

void ADroneNetPlacementRig::RebuildVisibleSegments()
{
	if (!NetStrands)
	{
		return;
	}
	NetStrands->ClearInstances();
	for (int32 Index = 0; Index < StrandSegmentTransforms.Num(); ++Index)
	{
		if (!BrokenSegmentIndices.Contains(Index))
		{
			NetStrands->AddInstance(StrandSegmentTransforms[Index]);
		}
	}
	const bool bHasIntactSegments = GetIntactStrandSegmentCount() > 0;
	NetStrands->SetCollisionEnabled(
		bEnableStrandCollision && bHasIntactSegments ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	NetStrands->SetVisibility(bHasIntactSegments, true);
}

void ADroneNetPlacementRig::RefreshAnchorVisuals()
{
	UStaticMeshComponent* Anchors[4] = {TopLeftAnchor, TopRightAnchor, BottomLeftAnchor, BottomRightAnchor};
	const FVector Locations[4] = {TopLeftCorner, TopRightCorner, BottomLeftCorner, BottomRightCorner};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (Anchors[Index])
		{
			Anchors[Index]->SetRelativeLocation(Locations[Index]);
			Anchors[Index]->SetRelativeScale3D(FVector(0.08f));
		}
	}
}

void ADroneNetPlacementRig::HandleStrandHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!bBreakOnImpact || !OtherActor || OtherActor == this)
	{
		return;
	}
	const FVector OtherVelocity = OtherComponent ? OtherComponent->GetComponentVelocity() : OtherActor->GetVelocity();
	if (OtherVelocity.Size() < FMath::Max(0.0f, MinimumImpactSpeed))
	{
		return;
	}
	BreakNetAtWorldLocationInternal(
		Hit.ImpactPoint,
		FMath::Max(1.0f, LocalBreakRadiusCentimeters),
		OtherVelocity);
}
