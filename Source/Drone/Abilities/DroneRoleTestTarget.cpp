#include "Abilities/DroneRoleTestTarget.h"

#include "Abilities/DronePayloadTargetComponent.h"
#include "Abilities/DroneReconScanTargetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Health/DroneHealthComponent.h"
#include "UObject/ConstructorHelpers.h"

ADroneRoleTestTarget::ADroneRoleTestTarget()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
	SetRootComponent(TargetMesh);
	TargetMesh->SetMobility(EComponentMobility::Static);
	TargetMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TargetMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TargetMesh->SetGenerateOverlapEvents(false);
	TargetMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		TargetVisualMesh = CubeMesh.Object;
	}

	InstructionText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InstructionText"));
	InstructionText->SetupAttachment(TargetMesh);
	InstructionText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	InstructionText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	InstructionText->SetWorldSize(30.0f);
	InstructionText->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	InstructionText->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	InstructionText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InstructionText->SetCanEverAffectNavigation(false);
	InstructionText->SetVisibility(false, true);
	RefreshPresentation();
}

void ADroneRoleTestTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshPresentation();
}

void ADroneRoleTestTarget::RefreshPresentation()
{
	TargetMesh->SetStaticMesh(TargetVisualMesh);
	TargetMesh->SetRelativeScale3D(TargetVisualScale);
	InstructionText->SetText(InstructionMessage);
	InstructionText->SetTextRenderColor(InstructionColor);
	InstructionText->SetWorldSize(FMath::Max(1.0f, InstructionWorldSize));
	InstructionText->SetRelativeLocation(InstructionRelativeLocation);
	InstructionText->SetRelativeRotation(InstructionRelativeRotation);
	InstructionText->SetVisibility(bShowInstructionText, true);
}

void ADroneRoleTestTarget::ConfigurePresentation(
	const FText& InText,
	const FColor& InColor,
	const FVector& InScale)
{
	TargetVisualScale = InScale;
	InstructionMessage = InText;
	InstructionColor = InColor;
	RefreshPresentation();
}

ADroneReconRoleTestTarget::ADroneReconRoleTestTarget()
{
	ReconTargetComponent = CreateDefaultSubobject<UDroneReconScanTargetComponent>(TEXT("ReconTargetComponent"));
	ConfigurePresentation(
		FText::FromString(TEXT("SCAN")),
		FColor(30, 225, 255),
		FVector(1.2f, 1.2f, 2.0f));
}

ADroneImpactRoleTestTarget::ADroneImpactRoleTestTarget()
{
	HealthComponent = CreateDefaultSubobject<UDroneHealthComponent>(TEXT("HealthComponent"));
	ConfigurePresentation(
		FText::FromString(TEXT("IMPACT")),
		FColor(255, 55, 40),
		FVector(2.0f, 2.0f, 2.0f));
}

ADronePayloadRoleTestTarget::ADronePayloadRoleTestTarget()
{
	PayloadTargetComponent = CreateDefaultSubobject<UDronePayloadTargetComponent>(TEXT("PayloadTargetComponent"));
	ConfigurePresentation(
		FText::FromString(TEXT("DROP")),
		FColor(255, 205, 30),
		FVector(2.5f, 2.5f, 0.15f));
}
