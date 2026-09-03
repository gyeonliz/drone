#include "AI/Weapons/DroneNPCProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ADroneNPCProjectile::ADroneNPCProjectile()
{
	// 이동은 ProjectileMovementComponent가 담당하므로 Actor Tick은 사용하지 않는다.
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
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

	// 별도 Asset 구매 전에도 탄속과 회피 여부를 눈으로 검증할 수 있는 Engine 기본 Greybox다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		ProjectileVisual->SetStaticMesh(SphereMesh.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->InitialSpeed = ProjectileSpeed;
	ProjectileMovement->MaxSpeed = ProjectileSpeed;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bInitialVelocityInLocalSpace = true;

	InitialLifeSpan = 2.0f;
}

void ADroneNPCProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* OwnerActor = GetOwner())
	{
		// 총구가 Capsule 안에 있더라도 발사자를 즉시 맞고 사라지지 않게 한다.
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);
	}
	if (APawn* InstigatorPawn = GetInstigator(); InstigatorPawn && InstigatorPawn != GetOwner())
	{
		// MG는 Station이 Owner이고 조작 NPC가 Instigator이므로 둘 모두 충돌에서 제외한다.
		CollisionComponent->IgnoreActorWhenMoving(InstigatorPawn, true);
	}
}

void ADroneNPCProjectile::InitializeProjectile(
	const EDroneNPCProjectileSource InSource,
	AActor* InTargetActor,
	const float InDamage,
	const float InSpeed,
	const float InMaxTravelDistance)
{
	ProjectileSource = InSource;
	IntendedTarget = InTargetActor;
	ProjectileDamage = FMath::Max(0.0f, InDamage);
	ProjectileSpeed = FMath::Max(1.0f, InSpeed);
	if (AActor* OwnerActor = GetOwner())
	{
		// Editor 시험 World처럼 BeginPlay가 생략되는 경우에도 자기 충돌 방지 계약을 유지한다.
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);
	}

	ProjectileMovement->InitialSpeed = ProjectileSpeed;
	ProjectileMovement->MaxSpeed = ProjectileSpeed;
	ProjectileMovement->Velocity = GetActorForwardVector() * ProjectileSpeed;

	// 사거리만큼 이동한 직후 작은 여유를 두고 제거해 Map에 탄환이 누적되지 않게 한다.
	const float TravelSeconds = FMath::Max(1.0f, InMaxTravelDistance) / ProjectileSpeed;
	SetLifeSpan(TravelSeconds + 0.25f);
}

void ADroneNPCProjectile::NotifyHit(
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
	const bool bHitIntendedTarget = OtherActor == IntendedTarget.Get();
	if (bHitIntendedTarget && ProjectileDamage > 0.0f)
	{
		UGameplayStatics::ApplyDamage(
			OtherActor,
			ProjectileDamage,
			GetInstigatorController(),
			GetOwner(),
			nullptr);
	}

	OnProjectileImpact.Broadcast(this, ProjectileSource, OtherActor, bHitIntendedTarget);
	Destroy();
}
