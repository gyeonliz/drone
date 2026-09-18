#include "AI/DroneNPCCharacter.h"

#include "Drone.h"
#include "AI/DroneNPCAIController.h"
#include "AI/DroneNPCProfileComponent.h"
#include "AI/Weapons/DroneNPCWeaponComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Health/DroneHealthComponent.h"
#include "SmartObjectUserComponent.h"
#include "UObject/ConstructorHelpers.h"

ADroneNPCCharacter::ADroneNPCCharacter()
{
	// CharacterMovement와 Animation Component가 각자 갱신되므로 Actor Tick은 별도로 쓰지 않는다.
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = ADroneNPCAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 순찰·엄폐·추적 이동은 이동 방향을 바라본다. 정지 사격 상태의 몸 Yaw만
		// AI Controller가 직접 보간해 두 회전 작성자가 동시에 경쟁하지 않게 한다.
		Movement->bUseControllerDesiredRotation = false;
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	}

	NPCProfileComponent = CreateDefaultSubobject<UDroneNPCProfileComponent>(TEXT("NPCProfileComponent"));
	SmartObjectUserComponent = CreateDefaultSubobject<USmartObjectUserComponent>(TEXT("SmartObjectUserComponent"));
	NPCWeaponComponent = CreateDefaultSubobject<UDroneNPCWeaponComponent>(TEXT("NPCWeaponComponent"));
	HealthComponent = CreateDefaultSubobject<UDroneHealthComponent>(TEXT("HealthComponent"));

	// 최종 Weapon Actor/Inventory가 정해지기 전, 역할 BP가 외형만 교체하는 안전한 경계다.
	WeaponVisualComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponVisualComponent"));
	WeaponVisualComponent->SetupAttachment(GetMesh(), WeaponAttachPointName);
	WeaponVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponVisualComponent->SetGenerateOverlapEvents(false);
	WeaponVisualComponent->SetCanEverAffectNavigation(false);
	WeaponVisualComponent->PrimaryComponentTick.bCanEverTick = false;

	// Mesh Pivot과 실제 총구 위치가 다를 수 있으므로 별도 자식 Component로 둔다.
	WeaponMuzzleComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponMuzzleComponent"));
	WeaponMuzzleComponent->SetupAttachment(WeaponVisualComponent);
	WeaponMuzzleComponent->PrimaryComponentTick.bCanEverTick = false;

	// 프로젝트에 이미 포함된 Manny Rifle Sequence를 에셋 구매 전 공통 임시 표현으로 사용한다.
	// 역할 Blueprint는 아래 포인터만 교체하거나 bUseGreyboxWeaponAnimations를 끌 수 있다.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> MannyRifleFireAsset(
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire.MM_Rifle_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> MannyRifleReloadAsset(
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
	if (MannyRifleFireAsset.Succeeded())
	{
		RifleFireAnimation = MannyRifleFireAsset.Object;
		ShotgunFireAnimation = MannyRifleFireAsset.Object;
	}
	if (MannyRifleReloadAsset.Succeeded())
	{
		RifleReloadAnimation = MannyRifleReloadAsset.Object;
		ShotgunReloadAnimation = MannyRifleReloadAsset.Object;
	}
}

void ADroneNPCCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnforceVisualOnlyCollision();
	RefreshWeaponVisualAttachment();
}

void ADroneNPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 기존 역할 Blueprint에 저장된 CharacterMovement 값이 C++ 생성자 기본값을 덮어쓸 수 있다.
	// 순찰·추적의 단일 회전 작성자는 항상 이동 벡터가 되도록 런타임 계약을 재적용한다.
	// RotationRate는 역할 Blueprint의 조정값을 보존한다.
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bUseControllerDesiredRotation = false;
		Movement->bOrientRotationToMovement = true;
	}
	EnforceVisualOnlyCollision();
	RefreshWeaponVisualAttachment();

	// Weapon Component의 판정 결과를 Character BP의 표현 Event로 한 번만 전달한다.
	NPCWeaponComponent->OnWeaponFired.AddUniqueDynamic(this, &ADroneNPCCharacter::HandleWeaponFiredVisual);
	NPCWeaponComponent->OnReloadCompleted.AddUniqueDynamic(this, &ADroneNPCCharacter::HandleReloadCompletedVisual);
	HealthComponent->OnDeath.AddDynamic(this, &ADroneNPCCharacter::HandleDeath);
}

void ADroneNPCCharacter::EnforceVisualOnlyCollision()
{
	const UCapsuleComponent* MovementCapsule = GetCapsuleComponent();
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
		if (!Component || Component == MovementCapsule)
		{
			continue;
		}

		if (Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision
			|| Component->GetGenerateOverlapEvents()
			|| Component->CanEverAffectNavigation())
		{
			UE_LOG(
				LogDrone,
				Display,
				TEXT("[NPC-COLLISION-FIX] pawn=%s component=%s collision=%d overlap=%d nav=%d -> VisualOnly"),
				*GetNameSafe(this),
				*GetNameSafe(Component),
				static_cast<int32>(Component->GetCollisionEnabled()),
				Component->GetGenerateOverlapEvents() ? 1 : 0,
				Component->CanEverAffectNavigation() ? 1 : 0);
		}

		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
	}
}

void ADroneNPCCharacter::RefreshWeaponVisualAttachment()
{
	if (!WeaponVisualComponent || !GetMesh())
	{
		return;
	}

	// KeepRelativeTransform으로 역할 BP에서 조정한 위치·회전·Scale을 보존한다.
	WeaponVisualComponent->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::KeepRelativeTransform,
		WeaponAttachPointName);
}

UAnimSequenceBase* ADroneNPCCharacter::GetGreyboxFireAnimation(
	const EDroneNPCWeaponType WeaponType) const
{
	if (WeaponType == EDroneNPCWeaponType::Rifle)
	{
		return RifleFireAnimation;
	}
	if (WeaponType == EDroneNPCWeaponType::Shotgun)
	{
		return ShotgunFireAnimation;
	}
	return nullptr;
}

UAnimSequenceBase* ADroneNPCCharacter::GetGreyboxReloadAnimation(
	const EDroneNPCWeaponType WeaponType) const
{
	if (WeaponType == EDroneNPCWeaponType::Rifle)
	{
		return RifleReloadAnimation;
	}
	if (WeaponType == EDroneNPCWeaponType::Shotgun)
	{
		return ShotgunReloadAnimation;
	}
	return nullptr;
}

void ADroneNPCCharacter::PlayGreyboxWeaponAnimation(UAnimSequenceBase* Animation, const float PlayRate)
{
	if (!bUseGreyboxWeaponAnimations || !Animation || GreyboxWeaponAnimationSlotName.IsNone() || !GetMesh())
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		// Dynamic Montage는 원본 Sequence Asset을 수정하지 않는다. 판정은 이미 Weapon Component에서 끝났고
		// 여기서는 소총 자세 AnimBP의 DefaultSlot에 짧은 표현만 겹친다.
		// 같은 발사 Sequence가 재생 중이면 다음 탄의 표현은 건너뛴다. 연사 Cooldown마다 Montage를
		// 처음부터 되감으면 가산 자세가 급격히 왕복해 캐릭터 전체가 떨려 보일 수 있다.
		if (AnimInstance->IsPlayingSlotAnimation(Animation, GreyboxWeaponAnimationSlotName))
		{
			return;
		}

		AnimInstance->PlaySlotAnimationAsDynamicMontage(
			Animation,
			GreyboxWeaponAnimationSlotName,
			FMath::Max(0.0f, GreyboxAnimationBlendInSeconds),
			FMath::Max(0.0f, GreyboxAnimationBlendOutSeconds),
			FMath::Max(0.01f, PlayRate),
			1);
	}
}

void ADroneNPCCharacter::HandleWeaponFiredVisual(
	const EDroneNPCWeaponType WeaponType,
	const FVector TraceStart,
	const FVector AimPoint)
{
	PlayGreyboxWeaponAnimation(GetGreyboxFireAnimation(WeaponType), GreyboxFireAnimationPlayRate);

	FTransform MuzzleTransform = FTransform::Identity;
	if (WeaponMuzzleComponent)
	{
		MuzzleTransform = WeaponMuzzleComponent->GetComponentTransform();
	}
	else
	{
		// Component가 예상치 못하게 없을 때에도 BP가 사용할 수 있는 최소 방향을 제공한다.
		const FVector AimDirection = (AimPoint - TraceStart).GetSafeNormal();
		MuzzleTransform = FTransform(AimDirection.IsNearlyZero() ? FRotator::ZeroRotator : AimDirection.Rotation(), TraceStart);
	}

	ReceiveWeaponFiredVisual(WeaponType, MuzzleTransform, AimPoint);
}

void ADroneNPCCharacter::HandleReloadCompletedVisual(
	const EDroneNPCWeaponType WeaponType,
	const int32 CurrentAmmo,
	const int32 MagazineCapacity)
{
	PlayGreyboxWeaponAnimation(GetGreyboxReloadAnimation(WeaponType), GreyboxReloadAnimationPlayRate);
	ReceiveReloadCompletedVisual(WeaponType, CurrentAmmo, MagazineCapacity);
}

void ADroneNPCCharacter::HandleDeath(
	AActor* DeadActor,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	if (DeadActor != this)
	{
		return;
	}

	// 회색상자 단계에서는 시체를 제거하거나 래그돌로 바꾸지 않는다. 이동·충돌·AI만
	// 확실히 정지해 이후 Animation/Respawn 규칙을 Blueprint에서 안전하게 추가한다.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	SetActorEnableCollision(false);

	if (ADroneNPCAIController* DroneController = Cast<ADroneNPCAIController>(GetController()))
	{
		DroneController->HandlePossessedPawnDeath();
	}
}
