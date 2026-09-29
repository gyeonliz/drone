#include "Physics/DroneBreakableWallPanel.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADroneBreakableWallPanel::ADroneBreakableWallPanel()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	IntactPieces = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("IntactPieces"));
	IntactPieces->SetupAttachment(SceneRoot);
	IntactPieces->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	IntactPieces->SetCollisionProfileName(TEXT("BlockAll"));
	IntactPieces->SetGenerateOverlapEvents(false);
	IntactPieces->SetNotifyRigidBodyCollision(true);
	IntactPieces->OnComponentHit.AddDynamic(this, &ADroneBreakableWallPanel::HandleIntactPieceHit);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		PieceMesh = CubeFinder.Object;
		IntactPieces->SetStaticMesh(PieceMesh);
	}
}

void ADroneBreakableWallPanel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildWall();
}

float ADroneBreakableWallPanel::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (AppliedDamage < FMath::Max(0.0f, MinimumPointDamage))
	{
		return AppliedDamage;
	}

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamage = static_cast<const FPointDamageEvent&>(DamageEvent);
		BreakWallAtWorldLocation(
			PointDamage.HitInfo.ImpactPoint,
			FMath::Max(1.0f, LocalBreakRadiusCentimeters),
			PointDamage.ShotDirection);
	}
	else
	{
		const FVector Origin = DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation();
		const FVector Direction = (GetActorLocation() - Origin).GetSafeNormal();
		BreakWallAtWorldLocation(GetActorLocation(), FMath::Max(1.0f, LocalBreakRadiusCentimeters), Direction);
	}
	return AppliedDamage;
}

void ADroneBreakableWallPanel::RebuildWall()
{
	DestroyPhysicsPieces();
	PieceTransforms.Reset();
	BrokenPieceIndices.Reset();

	const int32 SafeColumns = FMath::Clamp(Columns, 1, 24);
	const int32 SafeRows = FMath::Clamp(Rows, 1, 24);
	const FVector SafeSize(
		FMath::Max(10.0f, PieceSizeCentimeters.X),
		FMath::Max(10.0f, PieceSizeCentimeters.Y),
		FMath::Max(10.0f, PieceSizeCentimeters.Z));
	const float SafeGap = FMath::Max(0.0f, PieceGapCentimeters);
	const float TotalWidth = SafeColumns * SafeSize.Y + (SafeColumns - 1) * SafeGap;

	for (int32 Row = 0; Row < SafeRows; ++Row)
	{
		for (int32 Column = 0; Column < SafeColumns; ++Column)
		{
			const float Y = -0.5f * TotalWidth + 0.5f * SafeSize.Y + Column * (SafeSize.Y + SafeGap);
			const float Z = 0.5f * SafeSize.Z + Row * (SafeSize.Z + SafeGap);
			PieceTransforms.Emplace(
				FQuat::Identity,
				FVector(0.0f, Y, Z),
				SafeSize / 100.0f);
		}
	}
	RebuildIntactInstances();
}

int32 ADroneBreakableWallPanel::BreakWallAtWorldLocation(
	const FVector WorldLocation,
	const float RadiusCentimeters,
	const FVector ImpulseDirection)
{
	if (PieceTransforms.IsEmpty())
	{
		return 0;
	}

	const float RadiusSquared = FMath::Square(FMath::Max(1.0f, RadiusCentimeters));
	TArray<int32> NewlyBroken;
	for (int32 Index = 0; Index < PieceTransforms.Num(); ++Index)
	{
		if (BrokenPieceIndices.Contains(Index))
		{
			continue;
		}
		const FVector PieceWorldLocation = GetActorTransform().TransformPosition(PieceTransforms[Index].GetLocation());
		if (FVector::DistSquared(PieceWorldLocation, WorldLocation) <= RadiusSquared)
		{
			BrokenPieceIndices.Add(Index);
			NewlyBroken.Add(Index);
		}
	}

	if (NewlyBroken.IsEmpty())
	{
		return 0;
	}

	RebuildIntactInstances();
	for (const int32 PieceIndex : NewlyBroken)
	{
		SpawnPhysicsPiece(PieceIndex, ImpulseDirection);
	}
	return NewlyBroken.Num();
}

void ADroneBreakableWallPanel::ResetWall()
{
	RebuildWall();
}

int32 ADroneBreakableWallPanel::GetIntactPieceCount() const
{
	return FMath::Max(0, PieceTransforms.Num() - BrokenPieceIndices.Num());
}

FVector ADroneBreakableWallPanel::GetConfiguredWallSizeCentimeters() const
{
	const int32 SafeColumns = FMath::Clamp(Columns, 1, 24);
	const int32 SafeRows = FMath::Clamp(Rows, 1, 24);
	const FVector SafeSize(
		FMath::Max(10.0f, PieceSizeCentimeters.X),
		FMath::Max(10.0f, PieceSizeCentimeters.Y),
		FMath::Max(10.0f, PieceSizeCentimeters.Z));
	const float SafeGap = FMath::Max(0.0f, PieceGapCentimeters);
	return FVector(
		SafeSize.X,
		SafeColumns * SafeSize.Y + (SafeColumns - 1) * SafeGap,
		SafeRows * SafeSize.Z + (SafeRows - 1) * SafeGap);
}

void ADroneBreakableWallPanel::RebuildIntactInstances()
{
	if (!IntactPieces)
	{
		return;
	}
	IntactPieces->ClearInstances();
	IntactPieces->SetStaticMesh(PieceMesh);
	for (int32 Index = 0; Index < PieceTransforms.Num(); ++Index)
	{
		if (!BrokenPieceIndices.Contains(Index))
		{
			IntactPieces->AddInstance(PieceTransforms[Index]);
		}
	}
	IntactPieces->SetCollisionEnabled(GetIntactPieceCount() > 0 ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	IntactPieces->SetVisibility(GetIntactPieceCount() > 0, true);
}

void ADroneBreakableWallPanel::SpawnPhysicsPiece(const int32 PieceIndex, const FVector& ImpulseDirection)
{
	if (!PieceMesh || !PieceTransforms.IsValidIndex(PieceIndex) || !GetWorld())
	{
		return;
	}

	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
	Piece->SetMobility(EComponentMobility::Movable);
	Piece->SetStaticMesh(PieceMesh);
	Piece->SetCollisionProfileName(TEXT("PhysicsActor"));
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetupAttachment(SceneRoot);
	Piece->RegisterComponent();
	AddInstanceComponent(Piece);
	Piece->SetRelativeTransform(PieceTransforms[PieceIndex]);
	Piece->SetSimulatePhysics(true);

	const FVector BaseDirection = ImpulseDirection.IsNearlyZero() ? GetActorForwardVector() : ImpulseDirection.GetSafeNormal();
	const FVector Impulse = BaseDirection * FMath::Max(0.0f, BreakImpulseStrength)
		+ FMath::VRand() * FMath::Max(0.0f, RandomImpulseStrength);
	Piece->AddImpulse(Impulse, NAME_None, true);
	Piece->AddAngularImpulseInDegrees(FMath::VRand() * FMath::Max(0.0f, RandomImpulseStrength), NAME_None, true);
	PhysicsPieces.Add(Piece);
	if (DebrisLifetimeSeconds > 0.0f)
	{
		FTimerHandle& TimerHandle = DebrisTimerHandles.AddDefaulted_GetRef();
		FTimerDelegate ExpireDelegate = FTimerDelegate::CreateUObject(
			this, &ADroneBreakableWallPanel::ExpirePhysicsPiece, Piece);
		GetWorldTimerManager().SetTimer(TimerHandle, ExpireDelegate, DebrisLifetimeSeconds, false);
	}
}

void ADroneBreakableWallPanel::ExpirePhysicsPiece(UStaticMeshComponent* Piece)
{
	if (IsValid(Piece))
	{
		Piece->DestroyComponent();
	}
	PhysicsPieces.RemoveSingleSwap(Piece);
}

void ADroneBreakableWallPanel::DestroyPhysicsPieces()
{
	for (FTimerHandle& TimerHandle : DebrisTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	DebrisTimerHandles.Reset();
	for (UStaticMeshComponent* Piece : PhysicsPieces)
	{
		if (IsValid(Piece))
		{
			Piece->DestroyComponent();
		}
	}
	PhysicsPieces.Reset();
}

void ADroneBreakableWallPanel::HandleIntactPieceHit(
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
	BreakWallAtWorldLocation(Hit.ImpactPoint, FMath::Max(1.0f, LocalBreakRadiusCentimeters), OtherVelocity);
}
