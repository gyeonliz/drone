#include "Health/DroneHealthComponent.h"

#include "GameFramework/Actor.h"

UDroneHealthComponent::UDroneHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetHealth();

	// Unreal 표준 ApplyDamage/TakeDamage 흐름을 한곳에서 받아 무기별 체력 계산 중복을 막는다.
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnTakeAnyDamage.AddDynamic(this, &UDroneHealthComponent::HandleOwnerAnyDamage);
	}
}

void UDroneHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnTakeAnyDamage.RemoveDynamic(this, &UDroneHealthComponent::HandleOwnerAnyDamage);
	}
	Super::EndPlay(EndPlayReason);
}

float UDroneHealthComponent::GetHealthNormalized() const
{
	return MaxHealth > UE_SMALL_NUMBER
		? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
		: 0.0f;
}

bool UDroneHealthComponent::ApplyHealthDamage(
	const float Damage,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	if (bDead || Damage <= 0.0f)
	{
		return false;
	}

	const float PreviousHealth = CurrentHealth;
	const float AppliedDamage = FMath::Min(Damage, CurrentHealth);
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaxHealth);
	const bool bReachedZero = CurrentHealth <= 0.0f;
	if (bReachedZero)
	{
		// HealthChanged 수신 UI도 같은 프레임에 사망 상태를 읽을 수 있게 먼저 확정한다.
		bDead = true;
	}
	OnHealthChanged.Broadcast(PreviousHealth, CurrentHealth, MaxHealth, AppliedDamage);

	if (bReachedZero)
	{
		// 여러 Pellet이나 동시 Trace가 들어와도 사망 규칙은 한 번만 실행한다.
		++DeathEventCount;
		OnDeath.Broadcast(GetOwner(), InstigatorController, DamageCauser);
	}
	return true;
}

void UDroneHealthComponent::ResetHealth()
{
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	CurrentHealth = MaxHealth;
	bDead = false;
	DeathEventCount = 0;
}

void UDroneHealthComponent::HandleOwnerAnyDamage(
	AActor* DamagedActor,
	const float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	if (DamagedActor == GetOwner())
	{
		ApplyHealthDamage(Damage, InstigatedBy, DamageCauser);
	}
}
