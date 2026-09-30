#include "Tutorial/DroneTrainingGate.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tutorial/DroneTrainingGateSequenceComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace DroneTrainingGate
{
constexpr int32 SerializedVisualSegmentCount = 16;
constexpr int32 VisibleFrameSegmentCount = 4;
constexpr float EngineCubeSizeCentimeters = 100.0f;
}

ADroneTrainingGate::ADroneTrainingGate()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	GateRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GateRoot"));
	// Runtime Spawn과 상태 Material 갱신에서도 안전하게 재구성할 수 있도록 Movable로 둔다.
	// 실제 이동 기능을 뜻하지 않으며 Tick·Physics·Navigation은 계속 사용하지 않는다.
	GateRoot->SetMobility(EComponentMobility::Movable);
	GateRoot->SetCanEverAffectNavigation(false);
	SetRootComponent(GateRoot);

	GateTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("GateTrigger"));
	GateTrigger->SetupAttachment(GateRoot);
	GateTrigger->SetMobility(EComponentMobility::Movable);
	GateTrigger->SetHiddenInGame(true);
	GateTrigger->OnComponentBeginOverlap.AddDynamic(this, &ADroneTrainingGate::HandleTriggerBeginOverlap);
	GateTrigger->OnComponentEndOverlap.AddDynamic(this, &ADroneTrainingGate::HandleTriggerEndOverlap);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		RingSegmentMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GuideMaterialFinder(
		TEXT("/Game/Drone/Tutorial/Materials/M_DroneTrainingGuide.M_DroneTrainingGuide"));
	if (GuideMaterialFinder.Succeeded())
	{
		RingMaterial = GuideMaterialFinder.Object;
	}

	RingVisualSegments.Reserve(DroneTrainingGate::SerializedVisualSegmentCount);
	for (int32 SegmentIndex = 0; SegmentIndex < DroneTrainingGate::SerializedVisualSegmentCount; ++SegmentIndex)
	{
		const FName SegmentName(*FString::Printf(TEXT("RingVisualSegment_%02d"), SegmentIndex));
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(SegmentName);
		Segment->SetupAttachment(GateRoot);
		Segment->SetMobility(EComponentMobility::Movable);
		RingVisualSegments.Add(Segment);
	}

	GateAssetVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateAssetVisual"));
	GateAssetVisual->SetupAttachment(GateRoot);
	GateAssetVisual->SetMobility(EComponentMobility::Movable);
	GateAssetVisual->SetVisibility(false);
	GateAssetVisual->SetHiddenInGame(true);

	ApplyComponentRules();
}

void ADroneTrainingGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyComponentRules();
	RefreshRingVisual();
	RefreshStateMaterial();
}

void ADroneTrainingGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PendingEntryLocations.Reset();
	AssignedGateSequence.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADroneTrainingGate::BeginPlay()
{
	Super::BeginPlay();
	// 기존 맵의 저장된 Component에도 최신 BP 메시/보정을 적용한다.
	// Course/Selector가 꺼 둔 Trigger나 Actor Collision은 다시 켜지 않는다.
	RefreshRingVisual();
	RefreshStateMaterial();
}

FVector ADroneTrainingGate::GetForwardDirectionWorld() const
{
	// Ring과 Box는 항상 Actor 로컬 YZ 평면에 놓이므로 정방향도 로컬 +X 하나로 고정한다.
	return GetActorForwardVector().GetSafeNormal();
}

void ADroneTrainingGate::ConfigureGateDefinition(
	const FName InCourseId,
	const int32 InGateIndex,
	const float InSegmentDistance)
{
	CourseId = InCourseId;
	GateIndex = FMath::Max(0, InGateIndex);
	SegmentDistance = FMath::Max(0.0f, InSegmentDistance);
}

void ADroneTrainingGate::SetGateVisualState(const EDroneTrainingGateVisualState NewState)
{
	GateVisualState = NewState;
	RefreshStateMaterial();
}

FLinearColor ADroneTrainingGate::GetColorForVisualState(const EDroneTrainingGateVisualState State) const
{
	if (State == EDroneTrainingGateVisualState::Current)
	{
		return CurrentColor;
	}
	if (State == EDroneTrainingGateVisualState::Completed)
	{
		return CompletedColor;
	}
	return InactiveColor;
}

void ADroneTrainingGate::SetGateStateColors(
	const FLinearColor BeforePassColor,
	const FLinearColor CurrentTargetColor,
	const FLinearColor AfterPassColor)
{
	InactiveColor = BeforePassColor;
	CurrentColor = CurrentTargetColor;
	CompletedColor = AfterPassColor;
	RefreshStateMaterial();
}

void ADroneTrainingGate::SetGateStateMaterials(UMaterialInterface* BeforePassMaterial,
	UMaterialInterface* CurrentTargetMaterial, UMaterialInterface* AfterPassMaterial)
{
	InactiveMaterial = BeforePassMaterial;
	CurrentMaterial = CurrentTargetMaterial;
	CompletedMaterial = AfterPassMaterial;
	RefreshStateMaterial();
}

void ADroneTrainingGate::SetGateAssetMesh(UStaticMesh* Mesh, const FTransform MeshLocalTransform)
{
	GateAssetMesh = Mesh;
	GateAssetLocalTransform = MeshLocalTransform;
	RefreshRingVisual();
	RefreshStateMaterial();
}

void ADroneTrainingGate::PlayAcceptedPassFeedback(AActor* PassingDrone)
{
	// Construction/Reset/색 변경에서는 호출하지 않는다. Audio가 비어도 BP 연출은 전달한다.
	++AcceptedPassFeedbackCount;
	if (GatePassSound && GatePassSoundVolume > 0.0f && GetWorld() && GetWorld()->IsGameWorld())
	{
		if (bGatePassSound2D)
		{
			UGameplayStatics::PlaySound2D(this, GatePassSound,
				GatePassSoundVolume, FMath::Max(GatePassSoundPitch, 0.1f));
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(this, GatePassSound, GetActorLocation(),
				GatePassSoundVolume, FMath::Max(GatePassSoundPitch, 0.1f));
		}
	}
	OnGatePassed(PassingDrone);
}

void ADroneTrainingGate::AssignGateSequence(UDroneTrainingGateSequenceComponent* InSequence)
{
	AssignedGateSequence = InSequence;
	PendingEntryLocations.Reset();
}

void ADroneTrainingGate::CancelPendingTraversals()
{
	PendingEntryLocations.Reset();
}

void ADroneTrainingGate::ApplyComponentRules()
{
	SetActorEnableCollision(true);
	if (GateRoot)
	{
		GateRoot->SetCanEverAffectNavigation(false);
	}

	// Visual은 보이기만 하며 Hit, Overlap, Physics, Nav 어느 쪽에도 참여하지 않는다.
	TArray<UStaticMeshComponent*> Visuals;
	for (UStaticMeshComponent* Segment : RingVisualSegments)
	{
		Visuals.Add(Segment);
	}
	Visuals.Add(GateAssetVisual);
	for (UStaticMeshComponent* Segment : Visuals)
	{
		if (!Segment)
		{
			continue;
		}

		Segment->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetGenerateOverlapEvents(false);
		Segment->SetSimulatePhysics(false);
		Segment->SetCanEverAffectNavigation(false);
		Segment->SetCastShadow(false);
		Segment->SetReceivesDecals(false);
	}

	// 판정은 이 Box 하나만 담당하며 Pawn 외 채널은 모두 무시한다.
	if (GateTrigger)
	{
		GateTrigger->SetCollisionObjectType(ECC_WorldDynamic);
		GateTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GateTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		GateTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		GateTrigger->SetGenerateOverlapEvents(true);
		GateTrigger->SetSimulatePhysics(false);
		GateTrigger->SetCanEverAffectNavigation(false);
	}
}

void ADroneTrainingGate::RefreshRingVisual()
{
	const float ApertureHalfSize = GetTriggerApertureHalfSizeCentimeters();
	const float Thickness = FMath::Max(RingThicknessCentimeters, 2.0f);
	const float OuterHalfSize = ApertureHalfSize + Thickness;
	const float BarCenterOffset = ApertureHalfSize + Thickness * 0.5f;
	const float CubeSize = DroneTrainingGate::EngineCubeSizeCentimeters;

	const FVector HorizontalBarScale(
		Thickness / CubeSize,
		(OuterHalfSize * 2.0f) / CubeSize,
		Thickness / CubeSize);
	const FVector VerticalBarScale(
		Thickness / CubeSize,
		Thickness / CubeSize,
		(ApertureHalfSize * 2.0f) / CubeSize);

	const FVector FramePositions[DroneTrainingGate::VisibleFrameSegmentCount] =
	{
		FVector(0.0f, 0.0f, BarCenterOffset),
		FVector(0.0f, 0.0f, -BarCenterOffset),
		FVector(0.0f, BarCenterOffset, 0.0f),
		FVector(0.0f, -BarCenterOffset, 0.0f)
	};

	for (int32 SegmentIndex = 0; SegmentIndex < RingVisualSegments.Num(); ++SegmentIndex)
	{
		UStaticMeshComponent* Segment = RingVisualSegments[SegmentIndex];
		if (!Segment)
		{
			continue;
		}

		Segment->SetStaticMesh(RingSegmentMesh);
		if (SegmentIndex < DroneTrainingGate::VisibleFrameSegmentCount)
		{
			Segment->SetRelativeLocationAndRotation(
				FramePositions[SegmentIndex],
				FRotator::ZeroRotator);
			Segment->SetRelativeScale3D(SegmentIndex < 2 ? HorizontalBarScale : VerticalBarScale);
			Segment->SetVisibility(!GateAssetMesh);
			Segment->SetHiddenInGame(GateAssetMesh != nullptr);
		}
		else
		{
			Segment->SetRelativeTransform(FTransform::Identity);
			Segment->SetVisibility(false);
			Segment->SetHiddenInGame(true);
		}
	}

	if (GateAssetVisual)
	{
		GateAssetVisual->SetStaticMesh(GateAssetMesh);
		GateAssetVisual->SetRelativeTransform(GateAssetLocalTransform);
		GateAssetVisual->SetVisibility(GateAssetMesh != nullptr);
		GateAssetVisual->SetHiddenInGame(GateAssetMesh == nullptr);
		// 메시를 바꿀 때 이전 Override가 새 자산의 기본 재질을 덮지 않도록 초기화한다.
		GateAssetVisual->EmptyOverrideMaterials();
		DynamicAssetMaterials.Reset();
	}

	if (GateTrigger)
	{
		GateTrigger->SetBoxExtent(FVector(
			FMath::Max(TriggerHalfDepthCentimeters, 1.0f),
			FMath::Max(TriggerHalfSizeCentimeters, 1.0f),
			FMath::Max(TriggerHalfSizeCentimeters, 1.0f)));
	}
}

UMaterialInterface* ADroneTrainingGate::GetMaterialForVisualState() const
{
	if (GateVisualState == EDroneTrainingGateVisualState::Current && CurrentMaterial)
	{
		return CurrentMaterial;
	}
	if (GateVisualState == EDroneTrainingGateVisualState::Completed && CompletedMaterial)
	{
		return CompletedMaterial;
	}
	if (GateVisualState == EDroneTrainingGateVisualState::Inactive && InactiveMaterial)
	{
		return InactiveMaterial;
	}
	return RingMaterial;
}

void ADroneTrainingGate::RefreshStateMaterial()
{
	UMaterialInterface* StateMaterial = GetMaterialForVisualState();
	const FLinearColor StateColor = GetColorForVisualState(GateVisualState);
	if (StateMaterial)
	{
		// 같은 상태의 반복 호출은 MID를 재사용한다. 상태별 재질이 다를 때만 부모를 교체한다.
		if (!DynamicRingMaterial || DynamicRingMaterial->Parent != StateMaterial)
		{
			DynamicRingMaterial = UMaterialInstanceDynamic::Create(StateMaterial, this);
		}
		if (DynamicRingMaterial && !StateColorParameterName.IsNone())
		{
			DynamicRingMaterial->SetVectorParameterValue(StateColorParameterName, StateColor);
		}
	}
	else
	{
		DynamicRingMaterial = nullptr;
	}
	for (UStaticMeshComponent* Segment : RingVisualSegments)
	{
		if (Segment)
		{
			Segment->SetMaterial(0, DynamicRingMaterial);
		}
	}

	if (!GateAssetVisual || !GateAssetMesh)
	{
		return;
	}
	const int32 SlotCount = GateAssetVisual->GetNumMaterials();
	DynamicAssetMaterials.SetNum(SlotCount);
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (!GateAssetStateMaterialSlots.IsEmpty() && !GateAssetStateMaterialSlots.Contains(SlotIndex))
		{
			// 지정하지 않은 금속/기둥 등의 슬롯은 원래 Asset 재질을 보존한다.
			GateAssetVisual->SetMaterial(SlotIndex, nullptr);
			DynamicAssetMaterials[SlotIndex] = nullptr;
			continue;
		}
		UMaterialInterface* ParentMaterial = StateMaterial ? StateMaterial : GateAssetMesh->GetMaterial(SlotIndex);
		UMaterialInstanceDynamic* DynamicMaterial = DynamicAssetMaterials[SlotIndex];
		if (ParentMaterial && (!DynamicMaterial || DynamicMaterial->Parent != ParentMaterial))
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(ParentMaterial, this);
		}
		if (!ParentMaterial)
		{
			DynamicMaterial = nullptr;
		}
		DynamicAssetMaterials[SlotIndex] = DynamicMaterial;
		if (DynamicMaterial && !StateColorParameterName.IsNone())
		{
			DynamicMaterial->SetVectorParameterValue(StateColorParameterName, StateColor);
		}
		GateAssetVisual->SetMaterial(SlotIndex, DynamicMaterial);
	}
}

void ADroneTrainingGate::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// Destroy된 Pawn의 weak key가 남아 있더라도 다음 Overlap에서 즉시 정리한다.
	for (auto EntryIt = PendingEntryLocations.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (!EntryIt.Key().IsValid())
		{
			EntryIt.RemoveCurrent();
		}
	}

	if (!IsValid(OtherActor) || !OtherActor->IsA<ADronePrototypePawn>())
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(OtherActor);
	if (!PendingEntryLocations.Contains(ActorKey))
	{
		PendingEntryLocations.Add(ActorKey, OtherActor->GetActorLocation());
	}
}

void ADroneTrainingGate::HandleTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	// Pending-kill 상태에서도 먼저 같은 weak key를 제거한 뒤 Actor 유효성을 판정한다.
	FVector EntryWorldLocation;
	const bool bHadPendingEntry = OtherActor
		&& PendingEntryLocations.RemoveAndCopyValue(TWeakObjectPtr<AActor>(OtherActor), EntryWorldLocation);
	for (auto EntryIt = PendingEntryLocations.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (!EntryIt.Key().IsValid())
		{
			EntryIt.RemoveCurrent();
		}
	}

	if (!bHadPendingEntry || !IsValid(OtherActor))
	{
		return;
	}

	if (UDroneTrainingGateSequenceComponent* Sequence = AssignedGateSequence.Get(); IsValid(Sequence))
	{
		Sequence->TryAcceptTraversal(
			this,
			OtherActor,
			EntryWorldLocation,
			OtherActor->GetActorLocation());
	}
}
