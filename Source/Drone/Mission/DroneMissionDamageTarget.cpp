#include "Mission/DroneMissionDamageTarget.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Flow/DroneMissionPlayerController.h"
#include "Health/DroneHealthComponent.h"
#include "Mission/DroneMissionDirector.h"
#include "UObject/ConstructorHelpers.h"

ADroneMissionDamageTarget::ADroneMissionDamageTarget()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	TargetCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("TargetCollision"));
	TargetCollision->SetupAttachment(SceneRoot);
	TargetCollision->InitBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	TargetCollision->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	TargetCollision->SetGenerateOverlapEvents(false);

	TargetVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetVisual"));
	TargetVisual->SetupAttachment(SceneRoot);
	TargetVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetVisual->SetGenerateOverlapEvents(false);
	TargetVisual->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		TargetVisual->SetStaticMesh(CubeFinder.Object);
		TargetVisual->SetRelativeScale3D(FVector(2.0f));
	}

	HealthComponent = CreateDefaultSubobject<UDroneHealthComponent>(TEXT("HealthComponent"));
}

void ADroneMissionDamageTarget::BeginPlay()
{
	Super::BeginPlay();
	HealthComponent->ConfigureMaxHealth(MaximumHealth, true);
	HealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ADroneMissionDamageTarget::HandleHealthChanged);
	HealthComponent->OnDeath.AddUniqueDynamic(this, &ADroneMissionDamageTarget::HandleTargetDeath);
	TryRegisterWithMissionDirector();
}

void ADroneMissionDamageTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromMissionDirector();
	HealthComponent->OnHealthChanged.RemoveDynamic(this, &ADroneMissionDamageTarget::HandleHealthChanged);
	HealthComponent->OnDeath.RemoveDynamic(this, &ADroneMissionDamageTarget::HandleTargetDeath);
	Super::EndPlay(EndPlayReason);
}

bool ADroneMissionDamageTarget::IsTargetDestroyed() const
{
	return HealthComponent && HealthComponent->IsDead();
}

void ADroneMissionDamageTarget::ResetMissionTarget()
{
	HealthComponent->ConfigureMaxHealth(MaximumHealth, true);
	TargetCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TargetVisual->SetHiddenInGame(false, true);
}

void ADroneMissionDamageTarget::HandleHealthChanged(
	const float PreviousHealth,
	const float CurrentHealth,
	float /*MaxHealth*/,
	const float AppliedDamage)
{
	OnMissionTargetDamaged.Broadcast(this, PreviousHealth, CurrentHealth, AppliedDamage);
	ReceiveMissionTargetDamagedVisual(PreviousHealth, CurrentHealth, AppliedDamage);
}

void ADroneMissionDamageTarget::HandleTargetDeath(
	AActor* /*DeadActor*/,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	if (bDisableCollisionWhenDestroyed)
	{
		TargetCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (bHideVisualWhenDestroyed)
	{
		TargetVisual->SetHiddenInGame(true, true);
	}
	OnMissionTargetDestroyed.Broadcast(this, InstigatorController, DamageCauser);
	ReceiveMissionTargetDestroyedVisual(InstigatorController, DamageCauser);
}

void ADroneMissionDamageTarget::TryRegisterWithMissionDirector()
{
	UWorld* World = GetWorld();
	ADroneMissionPlayerController* Controller = World
		? World->GetFirstPlayerController<ADroneMissionPlayerController>()
		: nullptr;
	if (ADroneMissionDirector* Director = Controller ? Controller->GetMissionDirector() : nullptr)
	{
		Director->RegisterObjectiveTarget(this);
	}
}

void ADroneMissionDamageTarget::UnregisterFromMissionDirector()
{
	UWorld* World = GetWorld();
	ADroneMissionPlayerController* Controller = World
		? World->GetFirstPlayerController<ADroneMissionPlayerController>()
		: nullptr;
	if (ADroneMissionDirector* Director = Controller ? Controller->GetMissionDirector() : nullptr)
	{
		Director->UnregisterObjectiveTarget(this);
	}
}
