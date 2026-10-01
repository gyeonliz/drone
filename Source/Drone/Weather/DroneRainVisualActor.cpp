#include "Weather/DroneRainVisualActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Weather/DroneWeatherWorldSubsystem.h"

namespace DroneRainVisual
{
	bool TraceBlockingSurface(
		UWorld* World,
		FHitResult& OutHit,
		const FVector& Start,
		const FVector& End,
		const ECollisionChannel TraceChannel,
		const FCollisionQueryParams& QueryParams)
	{
		if (!World)
		{
			return false;
		}

		if (World->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, QueryParams))
		{
			return true;
		}

		// Some marketplace roofs do not block Visibility. Fall back to their object type
		// so rain still stops at WorldStatic/WorldDynamic geometry without editing the asset.
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		return World->LineTraceSingleByObjectType(
			OutHit,
			Start,
			End,
			ObjectQueryParams,
			QueryParams);
	}
}

ADroneRainVisualActor::ADroneRainVisualActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RainStreaks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RainStreaks"));
	RainStreaks->SetupAttachment(SceneRoot);
	RainStreaks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainStreaks->SetGenerateOverlapEvents(false);
	RainStreaks->SetCanEverAffectNavigation(false);
	RainStreaks->SetCastShadow(false);
	RainStreaks->SetReceivesDecals(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		StreakMesh = PlaneMesh.Object;
		RainStreaks->SetStaticMesh(PlaneMesh.Object);
	}
	RainStreaks->SetTranslucentSortPriority(20);
	RainRandomStream.Initialize(260923);
}

void ADroneRainVisualActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (RainStreaks)
	{
		RainStreaks->SetStaticMesh(StreakMesh);
		if (StreakMaterial)
		{
			RainStreaks->SetMaterial(0, StreakMaterial);
		}
	}
}

void ADroneRainVisualActor::SetRainEnabled(const bool bEnabled)
{
	bRainEnabled = bEnabled;
	if (RainStreaks)
	{
		RainStreaks->SetVisibility(bRainEnabled, true);
	}
}

float ADroneRainVisualActor::CalculateIndoorExposure01(
	const bool bRoofBlocked,
	const float IndoorAttenuation01)
{
	return bRoofBlocked
		? 1.0f - FMath::Clamp(IndoorAttenuation01, 0.0f, 1.0f)
		: 1.0f;
}

bool ADroneRainVisualActor::ShouldRenderStreakAboveSurface(
	const float StreakCenterWorldZ,
	const float StreakHalfLengthCentimeters,
	const bool bBlockingSurfaceFound,
	const float BlockingSurfaceWorldZ,
	const float SurfaceClearanceCentimeters)
{
	if (!bBlockingSurfaceFound)
	{
		return true;
	}

	const float StreakBottomWorldZ = StreakCenterWorldZ - FMath::Max(0.0f, StreakHalfLengthCentimeters);
	return StreakBottomWorldZ > BlockingSurfaceWorldZ + FMath::Max(0.0f, SurfaceClearanceCentimeters);
}

void ADroneRainVisualActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	LastCeilingTraceColumnCount = 0;
	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (CameraManager && RainStreaks) UpdateRainForCamera(DeltaSeconds, CameraManager->GetCameraLocation());
}

void ADroneRainVisualActor::UpdateRainForCamera(const float DeltaSeconds, const FVector& CameraLocation)
{
	LastCeilingTraceColumnCount = 0;
	if (!RainStreaks) return;
	const UDroneWeatherWorldSubsystem* Weather = GetWorld()
		? GetWorld()->GetSubsystem<UDroneWeatherWorldSubsystem>() : nullptr;
	const FDroneWeatherSnapshot Snapshot = Weather ? Weather->GetSnapshot() : FDroneWeatherSnapshot();
	// 맑은 날에도 112개 Transform과 복잡한 천장 Trace를 계속 갱신하던 비용을 제거한다.
	if (!bRainEnabled || Snapshot.RainIntensity01 * Snapshot.RainSpawnScale01 <= UE_SMALL_NUMBER)
	{
		if (RainStreaks) RainStreaks->SetVisibility(false, true);
		PreviousActiveStreakCount = 0;
		bForceCeilingCacheRefresh = true;
		return;
	}
	SetActorLocation(CameraLocation);
	IndoorCheckRemainingSeconds -= FMath::Max(0.0f, DeltaSeconds);
	if (IndoorCheckRemainingSeconds <= 0.0f)
	{
		UpdateIndoorState(CameraLocation);
		IndoorCheckRemainingSeconds = FMath::Max(0.05f, IndoorCheckIntervalSeconds);
	}
	UpdateStreaks(DeltaSeconds, CameraLocation);
}

void ADroneRainVisualActor::RebuildStreakPool()
{
	RainStreaks->ClearInstances();
	StreakLocalPositions.Reset();
	StreakVisualScaleVariation.Reset();
	const int32 SafeMaximum = FMath::Clamp(MaximumStreakCount, 8, 512);
	StreakLocalPositions.Reserve(SafeMaximum);
	StreakVisualScaleVariation.Reserve(SafeMaximum);
	StreakBlockingSurfaceWorldZ.Init(0.0f, SafeMaximum);
	StreakHasBlockingSurface.Init(false, SafeMaximum);
	StreakSurfaceCacheValid.Init(false, SafeMaximum);
	StreakTransforms.SetNum(SafeMaximum);
	NextCeilingTraceIndex = 0;
	PreviousActiveStreakCount = 0;
	bForceCeilingCacheRefresh = true;
	RainRandomStream.Initialize(260923);
	for (int32 Index = 0; Index < SafeMaximum; ++Index)
	{
		const float Angle = RainRandomStream.FRandRange(0.0f, 2.0f * PI);
		const float Radius = FMath::Sqrt(RainRandomStream.FRand()) * FollowRadiusCentimeters;
		StreakLocalPositions.Add(FVector(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			RainRandomStream.FRandRange(-FollowHalfHeightCentimeters, FollowHalfHeightCentimeters)));
		StreakVisualScaleVariation.Add(FVector2D(
			RainRandomStream.FRandRange(0.70f, 1.15f),
			RainRandomStream.FRandRange(0.55f, 1.00f)));
		RainStreaks->AddInstance(FTransform::Identity);
	}
}

void ADroneRainVisualActor::UpdateIndoorState(const FVector& CameraLocation)
{
	if (!bSuppressRainIndoors || !GetWorld())
	{
		bForceCeilingCacheRefresh |= bCameraIndoors;
		bCameraIndoors = false;
		return;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(DroneRainIndoorTrace),
		bUseComplexCeilingTraces,
		this);
	if (const APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		QueryParams.AddIgnoredActor(Controller->GetPawn());
	}
	const FVector TraceEnd = CameraLocation + FVector::UpVector * IndoorTraceDistanceCentimeters;
	FHitResult Hit;
	const bool bWasCameraIndoors = bCameraIndoors;
	bCameraIndoors = DroneRainVisual::TraceBlockingSurface(
		GetWorld(),
		Hit,
		CameraLocation + FVector::UpVector * 20.0f,
		TraceEnd,
		CeilingTraceChannel,
		QueryParams);
	if (bCameraIndoors != bWasCameraIndoors)
	{
		bForceCeilingCacheRefresh = true;
	}
	if (bDrawIndoorTrace)
	{
		DrawDebugLine(
			GetWorld(), CameraLocation, TraceEnd,
			bCameraIndoors ? FColor::Red : FColor::Green,
			false, IndoorCheckIntervalSeconds, 0, 2.0f);
	}
}

void ADroneRainVisualActor::UpdateCeilingSurfaceCache(
	const FVector& CameraLocation,
	const int32 ActiveCount,
	const bool bForceFullRefresh)
{
	UWorld* World = GetWorld();
	const int32 SafeActiveCount = FMath::Clamp(ActiveCount, 0, StreakLocalPositions.Num());
	if (!World || !bClipStreaksAgainstCeilings || SafeActiveCount <= 0
		|| StreakBlockingSurfaceWorldZ.Num() != StreakLocalPositions.Num()
		|| StreakHasBlockingSurface.Num() != StreakLocalPositions.Num())
	{
		return;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(DroneRainCeilingTrace),
		bUseComplexCeilingTraces,
		this);
	if (const APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		QueryParams.AddIgnoredActor(Controller->GetPawn());
	}

	const float TraceTopOffset = FMath::Max(
		IndoorTraceDistanceCentimeters,
		FMath::Max(100.0f, FollowHalfHeightCentimeters) + StreakLengthCentimeters);
	const float TraceBottomOffset = FMath::Max(100.0f, FollowHalfHeightCentimeters) + StreakLengthCentimeters;
	if (bForceFullRefresh)
	{
		// 최초/실내 전환 때 전부 즉시 검사하면 최대 512개 Complex Trace가 한 프레임에 몰린다.
		// 대신 캐시를 무효화한 뒤 매 프레임 예산만큼 복구한다.
		StreakSurfaceCacheValid.Init(false, StreakLocalPositions.Num());
		NextCeilingTraceIndex = 0;
	}
	const int32 TraceCount = FMath::Clamp(CeilingTraceBudgetPerFrame, 1, SafeActiveCount);
	LastCeilingTraceColumnCount = TraceCount;

	for (int32 TraceOffset = 0; TraceOffset < TraceCount; ++TraceOffset)
	{
		const int32 Index = NextCeilingTraceIndex % SafeActiveCount;
		NextCeilingTraceIndex = (Index + 1) % SafeActiveCount;

		const FVector& LocalPosition = StreakLocalPositions[Index];
		const FVector ColumnLocation(
			CameraLocation.X + LocalPosition.X,
			CameraLocation.Y + LocalPosition.Y,
			CameraLocation.Z);
		const FVector TraceStart = ColumnLocation + FVector::UpVector * TraceTopOffset;
		const FVector TraceEnd = ColumnLocation - FVector::UpVector * TraceBottomOffset;
		FHitResult Hit;
		const bool bHit = DroneRainVisual::TraceBlockingSurface(
			World,
			Hit,
			TraceStart,
			TraceEnd,
			CeilingTraceChannel,
			QueryParams);
		StreakHasBlockingSurface[Index] = bHit;
		StreakSurfaceCacheValid[Index] = true;
		StreakBlockingSurfaceWorldZ[Index] = bHit ? Hit.ImpactPoint.Z : 0.0f;
	}
}

void ADroneRainVisualActor::UpdateStreaks(const float DeltaSeconds, const FVector& CameraLocation)
{
	bool bPoolRebuilt = false;
	if (StreakLocalPositions.Num() != FMath::Clamp(MaximumStreakCount, 8, 512))
	{
		RebuildStreakPool();
		bPoolRebuilt = true;
	}

	const UDroneWeatherWorldSubsystem* Weather = GetWorld()
		? GetWorld()->GetSubsystem<UDroneWeatherWorldSubsystem>()
		: nullptr;
	const FDroneWeatherSnapshot Snapshot = Weather ? Weather->GetSnapshot() : FDroneWeatherSnapshot();
	const float TargetExposure = CalculateIndoorExposure01(
		bCameraIndoors,
		Snapshot.IndoorRainAttenuation01);
	const float ExposureAlpha = 1.0f - FMath::Exp(
		-FMath::Max(0.0f, DeltaSeconds) / FMath::Max(0.01f, IndoorBlendSeconds));
	CurrentLocalExposure01 = FMath::Lerp(CurrentLocalExposure01, TargetExposure, ExposureAlpha);
	const float EffectiveRain = bRainEnabled
		? FMath::Clamp(Snapshot.RainIntensity01 * Snapshot.RainSpawnScale01 * CurrentLocalExposure01, 0.0f, 1.0f)
		: 0.0f;
	const int32 ActiveCount = FMath::RoundToInt(EffectiveRain * StreakLocalPositions.Num());
	RainStreaks->SetVisibility(ActiveCount > 0, true);

	const FVector FallVelocity = FVector(
		Snapshot.WindVelocityCentimetersPerSecond.X * WindVisualInfluence,
		Snapshot.WindVelocityCentimetersPerSecond.Y * WindVisualInfluence,
		-FallSpeedCentimetersPerSecond);
	const FVector FallDirection = FallVelocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::DownVector);
	const float SafeRadius = FMath::Max(100.0f, FollowRadiusCentimeters);
	const float SafeHalfHeight = FMath::Max(100.0f, FollowHalfHeightCentimeters);

	for (FVector& LocalPosition : StreakLocalPositions)
	{
		LocalPosition += FallVelocity * FMath::Max(0.0f, DeltaSeconds);
		if (LocalPosition.Z < -SafeHalfHeight)
		{
			LocalPosition.Z += SafeHalfHeight * 2.0f;
		}
		if (FVector2D(LocalPosition.X, LocalPosition.Y).SizeSquared() > FMath::Square(SafeRadius))
		{
			LocalPosition.X = FMath::Fmod(LocalPosition.X + SafeRadius * 3.0f, SafeRadius * 2.0f) - SafeRadius;
			LocalPosition.Y = FMath::Fmod(LocalPosition.Y + SafeRadius * 3.0f, SafeRadius * 2.0f) - SafeRadius;
		}
	}

	const bool bForceSurfaceRefresh = bPoolRebuilt
		|| bForceCeilingCacheRefresh
		|| (ActiveCount > 0 && PreviousActiveStreakCount <= 0);
	UpdateCeilingSurfaceCache(CameraLocation, ActiveCount, bForceSurfaceRefresh);
	bForceCeilingCacheRefresh = false;
	PreviousActiveStreakCount = ActiveCount;

	const float StreakHalfLength = FMath::Max(0.0f, StreakLengthCentimeters * 0.5f);
	for (int32 Index = 0; Index < StreakLocalPositions.Num(); ++Index)
	{
		const FVector& LocalPosition = StreakLocalPositions[Index];
		const FVector2D VisualVariation = StreakVisualScaleVariation.IsValidIndex(Index)
			? StreakVisualScaleVariation[Index]
			: FVector2D(1.0f, 1.0f);
		const FVector ToCamera = (-LocalPosition).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector FacingNormal = FVector::VectorPlaneProject(ToCamera, FallDirection)
			.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FQuat StreakRotation = FRotationMatrix::MakeFromYZ(FallDirection, FacingNormal).ToQuat();
		const bool bAboveBlockingSurface = !bClipStreaksAgainstCeilings
			|| (StreakSurfaceCacheValid.IsValidIndex(Index) && StreakSurfaceCacheValid[Index]
			&& ShouldRenderStreakAboveSurface(
				CameraLocation.Z + LocalPosition.Z,
				StreakHalfLength * VisualVariation.Y,
				StreakHasBlockingSurface.IsValidIndex(Index) && StreakHasBlockingSurface[Index] != 0,
				StreakBlockingSurfaceWorldZ.IsValidIndex(Index) ? StreakBlockingSurfaceWorldZ[Index] : 0.0f,
				CeilingSurfaceClearanceCentimeters));
		const FVector Scale = Index < ActiveCount && bAboveBlockingSurface
			? FVector(
				StreakWidthCentimeters * VisualVariation.X / 100.0f,
				StreakLengthCentimeters * VisualVariation.Y / 100.0f,
				1.0f)
			: FVector::ZeroVector;
		StreakTransforms[Index] = FTransform(StreakRotation, LocalPosition, Scale);
	}
	// 같은 ISM에 대한 개별 갱신을 묶고 렌더 상태 변경도 한 번만 알린다.
	RainStreaks->BatchUpdateInstancesTransforms(0, StreakTransforms, false, true, true);
}
