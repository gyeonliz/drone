#include "Weapons/DronePlayerProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ADronePlayerProjectile::ADronePlayerProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;
	CollisionComponent->InitSphereRadius(6.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionComponent->SetNotifyRigidBodyCollision(true);

	ProjectileVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileVisual"));
	ProjectileVisual->SetupAttachment(CollisionComponent);
	ProjectileVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileVisual->SetGenerateOverlapEvents(false);
	ProjectileVisual->SetCanEverAffectNavigation(false);
	ProjectileVisual->SetCastShadow(false);
	ProjectileVisual->SetRelativeScale3D(FVector(0.08f));

	TrailVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrailVisual"));
	TrailVisual->SetupAttachment(CollisionComponent);
	TrailVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrailVisual->SetGenerateOverlapEvents(false);
	TrailVisual->SetCanEverAffectNavigation(false);
	TrailVisual->SetCastShadow(false);
	TrailVisual->SetRelativeLocation(FVector(-25.0f, 0.0f, 0.0f));
	TrailVisual->SetRelativeScale3D(FVector(0.50f, 0.02f, 0.02f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (SphereMesh.Succeeded())
	{
		ProjectileVisual->SetStaticMesh(SphereMesh.Object);
	}
	if (CubeMesh.Succeeded())
	{
		TrailVisual->SetStaticMesh(CubeMesh.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->InitialSpeed = 6000.0f;
	ProjectileMovement->MaxSpeed = 6000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bInitialVelocityInLocalSpace = true;

	InitialLifeSpan = 2.0f;
}

void ADronePlayerProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);
	}
	if (APawn* InstigatorPawn = GetInstigator(); InstigatorPawn && InstigatorPawn != GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(InstigatorPawn, true);
	}
}

void ADronePlayerProjectile::InitializeProjectile(
	const EDronePlayerProjectileMode InMode,
	const float InDamage,
	const float InSpeed,
	const float InMaximumTravelDistance,
	const float InRadialDamageRadius,
	const float InGravityScale)
{
	ProjectileMode = InMode;
	ProjectileDamage = FMath::Max(0.0f, InDamage);
	RadialDamageRadius = InMode == EDronePlayerProjectileMode::Radial
		? FMath::Max(1.0f, InRadialDamageRadius)
		: 0.0f;
	const float Speed = FMath::Max(1.0f, InSpeed);
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->ProjectileGravityScale = FMath::Max(0.0f, InGravityScale);
	ProjectileMovement->Velocity = GetActorForwardVector() * Speed;
	ProjectileVisual->SetRelativeScale3D(
		InMode == EDronePlayerProjectileMode::Radial ? FVector(0.16f) : FVector(0.08f));
	TrailVisual->SetVisibility(InMode == EDronePlayerProjectileMode::Direct, true);
	SetLifeSpan(FMath::Max(1.0f, InMaximumTravelDistance) / Speed + 0.5f);

	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);
	}
}

void ADronePlayerProjectile::NotifyHit(
	UPrimitiveComponent* MyComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const bool bSelfMoved,
	const FVector HitLocation,
	const FVector HitNormal,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	Super::NotifyHit(
		MyComponent,
		OtherActor,
		OtherComponent,
		bSelfMoved,
		HitLocation,
		HitNormal,
		NormalImpulse,
		Hit);

	if (bImpactHandled
		|| !IsValid(OtherActor)
		|| OtherActor == this
		|| OtherActor == GetOwner()
		|| OtherActor == GetInstigator())
	{
		return;
	}

	bImpactHandled = true;
	ApplyImpactDamage(OtherActor, HitLocation);
	OnProjectileImpact.Broadcast(this, OtherActor, ProjectileMode);
	Destroy();
}

void ADronePlayerProjectile::ApplyImpactDamage(AActor* HitActor, const FVector& ImpactLocation)
{
	if (ProjectileDamage <= 0.0f || !GetWorld())
	{
		return;
	}

	if (ProjectileMode == EDronePlayerProjectileMode::Direct)
	{
		UGameplayStatics::ApplyPointDamage(
			HitActor,
			ProjectileDamage,
			GetActorForwardVector(),
			FHitResult(),
			GetInstigatorController(),
			GetOwner(),
			nullptr);
		return;
	}

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(this);
	if (AActor* OwnerActor = GetOwner())
	{
		IgnoredActors.Add(OwnerActor);
	}
	UGameplayStatics::ApplyRadialDamage(
		this,
		ProjectileDamage,
		ImpactLocation,
		RadialDamageRadius,
		nullptr,
		IgnoredActors,
		GetOwner(),
		GetInstigatorController(),
		true,
		ECC_Visibility);
}
