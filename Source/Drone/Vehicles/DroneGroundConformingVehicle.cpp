#include "Vehicles/DroneGroundConformingVehicle.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

namespace DroneGroundVehicle
{
constexpr int32 FrontLeft = 0;
constexpr int32 FrontRight = 1;
constexpr int32 RearLeft = 2;
constexpr int32 RearRight = 3;
}

ADroneGroundConformingVehicle::ADroneGroundConformingVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->InitBoxExtent(FVector(145.0f, 88.0f, 28.0f));
	VehicleCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VehicleCollision->SetCollisionResponseToAllChannels(ECR_Block);
	VehicleCollision->SetSimulatePhysics(false);
	VehicleCollision->SetCanEverAffectNavigation(false);
	SetRootComponent(VehicleCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	VehicleBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleBodyMesh"));
	VehicleBodyMesh->SetupAttachment(VehicleCollision);
	VehicleBodyMesh->SetStaticMesh(CubeFinder.Object);
	VehicleBodyMesh->SetRelativeScale3D(FVector(2.8f, 1.65f, 0.48f));
	VehicleBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VehicleBodyMesh->SetCanEverAffectNavigation(false);

	auto CreateWheel = [this](const FName Name, const FVector& Location)
	{
		UStaticMeshComponent* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Wheel->SetupAttachment(VehicleCollision);
		Wheel->SetStaticMesh(CylinderFinder.Object);
		Wheel->SetRelativeLocation(Location);
		Wheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
		Wheel->SetRelativeScale3D(FVector(0.60f, 0.60f, 0.28f));
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wheel->SetCanEverAffectNavigation(false);
		return Wheel;
	};

	FrontLeftWheel = CreateWheel(TEXT("FrontLeftWheel"), FVector(HalfWheelbase, -HalfTrackWidth, -42.0f));
	FrontRightWheel = CreateWheel(TEXT("FrontRightWheel"), FVector(HalfWheelbase, HalfTrackWidth, -42.0f));
	RearLeftWheel = CreateWheel(TEXT("RearLeftWheel"), FVector(-HalfWheelbase, -HalfTrackWidth, -42.0f));
	RearRightWheel = CreateWheel(TEXT("RearRightWheel"), FVector(-HalfWheelbase, HalfTrackWidth, -42.0f));

	TurretMount = CreateDefaultSubobject<USceneComponent>(TEXT("TurretMount"));
	TurretMount->SetupAttachment(VehicleCollision);
	TurretMount->SetRelativeLocation(FVector(-20.0f, 0.0f, 52.0f));
}

void ADroneGroundConformingVehicle::BeginPlay()
{
	Super::BeginPlay();
	CaptureWheelVisualBaseRotations();
	InitializeDriveReference();
	RefreshGroundConformNow(true);
}

void ADroneGroundConformingVehicle::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	InitializeDriveReference();
	const FVector PreviousLocation = GetActorLocation();

	if (bGreyboxAutoDriveEnabled)
	{
		const float DistanceAlongRoute = FVector::DotProduct(
			GetActorLocation() - GreyboxAutoDriveOrigin,
			GreyboxAutoDriveForward);
		if (DistanceAlongRoute >= GreyboxAutoDriveDistance)
		{
			GreyboxAutoDriveDirection = -1.0f;
		}
		else if (DistanceAlongRoute <= 0.0f)
		{
			GreyboxAutoDriveDirection = 1.0f;
		}
		DriveThrottle = GreyboxAutoDriveDirection * FMath::Clamp(
			GreyboxAutoDriveSpeed / FMath::Max(MaximumDriveSpeed, 1.0f),
			0.0f,
			1.0f);
		DriveSteering = 0.0f;
	}

	DesiredHeadingYaw = FMath::UnwindDegrees(
		DesiredHeadingYaw + DriveSteering * MaximumTurnRateDegreesPerSecond * DeltaSeconds);
	UpdateGroundConforming(DeltaSeconds, false);
	UpdateWheelRollingVisuals(PreviousLocation, GetActorLocation(), DeltaSeconds);
}

TArray<UStaticMeshComponent*> ADroneGroundConformingVehicle::GetWheelMeshes() const
{
	return {FrontLeftWheel, FrontRightWheel, RearLeftWheel, RearRightWheel};
}

void ADroneGroundConformingVehicle::SetDriveInput(const float Throttle, const float Steering)
{
	DriveThrottle = FMath::Clamp(Throttle, -1.0f, 1.0f);
	DriveSteering = FMath::Clamp(Steering, -1.0f, 1.0f);
}

void ADroneGroundConformingVehicle::SetGreyboxAutoDriveEnabled(const bool bEnabled)
{
	bGreyboxAutoDriveEnabled = bEnabled;
	InitializeDriveReference();
	GreyboxAutoDriveOrigin = GetActorLocation();
	GreyboxAutoDriveForward = FRotator(0.0f, DesiredHeadingYaw, 0.0f).Vector();
	GreyboxAutoDriveDirection = 1.0f;
	if (!bEnabled)
	{
		DriveThrottle = 0.0f;
		DriveSteering = 0.0f;
	}
}

bool ADroneGroundConformingVehicle::RefreshGroundConformNow(const bool bSnapToGround)
{
	InitializeDriveReference();
	return UpdateGroundConforming(0.0f, bSnapToGround);
}

void ADroneGroundConformingVehicle::InitializeDriveReference()
{
	if (bDriveReferenceInitialized)
	{
		return;
	}

	DesiredHeadingYaw = GetActorRotation().Yaw;
	GreyboxAutoDriveOrigin = GetActorLocation();
	GreyboxAutoDriveForward = FRotator(0.0f, DesiredHeadingYaw, 0.0f).Vector();
	bDriveReferenceInitialized = true;
}

bool ADroneGroundConformingVehicle::UpdateGroundConforming(
	const float DeltaSeconds,
	const bool bSnapToGround)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector CandidateLocation = GetActorLocation();
	const FRotator HeadingRotation(0.0f, DesiredHeadingYaw, 0.0f);
	CandidateLocation += HeadingRotation.Vector() * DriveThrottle * MaximumDriveSpeed * DeltaSeconds;

	const FVector LocalSamples[4] = {
		FVector(HalfWheelbase, -HalfTrackWidth, 0.0f),
		FVector(HalfWheelbase, HalfTrackWidth, 0.0f),
		FVector(-HalfWheelbase, -HalfTrackWidth, 0.0f),
		FVector(-HalfWheelbase, HalfTrackWidth, 0.0f),
	};
	FGroundContact Contacts[4];
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneGroundConformingVehicle), false, this);
	QueryParams.AddIgnoredActor(this);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);
	QueryParams.AddIgnoredActors(AttachedActors);

	LastGroundContactCount = 0;
	FVector AveragePoint = FVector::ZeroVector;
	FVector AverageNormal = FVector::ZeroVector;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(LocalSamples); ++Index)
	{
		const FVector SampleOffset = HeadingRotation.RotateVector(LocalSamples[Index]);
		const FVector TraceStart(
			CandidateLocation.X + SampleOffset.X,
			CandidateLocation.Y + SampleOffset.Y,
			CandidateLocation.Z + TraceStartHeight);
		const FVector TraceEnd = TraceStart - FVector::UpVector * TraceLength;
		FHitResult Hit;
		Contacts[Index].bBlockingHit = World->LineTraceSingleByChannel(
			Hit,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);
		if (Contacts[Index].bBlockingHit)
		{
			Contacts[Index].Point = Hit.ImpactPoint;
			Contacts[Index].Normal = Hit.ImpactNormal.GetSafeNormal();
			AveragePoint += Contacts[Index].Point;
			AverageNormal += Contacts[Index].Normal;
			++LastGroundContactCount;
		}
	}

	if (LastGroundContactCount < 3)
	{
		// 지면을 잃어도 X/Y 이동과 Yaw 입력은 유지하되, 높이·기울기는 임의로 만들지 않는다.
		CandidateLocation.Z = GetActorLocation().Z;
		const FRotator FallbackRotation(
			GetActorRotation().Pitch,
			DesiredHeadingYaw,
			GetActorRotation().Roll);
		SetActorLocationAndRotation(CandidateLocation, FallbackRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return false;
	}

	AveragePoint /= static_cast<float>(LastGroundContactCount);
	FVector GroundUp = AverageNormal.GetSafeNormal();
	if (LastGroundContactCount == 4)
	{
		// 개별 충돌면의 Normal이 모두 Up인 계단형 Greybox에서도 네 접점의 실제
		// 높이 차이를 이용해 차체 Pitch/Roll을 만든다.
		const FVector FrontCenter = (Contacts[DroneGroundVehicle::FrontLeft].Point
			+ Contacts[DroneGroundVehicle::FrontRight].Point) * 0.5f;
		const FVector RearCenter = (Contacts[DroneGroundVehicle::RearLeft].Point
			+ Contacts[DroneGroundVehicle::RearRight].Point) * 0.5f;
		const FVector LeftCenter = (Contacts[DroneGroundVehicle::FrontLeft].Point
			+ Contacts[DroneGroundVehicle::RearLeft].Point) * 0.5f;
		const FVector RightCenter = (Contacts[DroneGroundVehicle::FrontRight].Point
			+ Contacts[DroneGroundVehicle::RearRight].Point) * 0.5f;
		const FVector ContactPlaneUp = FVector::CrossProduct(
			(FrontCenter - RearCenter).GetSafeNormal(),
			(RightCenter - LeftCenter).GetSafeNormal()).GetSafeNormal();
		if (!ContactPlaneUp.IsNearlyZero())
		{
			GroundUp = ContactPlaneUp.Z >= 0.0f ? ContactPlaneUp : -ContactPlaneUp;
		}
	}
	if (GroundUp.IsNearlyZero() || GroundUp.Z < 0.05f)
	{
		GroundUp = FVector::UpVector;
	}

	const FVector FlatForward = HeadingRotation.Vector();
	FVector GroundForward = FVector::VectorPlaneProject(FlatForward, GroundUp).GetSafeNormal();
	if (GroundForward.IsNearlyZero())
	{
		GroundForward = FlatForward;
	}
	FRotator TargetRotation = FRotationMatrix::MakeFromXZ(GroundForward, GroundUp).Rotator();
	TargetRotation.Pitch = FMath::Clamp(
		FMath::UnwindDegrees(TargetRotation.Pitch),
		-MaximumGroundAngleDegrees,
		MaximumGroundAngleDegrees);
	TargetRotation.Roll = FMath::Clamp(
		FMath::UnwindDegrees(TargetRotation.Roll),
		-MaximumGroundAngleDegrees,
		MaximumGroundAngleDegrees);
	TargetRotation.Yaw = DesiredHeadingYaw;

	const float TargetZ = AveragePoint.Z + RideHeight;
	CandidateLocation.Z = bSnapToGround
		? TargetZ
		: FMath::FInterpTo(GetActorLocation().Z, TargetZ, DeltaSeconds, HeightInterpolationSpeed);
	const FRotator AppliedRotation = bSnapToGround
		? TargetRotation
		: FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaSeconds, RotationInterpolationSpeed);
	SetActorLocationAndRotation(CandidateLocation, AppliedRotation, false, nullptr, ETeleportType::TeleportPhysics);
	UpdateWheelVisuals(Contacts);
	return true;
}

void ADroneGroundConformingVehicle::UpdateWheelVisuals(const FGroundContact (&Contacts)[4])
{
	UStaticMeshComponent* Wheels[4] = {
		FrontLeftWheel,
		FrontRightWheel,
		RearLeftWheel,
		RearRightWheel,
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Wheels); ++Index)
	{
		UStaticMeshComponent* Wheel = Wheels[Index];
		if (!Wheel || !Contacts[Index].bBlockingHit)
		{
			continue;
		}

		FVector RelativeLocation = Wheel->GetRelativeLocation();
		RelativeLocation.Z = GetActorTransform().InverseTransformPosition(Contacts[Index].Point).Z + WheelRadius;
		Wheel->SetRelativeLocation(RelativeLocation);
	}
}

void ADroneGroundConformingVehicle::CaptureWheelVisualBaseRotations()
{
	if (bWheelVisualBaseRotationsCaptured)
	{
		return;
	}

	const UStaticMeshComponent* Wheels[4] = {
		FrontLeftWheel,
		FrontRightWheel,
		RearLeftWheel,
		RearRightWheel,
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Wheels); ++Index)
	{
		if (Wheels[Index])
		{
			WheelVisualBaseRotations[Index] = Wheels[Index]->GetRelativeRotation().Quaternion();
		}
	}
	bWheelVisualBaseRotationsCaptured = true;
}

void ADroneGroundConformingVehicle::UpdateWheelRollingVisuals(
	const FVector& PreviousLocation,
	const FVector& CurrentLocation,
	const float DeltaSeconds)
{
	CaptureWheelVisualBaseRotations();
	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		CurrentForwardSpeedCentimetersPerSecond = 0.0f;
		return;
	}

	const FVector DriveForward = FRotator(0.0f, DesiredHeadingYaw, 0.0f).Vector();
	const float SignedForwardDistance = FVector::DotProduct(CurrentLocation - PreviousLocation, DriveForward);
	CurrentForwardSpeedCentimetersPerSecond = SignedForwardDistance / DeltaSeconds;
	CurrentWheelRotationDegrees += FMath::RadiansToDegrees(
		SignedForwardDistance / FMath::Max(WheelRadius, 1.0f));

	const FQuat LocalSpin(
		FVector::UpVector,
		FMath::DegreesToRadians(CurrentWheelRotationDegrees * WheelVisualSpinDirectionMultiplier));
	UStaticMeshComponent* Wheels[4] = {
		FrontLeftWheel,
		FrontRightWheel,
		RearLeftWheel,
		RearRightWheel,
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Wheels); ++Index)
	{
		if (Wheels[Index])
		{
			// 기본 Mesh 축 정렬 뒤 로컬 Cylinder 축 회전을 합성해 Suspension Z는 그대로 유지한다.
			Wheels[Index]->SetRelativeRotation(WheelVisualBaseRotations[Index] * LocalSpin);
		}
	}
}
