#include "Physics/DroneCollisionResponseComponent.h"

#include "CollisionShape.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/DroneNetPlacementRig.h"
#include "Prototype/DronePrototypePawn.h"

UDroneCollisionResponseComponent::UDroneCollisionResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	RotorContactProbeLocalOffsets = {
		FVector(95.0f, 95.0f, 0.0f),
		FVector(95.0f, -95.0f, 0.0f),
		FVector(-95.0f, 95.0f, 0.0f),
		FVector(-95.0f, -95.0f, 0.0f)};
}

void UDroneCollisionResponseComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnActorHit.AddUniqueDynamic(this, &UDroneCollisionResponseComponent::HandleOwnerHit);
		// Pawn이 입력/자세를 갱신한 뒤 얽힘 감쇠와 교란을 적용해야 같은 프레임에 덮어쓰이지 않는다.
		PrimaryComponentTick.AddPrerequisite(OwnerActor, OwnerActor->PrimaryActorTick);
	}
	if (const ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(GetOwner()))
	{
		if (const UFloatingPawnMovement* Movement = Drone->GetPrototypeMovementComponent())
		{
			LastObservedFlightVelocity = Movement->Velocity;
		}
	}
}

void UDroneCollisionResponseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnActorHit.RemoveDynamic(this, &UDroneCollisionResponseComponent::HandleOwnerHit);
		PrimaryComponentTick.RemovePrerequisite(OwnerActor, OwnerActor->PrimaryActorTick);
	}
	ClearNetEntanglement();
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

	const float MaximumSpeed = FMath::Max(0.0f, MaximumResponseSpeed);
	if (MaximumSpeed <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float SignedNormalSpeed = FVector::DotProduct(IncomingVelocity, ImpactNormal);
	if (SignedNormalSpeed >= 0.0f)
	{
		return IncomingVelocity.GetClampedToMaxSize(MaximumSpeed);
	}

	const float InwardNormalSpeed = -SignedNormalSpeed;
	const FVector TangentialVelocity = IncomingVelocity - ImpactNormal * SignedNormalSpeed;
	const float RestitutionSpeed = InwardNormalSpeed * FMath::Clamp(InRestitution, 0.0f, 1.0f);
	// 기존 MinimumSeparationSpeed는 고정 90 cm/s Kick을 만들었다. 이제 최대 응답속도에 대한
	// 비율로만 보조하므로 20 cm/s 접촉과 80 cm/s 접촉이 서로 다른 크기로 밀려난다.
	const float ProportionalSeparationSpeed = FMath::Max(0.0f, MinimumSeparationSpeed)
		* FMath::Clamp(InwardNormalSpeed / MaximumSpeed, 0.0f, 1.0f);
	const float OutwardSpeed = FMath::Max(RestitutionSpeed, ProportionalSeparationSpeed);
	return (TangentialVelocity + ImpactNormal * OutwardSpeed).GetClampedToMaxSize(MaximumSpeed);
}

float UDroneCollisionResponseComponent::ComputeSpeedScaledSeparationDistance(
	const float InwardNormalSpeed,
	const float MinimumDistance,
	const float MaximumDistance,
	const float ReferenceImpactSpeed)
{
	const float SafeMinimum = FMath::Max(0.0f, MinimumDistance);
	const float SafeMaximum = FMath::Max(SafeMinimum, MaximumDistance);
	const float SpeedAlpha = FMath::Clamp(
		FMath::Max(0.0f, InwardNormalSpeed) / FMath::Max(1.0f, ReferenceImpactSpeed),
		0.0f,
		1.0f);
	return FMath::Lerp(SafeMinimum, SafeMaximum, SpeedAlpha);
}

FVector UDroneCollisionResponseComponent::ComputeContactAngularKickAxisAngleDegrees(
	const FVector ContactOffsetFromCenter,
	FVector ImpactNormal,
	const float InwardNormalSpeed,
	const float MaximumAngularKickDegrees,
	const float ReferenceImpactSpeed,
	const float ReferenceLeverArmCentimeters)
{
	ImpactNormal = ImpactNormal.GetSafeNormal();
	const FVector HorizontalNormal(ImpactNormal.X, ImpactNormal.Y, 0.0f);
	if (ImpactNormal.IsNearlyZero() || HorizontalNormal.IsNearlyZero()
		|| ContactOffsetFromCenter.IsNearlyZero() || InwardNormalSpeed <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	// Up x wall-normal은 기체 위쪽이 벽 바깥으로 기울어지는 축이다. 비구형 Collision이나
	// Probe의 접촉점이 비대칭이면 lever torque도 섞어 날개 끝 충돌의 방향성을 보존한다.
	const FVector SurfaceTiltAxis = FVector::CrossProduct(FVector::UpVector, HorizontalNormal.GetSafeNormal());
	const FVector LeverTorqueAxis = FVector::CrossProduct(
		ContactOffsetFromCenter.GetSafeNormal(),
		ImpactNormal);
	const FVector KickAxis = (SurfaceTiltAxis + LeverTorqueAxis * 0.5f).GetSafeNormal();
	if (KickAxis.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float SpeedAlpha = FMath::Clamp(
		InwardNormalSpeed / FMath::Max(1.0f, ReferenceImpactSpeed),
		0.0f,
		1.0f);
	const float LeverAlpha = FMath::Clamp(
		ContactOffsetFromCenter.Size() / FMath::Max(1.0f, ReferenceLeverArmCentimeters),
		0.15f,
		1.0f);
	return KickAxis * FMath::Max(0.0f, MaximumAngularKickDegrees) * SpeedAlpha * LeverAlpha;
}

bool UDroneCollisionResponseComponent::IsWallLikeSurfaceNormal(
	FVector ImpactNormal,
	const float MaximumAbsoluteNormalZ)
{
	ImpactNormal = ImpactNormal.GetSafeNormal();
	return !ImpactNormal.IsNearlyZero()
		&& FMath::Abs(ImpactNormal.Z) <= FMath::Clamp(MaximumAbsoluteNormalZ, 0.0f, 1.0f);
}

float UDroneCollisionResponseComponent::GetFlightControlEffectivenessMultiplier() const
{
	return FMath::Lerp(
		1.0f,
		FMath::Clamp(MinimumNetControlEffectiveness, 0.0f, 1.0f),
		FMath::Clamp(SmoothedNetSeverity, 0.0f, 1.0f));
}

bool UDroneCollisionResponseComponent::ApplyNetEntanglement(
	const FVector /*ContactPoint*/,
	const FVector /*ContactNormal*/,
	const float ImpactSpeedCentimetersPerSecond)
{
	ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!bNetEntanglementEnabled || !Drone || !World || Drone->IsGroundDriveModeActive()
		|| ImpactSpeedCentimetersPerSecond < FMath::Max(0.0f, MinimumNetContactSpeedCentimetersPerSecond))
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastNetContactWorldSeconds < FMath::Max(0.0f, NetContactAccumulationCooldownSeconds))
	{
		NetEntanglementRemainingSeconds = FMath::Max(
			NetEntanglementRemainingSeconds,
			NetEntanglementDurationSeconds);
		return true;
	}

	const float SpeedRange = FMath::Max(
		1.0f,
		ReferenceNetCaptureSpeedCentimetersPerSecond - MinimumNetContactSpeedCentimetersPerSecond);
	const float ImpactSeverity = FMath::Clamp(
		(ImpactSpeedCentimetersPerSecond - MinimumNetContactSpeedCentimetersPerSecond) / SpeedRange,
		0.12f,
		1.0f);
	const bool bAlreadyEntangled = IsNetEntangled();
	NetEntanglementSeverity = FMath::Clamp(
		FMath::Max(NetEntanglementSeverity, ImpactSeverity) + (bAlreadyEntangled ? 0.20f : 0.0f),
		0.0f,
		1.0f);
	bNetCaptured = NetEntanglementSeverity >= FMath::Clamp(NetCaptureSeverityThreshold, 0.0f, 1.0f);
	NetEntanglementRemainingSeconds = FMath::Max(
		NetEntanglementRemainingSeconds,
		FMath::Max(0.1f, NetEntanglementDurationSeconds)
			* FMath::Lerp(0.75f, 1.35f, NetEntanglementSeverity));
	LastNetContactWorldSeconds = Now;
	// Contact only changes the gameplay target. Drag is integrated in Tick; repeated
	// strand/probe hits must not cut velocity, restart a phase or play damage feedback.
	SetComponentTickEnabled(true);
	return true;
}

void UDroneCollisionResponseComponent::ClearNetEntanglement()
{
	NetEntanglementSeverity = 0.0f;
	NetEntanglementRemainingSeconds = 0.0f;
	SmoothedNetSeverity = 0.0f;
	bNetCaptured = false;
	SetComponentTickEnabled(true); // Allow drag/visual feedback to settle after release.
}

void UDroneCollisionResponseComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(GetOwner());
	UFloatingPawnMovement* Movement = Drone ? Drone->GetPrototypeMovementComponent() : nullptr;
	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaTime);
	if (Drone && Movement && !Drone->IsGroundDriveModeActive())
	{
		UpdateContactFeedback(Drone, SafeDeltaSeconds);
		TraceRotorWingContacts(Drone, Movement);
	}
	if (!IsNetEntangled() || !Drone || !Movement || Drone->IsGroundDriveModeActive())
	{
		if (IsNetEntangled())
		{
			ClearNetEntanglement();
		}
		LastObservedFlightVelocity = Movement ? Movement->Velocity : FVector::ZeroVector;
		return;
	}

	const float Severity = SmoothedNetSeverity;
	const float CaptureScale = bNetCaptured ? 1.45f : 1.0f;
	FVector Velocity = Movement->Velocity;
	const float HorizontalDamping = FMath::Exp(
		-FMath::Max(0.0f, NetHorizontalVelocityDampingPerSecond)
			* Severity * CaptureScale * SafeDeltaSeconds);
	Velocity.X *= HorizontalDamping;
	Velocity.Y *= HorizontalDamping;
	const float UpwardLimit = FMath::Max(0.0f, NetMaximumUpwardSpeedCentimetersPerSecond)
		* GetFlightControlEffectivenessMultiplier();
	if (Velocity.Z > UpwardLimit)
	{
		Velocity.Z = FMath::Lerp(Velocity.Z, UpwardLimit,
			1.0f - FMath::Exp(-SafeDeltaSeconds / FMath::Max(0.01f, NetResponseTimeSeconds)));
	}
	Velocity.Z -= FMath::Max(0.0f, NetDownwardAccelerationCentimetersPerSecondSquared)
		* Severity * CaptureScale * SafeDeltaSeconds;
	Movement->Velocity = Velocity;

	if (bAutoReleaseNetEntanglement)
	{
		NetEntanglementRemainingSeconds = FMath::Max(
			0.0f,
			NetEntanglementRemainingSeconds - SafeDeltaSeconds);
		if (NetEntanglementRemainingSeconds <= 0.0f)
		{
			ClearNetEntanglement();
		}
	}
	LastObservedFlightVelocity = Movement->Velocity;
}

void UDroneCollisionResponseComponent::UpdateContactFeedback(ADronePrototypePawn* Drone, const float DeltaSeconds)
{
	const float SeparationAlpha = 1.0f - FMath::Exp(
		-DeltaSeconds / FMath::Max(0.01f, SurfaceSeparationResponseTimeSeconds));
	if (!PendingSurfaceSeparation.IsNearlyZero(0.01f))
	{
		const FVector Step = PendingSurfaceSeparation * SeparationAlpha;
		PendingSurfaceSeparation -= Step;
		FHitResult SeparationHit;
		bApplyingSurfaceSeparation = true;
		Drone->AddActorWorldOffset(Step, true, &SeparationHit);
		bApplyingSurfaceSeparation = false;
		if (SeparationHit.bBlockingHit) PendingSurfaceSeparation = FVector::ZeroVector;
	}
	const float NetAlpha = 1.0f - FMath::Exp(-DeltaSeconds / FMath::Max(0.01f, NetResponseTimeSeconds));
	SmoothedNetSeverity = FMath::Lerp(SmoothedNetSeverity,
		IsNetEntangled() ? FMath::Clamp(NetEntanglementSeverity, 0.0f, 1.0f) : 0.0f, NetAlpha);
	NetDisturbancePhaseRadians = FMath::Fmod(NetDisturbancePhaseRadians
		+ DeltaSeconds * UE_TWO_PI * FMath::Max(0.0f, NetSwayFrequencyHertz), UE_TWO_PI);
	const float NetAmplitude = FMath::Max(0.0f, NetSwayMaximumDegrees) * SmoothedNetSeverity;
	FRotator Target(NetAmplitude * FMath::Sin(NetDisturbancePhaseRadians), 0.0f,
		NetAmplitude * 0.65f * FMath::Cos(NetDisturbancePhaseRadians));
	if (WallContactVisualTimeRemaining > 0.0f)
	{
		WallContactVisualTimeRemaining = FMath::Max(0.0f, WallContactVisualTimeRemaining - DeltaSeconds);
		const float Progress = 1.0f - WallContactVisualTimeRemaining / FMath::Max(0.05f, WallContactVisualDurationSeconds);
		Target += WallContactVisualPeak * FMath::Sin(UE_PI * Progress);
	}
	const float VisualAlpha = 1.0f - FMath::Exp(
		-DeltaSeconds / FMath::Max(0.01f, ContactVisualResponseTimeSeconds));
	CurrentContactVisualRotation = FMath::Lerp(CurrentContactVisualRotation, Target, VisualAlpha);
}

void UDroneCollisionResponseComponent::HandleOwnerHit(
	AActor* SelfActor,
	AActor* OtherActor,
	FVector /*NormalImpulse*/,
	const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	ADronePrototypePawn* Drone = Cast<ADronePrototypePawn>(SelfActor);
	APawn* Pawn = Drone;
	UFloatingPawnMovement* Movement = Pawn ? Cast<UFloatingPawnMovement>(Pawn->GetMovementComponent()) : nullptr;
	if (!World || !Movement || !IsValid(OtherActor) || OtherActor == SelfActor || (Drone && Drone->IsGroundDriveModeActive()))
	{
		return;
	}
	if (Cast<ADroneNetPlacementRig>(OtherActor))
	{
		ApplyNetEntanglement(Hit.ImpactPoint, Hit.ImpactNormal, Movement->Velocity.Size());
		return;
	}
	const FVector IncomingVelocity = Movement->Velocity;
	ResolveWallContact(Drone, OtherActor, Hit, IncomingVelocity);
}

bool UDroneCollisionResponseComponent::ResolveWallContact(
	ADronePrototypePawn* Drone,
	AActor* OtherActor,
	const FHitResult& Hit,
	const FVector IncomingVelocity,
	const bool bAllowPreviousVelocity)
{
	UWorld* World = GetWorld();
	UFloatingPawnMovement* Movement = Drone ? Drone->GetPrototypeMovementComponent() : nullptr;
	if (bApplyingSurfaceSeparation || !bCollisionResponseEnabled || !World || !Drone || !Movement || !IsValid(OtherActor)
		|| OtherActor == Drone || Drone->IsGroundDriveModeActive())
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	const FVector ImpactNormal = Hit.ImpactNormal.GetSafeNormal();
	if (ImpactNormal.IsNearlyZero() || (bOnlyRespondToWallLikeSurfaces
		&& !IsWallLikeSurfaceNormal(ImpactNormal, MaximumWallSurfaceAbsoluteNormalZ))) return false;
	const float CurrentInwardSpeed = -FVector::DotProduct(IncomingVelocity, ImpactNormal);
	const bool bContinuousContact = LastWallContactActor == OtherActor
		&& FVector::DotProduct(LastWallContactNormal, ImpactNormal) > 0.9f
		&& Now - LastWallTouchWorldSeconds < FMath::Max(0.01f, WallContactReleaseGraceSeconds);
	if (bContinuousContact)
	{
		LastWallTouchWorldSeconds = Now;
		// Continued pressure is a constraint, not another impact/shake. Keep tangential
		// travel and outward velocity; only replace fresh inward input with a gentle push.
		if (CurrentInwardSpeed > 0.0f)
		{
			Movement->Velocity = ComputeReflectedVelocity(IncomingVelocity, ImpactNormal,
				Restitution, MinimumSeparationSpeedCentimetersPerSecond, MaximumResponseSpeedCentimetersPerSecond);
		}
		return true;
	}
	if (Now - LastResolvedWorldSeconds < FMath::Max(0.0f, ResponseCooldownSeconds)
		|| CurrentInwardSpeed < -KINDA_SMALL_NUMBER) return false;
	const float PreviousInwardSpeed = -FVector::DotProduct(LastObservedFlightVelocity, ImpactNormal);
	const bool bUsePrevious = bAllowPreviousVelocity && PreviousInwardSpeed > CurrentInwardSpeed;
	const FVector EffectiveIncomingVelocity = bUsePrevious
		? LastObservedFlightVelocity
		: IncomingVelocity;
	const float InwardNormalSpeed = bUsePrevious ? PreviousInwardSpeed : CurrentInwardSpeed;
	if (InwardNormalSpeed < MinimumImpactSpeedCentimetersPerSecond
		|| ImpactNormal.IsNearlyZero()
		|| (bOnlyRespondToWallLikeSurfaces
			&& !IsWallLikeSurfaceNormal(ImpactNormal, MaximumWallSurfaceAbsoluteNormalZ)))
	{
		return false;
	}

	const FVector ReflectedVelocity = ComputeReflectedVelocity(
		EffectiveIncomingVelocity,
		ImpactNormal,
		Restitution,
		MinimumSeparationSpeedCentimetersPerSecond,
		MaximumResponseSpeedCentimetersPerSecond);
	if (ReflectedVelocity.IsNearlyZero())
	{
		return false;
	}

	if (bApplyImpactDamageToOtherActor && ImpactDamageToOtherActor > 0.0f)
	{
		const float DamageScale = FMath::Clamp(
			InwardNormalSpeed / FMath::Max(1.0f, ReferenceImpactSpeedForMaximumSeparation),
			0.0f,
			1.0f);
		UGameplayStatics::ApplyPointDamage(
			OtherActor,
			ImpactDamageToOtherActor * DamageScale,
			EffectiveIncomingVelocity.GetSafeNormal(),
			Hit,
			Drone->GetController(),
			Drone,
			UDamageType::StaticClass());
	}

	Movement->Velocity = ReflectedVelocity;
	const float SeparationDistance = ComputeSpeedScaledSeparationDistance(
		InwardNormalSpeed,
		MinimumSurfaceSeparationDistanceCentimeters,
		SurfaceSeparationDistanceCentimeters,
		ReferenceImpactSpeedForMaximumSeparation);
	PendingSurfaceSeparation = ImpactNormal * SeparationDistance;

	FVector ContactPoint = Hit.ImpactPoint;
	if (ContactPoint.IsNearlyZero())
	{
		ContactPoint = Drone->GetActorLocation() - ImpactNormal * 45.0f;
	}
	LastContactAngularKickAxisAngleDegrees = ComputeContactAngularKickAxisAngleDegrees(
		ContactPoint - Drone->GetActorLocation(),
		ImpactNormal,
		InwardNormalSpeed,
		MaximumRotorContactAngularKickDegrees,
		RotorContactAngularReferenceSpeedCentimetersPerSecond,
		RotorContactReferenceLeverArmCentimeters);
	const float KickDegrees = LastContactAngularKickAxisAngleDegrees.Size();
	if (KickDegrees > KINDA_SMALL_NUMBER)
	{
		const FVector LocalAxis = Drone->GetActorQuat().UnrotateVector(
			LastContactAngularKickAxisAngleDegrees / KickDegrees);
		WallContactVisualPeak = FQuat(LocalAxis, FMath::DegreesToRadians(KickDegrees)).Rotator();
		WallContactVisualTimeRemaining = FMath::Max(0.05f, WallContactVisualDurationSeconds);
	}
	LastResolvedWorldSeconds = Now;
	LastWallContactActor = OtherActor;
	LastWallContactNormal = ImpactNormal;
	LastWallTouchWorldSeconds = Now;
	LastResolvedVelocity = ReflectedVelocity;
	LastObservedFlightVelocity = ReflectedVelocity;
	++ResolvedImpactCount;
	return true;
}

void UDroneCollisionResponseComponent::TraceRotorWingContacts(
	ADronePrototypePawn* Drone,
	UFloatingPawnMovement* Movement)
{
	UWorld* World = GetWorld();
	if (!bUseRotorContactProbes || !bCollisionResponseEnabled || !World || !Drone || !Movement
		|| RotorContactProbeLocalOffsets.IsEmpty())
	{
		return;
	}

	const USphereComponent* RootSphere = Drone->GetCollisionComponent();
	const float RootRadius = RootSphere ? RootSphere->GetScaledSphereRadius() : 45.0f;
	const float ProbeRadius = FMath::Max(1.0f, RotorContactProbeRadiusCentimeters);
	const FTransform ActorTransform = Drone->GetActorTransform();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneRotorWallContact), false, Drone);
	QueryParams.AddIgnoredActor(Drone);

	for (const FVector& LocalOffset : RotorContactProbeLocalOffsets)
	{
		if (LocalOffset.IsNearlyZero())
		{
			continue;
		}
		const FVector WorldDirection = ActorTransform.TransformVectorNoScale(LocalOffset).GetSafeNormal();
		const FVector Start = Drone->GetActorLocation()
			+ WorldDirection * FMath::Max(0.0f, RootRadius - ProbeRadius * 0.5f);
		const FVector End = ActorTransform.TransformPosition(LocalOffset);
		FHitResult ProbeHit;
		if (!World->SweepSingleByChannel(
			ProbeHit,
			Start,
			End,
			FQuat::Identity,
			RotorContactTraceChannel,
			FCollisionShape::MakeSphere(ProbeRadius),
			QueryParams))
		{
			continue;
		}

		AActor* HitActor = ProbeHit.GetActor();
		if (ADroneNetPlacementRig* Net = Cast<ADroneNetPlacementRig>(HitActor))
		{
			ApplyNetEntanglement(ProbeHit.ImpactPoint, ProbeHit.ImpactNormal, Movement->Velocity.Size());
			return;
		}
		if (ResolveWallContact(Drone, HitActor, ProbeHit, Movement->Velocity, false))
		{
			return;
		}
	}
}
