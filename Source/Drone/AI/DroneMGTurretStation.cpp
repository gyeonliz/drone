#include "AI/DroneMGTurretStation.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void ConfigureTemporaryTurretMesh(UStaticMeshComponent* Component, UStaticMesh* CylinderMesh)
{
	if (!Component)
	{
		return;
	}
	Component->SetStaticMesh(CylinderMesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetMobility(EComponentMobility::Movable);
}
}

ADroneMGTurretStation::ADroneMGTurretStation()
{
	Activity = EDroneSmartObjectActivity::MGTurret;
	MGTurretMuzzleOffset = FVector(110.0f, 0.0f, 0.0f);

	MGTurretBaseMount = CreateDefaultSubobject<USceneComponent>(TEXT("MGTurretBaseMount"));
	MGTurretBaseMount->SetupAttachment(StationRoot);

	MGTurretYawPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MGTurretYawPivot"));
	MGTurretYawPivot->SetupAttachment(MGTurretBaseMount);
	MGTurretYawPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));

	// 사수 조작점은 포탑 몸체와 같은 Yaw Pivot 아래에 둔다. 포탑이 돌면
	// 별도 회전값 계산 없이 뒤 위치와 몸 방향이 그대로 함께 회전한다.
	MGTurretOperatorAnchor = CreateDefaultSubobject<UArrowComponent>(TEXT("MGTurretOperatorAnchor"));
	MGTurretOperatorAnchor->SetupAttachment(MGTurretYawPivot);
	MGTurretOperatorAnchor->SetArrowColor(FColor::Green);
	MGTurretOperatorAnchor->ArrowSize = 1.25f;
	MGTurretOperatorAnchor->bIsScreenSizeScaled = true;

	MGTurretAimPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MGTurretAimPivot"));
	MGTurretAimPivot->SetupAttachment(MGTurretYawPivot);
	MGTurretAimPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));

	MGTurretMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("MGTurretMuzzle"));
	MGTurretMuzzle->SetupAttachment(MGTurretAimPivot);
	MGTurretMuzzle->SetRelativeLocation(MGTurretMuzzleOffset);

	MGTurretBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MGTurretBaseMesh"));
	MGTurretBaseMesh->SetupAttachment(MGTurretBaseMount);
	MGTurretBaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	MGTurretBaseMesh->SetRelativeScale3D(FVector(0.65f, 0.65f, 0.4f));

	MGTurretBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MGTurretBodyMesh"));
	MGTurretBodyMesh->SetupAttachment(MGTurretYawPivot);
	MGTurretBodyMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.35f));

	MGTurretBarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MGTurretBarrelMesh"));
	MGTurretBarrelMesh->SetupAttachment(MGTurretAimPivot);
	MGTurretBarrelMesh->SetRelativeLocation(FVector(55.0f, 0.0f, 0.0f));
	// Engine Cylinder의 길이 축은 +Z다. Pitch 90도로 눕혀 Turret 전방(+X)을 향하게 한다.
	MGTurretBarrelMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	MGTurretBarrelMesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 1.1f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* CylinderMesh = CylinderAsset.Succeeded() ? CylinderAsset.Object : nullptr;
	ConfigureTemporaryTurretMesh(MGTurretBaseMesh, CylinderMesh);
	ConfigureTemporaryTurretMesh(MGTurretBodyMesh, CylinderMesh);
	ConfigureTemporaryTurretMesh(MGTurretBarrelMesh, CylinderMesh);
	RefreshMGTurretOperatorAnchor();
}

FTransform ADroneMGTurretStation::GetMGTurretOperatorTransform() const
{
	return MGTurretOperatorAnchor
		? MGTurretOperatorAnchor->GetComponentTransform()
		: GetActorTransform();
}

void ADroneMGTurretStation::RefreshMGTurretOperatorAnchor()
{
	if (!MGTurretOperatorAnchor)
	{
		return;
	}

	const float SafeDistance = FMath::Max(10.0f, MGTurretOperatorDistance);
	const float YawPivotHeight = MGTurretYawPivot
		? MGTurretYawPivot->GetRelativeLocation().Z
		: 0.0f;
	MGTurretOperatorAnchor->SetRelativeLocation(FVector(
		-SafeDistance,
		MGTurretOperatorLateralOffset,
		MGTurretOperatorVerticalOffset - YawPivotHeight));
	// 상대 회전은 항상 0이다. Operator는 YawPivot의 실제 회전을 그대로 상속한다.
	MGTurretOperatorAnchor->SetRelativeRotation(FRotator::ZeroRotator);
}

void ADroneMGTurretStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 이전 범용 Station 버전에서 저장된 BP Component 템플릿이 옛 Attachment를
	// 유지할 수 있다. MG 전용 Actor 안에서만 3분할 계층을 재확정한다.
	const FAttachmentTransformRules KeepRelativeAttachment(EAttachmentRule::KeepRelative, false);
	if (MGTurretBaseMount && MGTurretBaseMount->GetAttachParent() != StationRoot)
	{
		MGTurretBaseMount->AttachToComponent(StationRoot, KeepRelativeAttachment);
	}
	if (MGTurretYawPivot && MGTurretYawPivot->GetAttachParent() != MGTurretBaseMount)
	{
		MGTurretYawPivot->AttachToComponent(MGTurretBaseMount, KeepRelativeAttachment);
	}
	if (MGTurretOperatorAnchor && MGTurretOperatorAnchor->GetAttachParent() != MGTurretYawPivot)
	{
		MGTurretOperatorAnchor->AttachToComponent(MGTurretYawPivot, KeepRelativeAttachment);
	}
	if (MGTurretAimPivot && MGTurretAimPivot->GetAttachParent() != MGTurretYawPivot)
	{
		MGTurretAimPivot->AttachToComponent(MGTurretYawPivot, KeepRelativeAttachment);
	}
	if (MGTurretMuzzle && MGTurretMuzzle->GetAttachParent() != MGTurretAimPivot)
	{
		MGTurretMuzzle->AttachToComponent(MGTurretAimPivot, KeepRelativeAttachment);
	}
	if (MGTurretBaseMesh && MGTurretBaseMesh->GetAttachParent() != MGTurretBaseMount)
	{
		MGTurretBaseMesh->AttachToComponent(MGTurretBaseMount, KeepRelativeAttachment);
	}
	if (MGTurretBodyMesh && MGTurretBodyMesh->GetAttachParent() != MGTurretYawPivot)
	{
		MGTurretBodyMesh->AttachToComponent(MGTurretYawPivot, KeepRelativeAttachment);
	}
	if (MGTurretBarrelMesh && MGTurretBarrelMesh->GetAttachParent() != MGTurretAimPivot)
	{
		MGTurretBarrelMesh->AttachToComponent(MGTurretAimPivot, KeepRelativeAttachment);
	}
	if (MGTurretMuzzle)
	{
		MGTurretMuzzle->SetRelativeLocation(MGTurretMuzzleOffset);
	}
	RefreshMGTurretOperatorAnchor();
}
