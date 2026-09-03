#include "AI/DroneSmartObjectReservationComponent.h"

#include "Drone.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "SmartObjectSubsystem.h"

namespace
{
	struct FDroneSmartObjectCandidate
	{
		FSmartObjectRequestResult Result;
		FTransform Transform = FTransform::Identity;
		double DistanceSquared = TNumericLimits<double>::Max();
	};
}

UDroneSmartObjectReservationComponent::UDroneSmartObjectReservationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// StateTree 기반 Gameplay Interaction Definition만 기본 후보로 받는다.
	BehaviorDefinitionClasses.Add(UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass());
}

bool UDroneSmartObjectReservationComponent::ClaimNearestAvailableSlot(
	const FVector& SearchOrigin,
	FTransform& OutSlotTransform)

{
	return ClaimNearestAvailableSlotInternal(SearchOrigin, nullptr, 0.0f, OutSlotTransform);
}

bool UDroneSmartObjectReservationComponent::ClaimNearestAvailableSlotAvoiding(
	const FVector& SearchOrigin,
	const FVector& AvoidLocation,
	const float AvoidRadius,
	FTransform& OutSlotTransform)
{
	if (ClaimNearestAvailableSlotInternal(
		SearchOrigin,
		&AvoidLocation,
		FMath::Max(0.0f, AvoidRadius),
		OutSlotTransform))
	{
		return true;
	}

	// 지점이 하나뿐이거나 다른 모든 지점을 다른 NPC가 사용 중일 때는
	// 직전 지점 재사용을 허용해 순찰 전체가 영구 정지하지 않게 한다.
	return ClaimNearestAvailableSlotInternal(SearchOrigin, nullptr, 0.0f, OutSlotTransform);
}

bool UDroneSmartObjectReservationComponent::ClaimNearestAvailableSlotInternal(
	const FVector& SearchOrigin,
	const FVector* AvoidLocation,
	const float AvoidRadius,
	FTransform& OutSlotTransform)
{
	OutSlotTransform = FTransform::Identity;

	if (HasValidReservation())
	{
		return GetReservedSlotTransform(OutSlotTransform);
	}

	if (RequiredActivityTags.IsEmpty())
	{
		UE_LOG(
			LogDrone,
			Warning,
			TEXT("Smart Object reservation on '%s' requires at least one Activity Tag."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!Subsystem)
	{
		UE_LOG(LogDrone, Warning, TEXT("Smart Object subsystem is unavailable for '%s'."), *GetNameSafe(GetOwner()));
		return false;
	}

	FSmartObjectRequestFilter Filter;
	Filter.UserTags = UserTags;
	Filter.ClaimPriority = ClaimPriority;
	Filter.ActivityRequirements = FGameplayTagQuery::MakeQuery_MatchAnyTags(RequiredActivityTags);
	Filter.BehaviorDefinitionClasses = BehaviorDefinitionClasses;
	Filter.bShouldEvaluateConditions = true;
	Filter.bShouldIncludeClaimedSlots = false;

	const FVector QueryExtent(SearchRadius, SearchRadius, SearchHalfHeight);
	const FSmartObjectRequest Request(FBox::BuildAABB(SearchOrigin, QueryExtent), Filter);
	const FSmartObjectActorUserData UserData(GetOwner());

	TArray<FSmartObjectRequestResult> Results;
	Subsystem->FindSmartObjects(Request, Results, FConstStructView::Make(UserData));

	TArray<FDroneSmartObjectCandidate> Candidates;
	Candidates.Reserve(Results.Num());

	for (const FSmartObjectRequestResult& Result : Results)
	{
		const TOptional<FTransform> SlotTransform = Subsystem->GetSlotTransform(Result);
		if (!SlotTransform.IsSet())
		{
			continue;
		}
		if (AvoidLocation
			&& FVector::DistSquared2D(*AvoidLocation, SlotTransform->GetLocation()) < FMath::Square(AvoidRadius))
		{
			continue;
		}

		FDroneSmartObjectCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Result = Result;
		Candidate.Transform = SlotTransform.GetValue();
		Candidate.DistanceSquared = FVector::DistSquared(SearchOrigin, Candidate.Transform.GetLocation());
	}

	Candidates.Sort([](const FDroneSmartObjectCandidate& Left, const FDroneSmartObjectCandidate& Right)
	{
		return Left.DistanceSquared < Right.DistanceSquared;
	});

	// 검색과 Claim 사이에 다른 AI가 먼저 예약할 수 있으므로 가까운 순서대로 Claim을 재시도한다.
	for (const FDroneSmartObjectCandidate& Candidate : Candidates)
	{
		FSmartObjectClaimHandle NewClaim = Subsystem->MarkSlotAsClaimed(
			Candidate.Result.SlotHandle,
			ClaimPriority,
			FConstStructView::Make(UserData));

		if (!NewClaim.IsValid())
		{
			continue;
		}

		ClaimHandle = MoveTemp(NewClaim);
		CachedSlotTransform = Candidate.Transform;
		OutSlotTransform = CachedSlotTransform;
		OnReservationChanged.Broadcast(true, CachedSlotTransform);
		return true;
	}

	return false;
}

bool UDroneSmartObjectReservationComponent::MarkReservationOccupied()
{
	USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!Subsystem || !Subsystem->IsClaimedSmartObjectValid(ClaimHandle) || BehaviorDefinitionClasses.IsEmpty())
	{
		return false;
	}

	return Subsystem->MarkSlotAsOccupied(ClaimHandle, BehaviorDefinitionClasses[0]) != nullptr;
}

bool UDroneSmartObjectReservationComponent::ReleaseReservation()
{
	if (!ClaimHandle.IsValid())
	{
		return false;
	}

	bool bReleased = false;
	if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld()))
	{
		bReleased = Subsystem->MarkSlotAsFree(ClaimHandle);
	}

	ClaimHandle.Invalidate();
	CachedSlotTransform = FTransform::Identity;
	OnReservationChanged.Broadcast(false, CachedSlotTransform);
	return bReleased;
}

bool UDroneSmartObjectReservationComponent::HasValidReservation() const
{
	if (!ClaimHandle.IsValid())
	{
		return false;
	}

	const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	return Subsystem && Subsystem->IsClaimedSmartObjectValid(ClaimHandle);
}

bool UDroneSmartObjectReservationComponent::IsReservationOccupied() const
{
	const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	return Subsystem
		&& HasValidReservation()
		&& Subsystem->GetSlotState(ClaimHandle.SlotHandle) == ESmartObjectSlotState::Occupied;
}

AActor* UDroneSmartObjectReservationComponent::GetReservedSmartObjectActor() const
{
	const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!Subsystem || !HasValidReservation())
	{
		return nullptr;
	}

	// Level에 배치된 USmartObjectComponent는 Owner Actor를 이 표준 데이터로 등록한다.
	const FConstStructView OwnerData = Subsystem->GetOwnerData(ClaimHandle.SmartObjectHandle);
	const FSmartObjectActorUserData* ActorData = OwnerData.GetPtr<const FSmartObjectActorUserData>();
	return ActorData ? const_cast<AActor*>(ActorData->UserActor.Get()) : nullptr;
}

bool UDroneSmartObjectReservationComponent::GetReservedSlotTransform(FTransform& OutSlotTransform) const
{
	OutSlotTransform = FTransform::Identity;
	const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!Subsystem || !Subsystem->IsClaimedSmartObjectValid(ClaimHandle))
	{
		return false;
	}

	return Subsystem->GetSlotTransform(ClaimHandle, OutSlotTransform);
}

void UDroneSmartObjectReservationComponent::SetRequiredActivityTags(const FGameplayTagContainer& NewActivityTags)
{
	RequiredActivityTags = NewActivityTags;
}

void UDroneSmartObjectReservationComponent::SetUserTags(const FGameplayTagContainer& NewUserTags)
{
	UserTags = NewUserTags;
}

void UDroneSmartObjectReservationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseReservation();
	Super::EndPlay(EndPlayReason);
}
