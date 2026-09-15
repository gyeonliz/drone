#include "Abilities/DroneDroppedPayload.h"

#include "Abilities/DronePayloadTargetComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
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
	CollisionComponent->SetCanEverAffectNavigation(false);

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

	PickupLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));
	PickupLabel->SetupAttachment(CollisionComponent);
	PickupLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
	PickupLabel->SetHorizontalAlignment(EHTA_Center);
	PickupLabel->SetWorldSize(28.0f);
	PickupLabel->SetText(FText::FromString(TEXT("PICKUP")));
	PickupLabel->SetTextRenderColor(FColor(255, 205, 30));
	PickupLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupLabel->SetCanEverAffectNavigation(false);
	PickupLabel->SetVisibility(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->InitialSpeed = 300.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bAutoActivate = false;

	InitialLifeSpan = 0.0f;
}

void ADroneDroppedPayload::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);
	}
	if (bStartsAsCarryablePickup)
	{
		ActivateCarryablePickup();
	}
}

void ADroneDroppedPayload::ActivateCarryablePickup()
{
	if (AActor* PreviousOwner = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(PreviousOwner, false);
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetOwner(nullptr);
	bReusableCarryable = true;
	bAvailableForPickup = true;
	bCarried = false;
	bImpactResolved = false;
	bHitIntendedTarget = false;
	IntendedTarget.Reset();
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetActorHiddenInGame(false);
	PayloadVisual->SetVisibility(true, true);
	PickupLabel->SetVisibility(bShowPickupLabel, true);
	SetLifeSpan(0.0f);
}

bool ADroneDroppedPayload::PrepareForCarry(AActor* NewCarrierActor, USceneComponent* CarryAnchor)
{
	if (!bAvailableForPickup || !IsValid(NewCarrierActor) || !IsValid(CarryAnchor))
	{
		return false;
	}

	bAvailableForPickup = false;
	bCarried = true;
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupLabel->SetVisibility(false, true);
	SetOwner(NewCarrierActor);
	AttachToComponent(CarryAnchor, FAttachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(
		CarryAnchor->GetComponentLocation(),
		CarryAnchor->GetComponentRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetLifeSpan(0.0f);
	return true;
}

void ADroneDroppedPayload::InitializePayload(
	AActor* NewIntendedTarget,
	const float DownwardSpeedCentimetersPerSecond)
{
	bAvailableForPickup = false;
	bCarried = false;
	bImpactResolved = false;
	bHitIntendedTarget = false;
	IntendedTarget = NewIntendedTarget;
	PickupLabel->SetVisibility(false, true);
	PayloadVisual->SetVisibility(true, true);
	SetActorHiddenInGame(false);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);
	}
	ProjectileMovement->Activate(true);
	ProjectileMovement->Velocity = FVector::DownVector * FMath::Max(1.0f, DownwardSpeedCentimetersPerSecond);
	SetLifeSpan(bReusableCarryable ? 0.0f : FMath::Max(0.1f, DroppedPayloadLifetimeSeconds));
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
	if (bAvailableForPickup || bCarried || bImpactResolved || !IsValid(HitActor) || HitActor == this || HitActor == GetOwner())
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
	if (bReusableCarryable)
	{
		if (AActor* PreviousOwner = GetOwner())
		{
			CollisionComponent->IgnoreActorWhenMoving(PreviousOwner, false);
		}
		SetOwner(nullptr);
		bAvailableForPickup = true;
		bCarried = false;
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PickupLabel->SetVisibility(bShowPickupLabel, true);
		SetLifeSpan(0.0f);
		return true;
	}
	SetLifeSpan(0.10f);
	return true;
}
