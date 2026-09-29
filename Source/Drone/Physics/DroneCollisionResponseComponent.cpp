#include "Physics/DroneCollisionResponseComponent.h"

#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UDroneCollisionResponseComponent::UDroneCollisionResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneCollisionResponseComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnActorHit.AddUniqueDynamic(this, &UDroneCollisionResponseComponent::HandleOwnerHit);
	}
}

void UDroneCollisionResponseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnActorHit.RemoveDynamic(this, &UDroneCollisionResponseComponent::HandleOwnerHit);
	}
	Super::EndPlay(EndPlayReason);
}

void UDroneCollisionResponseComponent::ConfigureCollisionResponse(
	const bool bEnabled,
	const float NewRestitution,
	const float NewMinimumImpactSpeed,
	const float NewMinimumSeparationSpeed,
	const float NewMaximumResponseSpeed)
{
	bCollisionResponseEnabled = bEnabled;
	Restitution = FMath::Clamp(NewRestitution, 0.0f, 1.0f);
	MinimumImpactSpeedCentimetersPerSecond = FMath::Max(0.0f, NewMinimumImpactSpeed);
	MinimumSeparationSpeedCentimetersPerSecond = FMath::Max(0.0f, NewMinimumSeparationSpeed);
	MaximumResponseSpeedCentimetersPerSecond = FMath::Max(0.0f, NewMaximumResponseSpeed);
}

FVector UDroneCollisionResponseComponent::ComputeReflectedVelocity(
	const FVector IncomingVelocity,
	FVector ImpactNormal,
	const float InRestitution,
	const float MinimumSeparationSpeed,
	const float MaximumResponseSpeed)
{
	ImpactNormal = ImpactNormal.GetSafeNormal();
	if (ImpactNormal.IsNearlyZero() || IncomingVelocity.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector Result = IncomingVelocity.MirrorByVector(ImpactNormal)
		* FMath::Clamp(InRestitution, 0.0f, 1.0f);
	const float OutwardSpeed = FVector::DotProduct(Result, ImpactNormal);
	if (OutwardSpeed < FMath::Max(0.0f, MinimumSeparationSpeed))
	{
		Result += ImpactNormal * (FMath::Max(0.0f, MinimumSeparationSpeed) - OutwardSpeed);
	}
	return Result.GetClampedToMaxSize(FMath::Max(0.0f, MaximumResponseSpeed));
}

void UDroneCollisionResponseComponent::HandleOwnerHit(
	AActor* SelfActor,
	AActor* OtherActor,
	FVector /*NormalImpulse*/,
	const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(SelfActor);
	UFloatingPawnMovement* Movement = Pawn ? Cast<UFloatingPawnMovement>(Pawn->GetMovementComponent()) : nullptr;
	if (!bCollisionResponseEnabled || !World || !Movement || !IsValid(OtherActor) || OtherActor == SelfActor)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastResolvedWorldSeconds < FMath::Max(0.0f, ResponseCooldownSeconds))
	{
		return;
	}

	const FVector IncomingVelocity = Movement->Velocity;
	const FVector ImpactNormal = Hit.ImpactNormal.GetSafeNormal();
	if (IncomingVelocity.Size() < MinimumImpactSpeedCentimetersPerSecond
		|| ImpactNormal.IsNearlyZero()
		|| FVector::DotProduct(IncomingVelocity, ImpactNormal) >= 0.0f)
	{
		return;
	}

	const FVector ReflectedVelocity = ComputeReflectedVelocity(
		IncomingVelocity,
		ImpactNormal,
		Restitution,
		MinimumSeparationSpeedCentimetersPerSecond,
		MaximumResponseSpeedCentimetersPerSecond);
	if (ReflectedVelocity.IsNearlyZero())
	{
		return;
	}

	if (bApplyImpactDamageToOtherActor && ImpactDamageToOtherActor > 0.0f)
	{
		UGameplayStatics::ApplyPointDamage(
			OtherActor,
			ImpactDamageToOtherActor,
			IncomingVelocity.GetSafeNormal(),
			Hit,
			Pawn->GetController(),
			SelfActor,
			UDamageType::StaticClass());
	}

	Movement->Velocity = ReflectedVelocity;
	LastResolvedWorldSeconds = Now;
	LastResolvedVelocity = ReflectedVelocity;
	++ResolvedImpactCount;
}
