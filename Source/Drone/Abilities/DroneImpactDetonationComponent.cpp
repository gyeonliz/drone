#include "Abilities/DroneImpactDetonationComponent.h"

#include "GameFramework/Pawn.h"
#include "Health/DroneHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UDroneImpactDetonationComponent::UDroneImpactDetonationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneImpactDetonationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnActorHit.AddUniqueDynamic(this, &UDroneImpactDetonationComponent::HandleOwnerHit);
	}
}

void UDroneImpactDetonationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnActorHit.RemoveDynamic(this, &UDroneImpactDetonationComponent::HandleOwnerHit);
	}
	Super::EndPlay(EndPlayReason);
}

void UDroneImpactDetonationComponent::ConfigureFeatureEnabled(const bool bEnabled)
{
	bFeatureEnabled = bEnabled;
	bArmed = false;
	bDetonated = false;
	DetonationCount = 0;
}

bool UDroneImpactDetonationComponent::ArmImpactDetonation()
{
	const AActor* OwnerActor = GetOwner();
	const UDroneHealthComponent* Health = OwnerActor
		? OwnerActor->FindComponentByClass<UDroneHealthComponent>()
		: nullptr;
	if (!bFeatureEnabled || bArmed || bDetonated || (Health && Health->IsDead()))
	{
		return false;
	}

	bArmed = true;
	OnImpactDetonationArmed.Broadcast();
	return true;
}

void UDroneImpactDetonationComponent::DisarmImpactDetonation()
{
	const bool bWasArmed = bArmed;
	bArmed = false;
	if (bWasArmed)
	{
		OnImpactDetonationDisarmed.Broadcast();
	}
}

bool UDroneImpactDetonationComponent::TryDetonateFromImpact(
	AActor* HitActor,
	const float ImpactSpeedCentimetersPerSecond)
{
	AActor* OwnerActor = GetOwner();
	if (!bFeatureEnabled
		|| !bArmed
		|| bDetonated
		|| !OwnerActor
		|| HitActor == OwnerActor
		|| ImpactSpeedCentimetersPerSecond < MinimumImpactSpeedCentimetersPerSecond)
	{
		return false;
	}

	// 먼저 상태를 잠가 같은 프레임의 여러 Hit가 폭발을 중복 실행하지 못하게 한다.
	bArmed = false;
	bDetonated = true;
	++DetonationCount;
	const FVector ExplosionLocation = OwnerActor->GetActorLocation();
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ExplosionEffect,
			ExplosionLocation,
			FRotator::ZeroRotator,
			ExplosionEffectScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLocation);
	}
	AController* InstigatorController = Cast<APawn>(OwnerActor)
		? Cast<APawn>(OwnerActor)->GetController()
		: nullptr;

	// Owner는 Radial Damage에서 제외하고 아래 공용 Health 경로로 정확히 한 번 파괴한다.
	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(OwnerActor);
	UGameplayStatics::ApplyRadialDamage(
		this,
		ExplosionDamage,
		ExplosionLocation,
		ExplosionRadiusCentimeters,
		nullptr,
		IgnoredActors,
		OwnerActor,
		InstigatorController,
		true,
		ECC_Visibility);

	if (UDroneHealthComponent* Health = OwnerActor->FindComponentByClass<UDroneHealthComponent>())
	{
		Health->ApplyHealthDamage(Health->GetCurrentHealth(), InstigatorController, OwnerActor);
	}
	OnImpactDetonated.Broadcast(ExplosionLocation, HitActor);
	return true;
}

void UDroneImpactDetonationComponent::ConfigureImpactGreybox(
	const float NewMinimumImpactSpeed,
	const float NewExplosionDamage,
	const float NewExplosionRadius)
{
	MinimumImpactSpeedCentimetersPerSecond = FMath::Max(0.0f, NewMinimumImpactSpeed);
	ExplosionDamage = FMath::Max(0.0f, NewExplosionDamage);
	ExplosionRadiusCentimeters = FMath::Max(1.0f, NewExplosionRadius);
}

void UDroneImpactDetonationComponent::HandleOwnerHit(
	AActor* SelfActor,
	AActor* OtherActor,
	FVector,
	const FHitResult&)
{
	if (SelfActor == GetOwner())
	{
		TryDetonateFromImpact(OtherActor, SelfActor->GetVelocity().Size());
	}
}
