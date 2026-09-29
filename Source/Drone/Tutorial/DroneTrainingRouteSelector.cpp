#include "Tutorial/DroneTrainingRouteSelector.h"

#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Tutorial/DroneTrainingCourse.h"

namespace DroneTrainingRouteSelector
{
constexpr uint64 RouteMessageKey = 260929;
}

ADroneTrainingRouteSelector::ADroneTrainingRouteSelector()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
}

void ADroneTrainingRouteSelector::BeginPlay()
{
	Super::BeginPlay();
	RandomStream.Initialize(RandomSeed);
	BindTestInput();
	ActivateRouteNumber(FMath::Clamp(InitialRouteNumber, 1, FMath::Max(1, Routes.Num())));
}

void ADroneTrainingRouteSelector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		DisableInput(PlayerController);
	}
	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(DroneTrainingRouteSelector::RouteMessageKey);
	}
	Super::EndPlay(EndPlayReason);
}

void ADroneTrainingRouteSelector::ConfigureRoutes(const TArray<ADroneTrainingCourse*>& InRoutes)
{
	Routes.Reset();
	for (ADroneTrainingCourse* Route : InRoutes)
	{
		if (IsValid(Route))
		{
			Routes.Add(Route);
		}
	}
	ActiveRouteIndex = INDEX_NONE;
	if (HasActorBegunPlay() && !Routes.IsEmpty())
	{
		ActivateRouteNumber(FMath::Clamp(InitialRouteNumber, 1, Routes.Num()));
	}
}

bool ADroneTrainingRouteSelector::ActivateRouteNumber(const int32 RouteNumber)
{
	return ApplyActiveRouteIndex(RouteNumber - 1);
}

bool ADroneTrainingRouteSelector::ActivateRandomRoute()
{
	TArray<int32> CandidateIndices;
	for (int32 RouteIndex = 0; RouteIndex < Routes.Num(); ++RouteIndex)
	{
		if (!IsValid(Routes[RouteIndex]))
		{
			continue;
		}
		if (bAvoidImmediateRandomRepeat && Routes.Num() > 1 && RouteIndex == ActiveRouteIndex)
		{
			continue;
		}
		CandidateIndices.Add(RouteIndex);
	}

	if (CandidateIndices.IsEmpty())
	{
		return false;
	}
	return ApplyActiveRouteIndex(CandidateIndices[RandomStream.RandRange(0, CandidateIndices.Num() - 1)]);
}

ADroneTrainingCourse* ADroneTrainingRouteSelector::GetActiveRoute() const
{
	return Routes.IsValidIndex(ActiveRouteIndex) ? Routes[ActiveRouteIndex].Get() : nullptr;
}

bool ADroneTrainingRouteSelector::IsRouteActive(const int32 RouteNumber) const
{
	const int32 RouteIndex = RouteNumber - 1;
	return Routes.IsValidIndex(RouteIndex)
		&& RouteIndex == ActiveRouteIndex
		&& IsValid(Routes[RouteIndex])
		&& Routes[RouteIndex]->IsCourseRuntimeActive();
}

bool ADroneTrainingRouteSelector::ApplyActiveRouteIndex(const int32 NewActiveRouteIndex)
{
	if (!Routes.IsValidIndex(NewActiveRouteIndex) || !IsValid(Routes[NewActiveRouteIndex]))
	{
		return false;
	}

	for (int32 RouteIndex = 0; RouteIndex < Routes.Num(); ++RouteIndex)
	{
		if (ADroneTrainingCourse* Route = Routes[RouteIndex].Get(); IsValid(Route))
		{
			Route->SetCourseRuntimeActive(RouteIndex == NewActiveRouteIndex);
		}
	}

	ActiveRouteIndex = NewActiveRouteIndex;
	OnActiveRouteChanged.Broadcast(GetActiveRouteNumber(), GetActiveRoute());
	ShowRouteMessage();
	return true;
}

void ADroneTrainingRouteSelector::BindTestInput()
{
	if (bInputBound)
	{
		return;
	}

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return;
	}

	EnableInput(PlayerController);
	if (!InputComponent)
	{
		return;
	}

	InputComponent->Priority = 20;
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ADroneTrainingRouteSelector::SelectRouteOne).bConsumeInput = true;
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ADroneTrainingRouteSelector::SelectRouteTwo).bConsumeInput = true;
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ADroneTrainingRouteSelector::SelectRouteThree).bConsumeInput = true;
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ADroneTrainingRouteSelector::SelectRouteFour).bConsumeInput = true;
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ADroneTrainingRouteSelector::SelectRandomRoute).bConsumeInput = true;
	bInputBound = true;
}

void ADroneTrainingRouteSelector::ShowRouteMessage() const
{
	if (!GEngine)
	{
		return;
	}
	GEngine->AddOnScreenDebugMessage(
		DroneTrainingRouteSelector::RouteMessageKey,
		-1.0f,
		FColor::Cyan,
		FString::Printf(TEXT("Training Route %d Active | 1-4: Select | 5: Random"), GetActiveRouteNumber()));
}

void ADroneTrainingRouteSelector::SelectRouteOne() { ActivateRouteNumber(1); }
void ADroneTrainingRouteSelector::SelectRouteTwo() { ActivateRouteNumber(2); }
void ADroneTrainingRouteSelector::SelectRouteThree() { ActivateRouteNumber(3); }
void ADroneTrainingRouteSelector::SelectRouteFour() { ActivateRouteNumber(4); }
void ADroneTrainingRouteSelector::SelectRandomRoute() { ActivateRandomRoute(); }

