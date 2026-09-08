#include "Abilities/DroneDroppedPayload.h"

#include "Abilities/DronePayloadTargetComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

ADroneDroppedPayload::ADroneDroppedPayload()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(18.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionComponent->SetNotifyRigidBodyCollision(true);

	PayloadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PayloadVisual"));
	PayloadVisual->SetupAttachment(CollisionComponent);
	PayloadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PayloadVisual->SetGenerateOverlapEvents(false);
	PayloadVisual->SetCanEverAffectNavigation(false);
	PayloadVisual->SetRelativeScale3D(FVector(0.25f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PayloadVisual->SetStaticMesh(CubeMesh.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->InitialSpeed = 300.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;

	InitialLifeSpan = 15.0f;
}

void ADroneDroppedPayload::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);
	}
}

void ADroneDroppedPayload::InitializePayload(
	AActor* NewIntendedTarget,
	const float DownwardSpeedCentimetersPerSecond)
{
	IntendedTarget = NewIntendedTarget;
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);
	}
	ProjectileMovement->Velocity = FVector::DownVector * FMath::Max(1.0f, DownwardSpeedCentimetersPerSecond);
}

void ADroneDroppedPayload::NotifyHit(
	UPrimitiveComponent* MyComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const bool bSelfMoved,
	const FVector HitLocation,
	const FVector HitNormal,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	Super::NotifyHit(MyComponent, OtherActor, OtherComponent, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	ResolveImpactGreybox(OtherActor);
}

bool ADroneDroppedPayload::ResolveImpactGreybox(AActor* HitActor)
{
	if (bImpactResolved || !IsValid(HitActor) || HitActor == this || HitActor == GetOwner())
	{
		return false;
	}

	bImpactResolved = true;
	bHitIntendedTarget = HitActor == IntendedTarget.Get();
	if (bHitIntendedTarget)
	{
		if (UDronePayloadTargetComponent* TargetComponent =
			HitActor->FindComponentByClass<UDronePayloadTargetComponent>())
		{
			TargetComponent->MarkPayloadDelivered(this);
		}
	}
	ProjectileMovement->StopMovementImmediately();
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnPayloadImpact.Broadcast(this, HitActor, bHitIntendedTarget);
	SetLifeSpan(0.10f);
	return true;
}
