#include "UI/DroneGamepadFocus.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Drone.h"
#include "GameFramework/PlayerController.h"

namespace DroneGamepadFocus
{
// 새로 보인 위젯이 배치될 때까지 기다리는 최대 프레임. 넘으면 포기하고 로그만 남긴다.
constexpr int32 MaxPendingAttempts = 30;
}

void FDroneGamepadFocus::RequestFocus(const TArray<UWidget*>& Candidates)
{
	PendingCandidates.Reset();
	for (UWidget* Candidate : Candidates)
	{
		if (Candidate)
		{
			PendingCandidates.Add(Candidate);
		}
	}
	PendingAttempts = 0;
}

void FDroneGamepadFocus::CancelPendingFocus()
{
	PendingCandidates.Reset();
	PendingAttempts = 0;
}

bool FDroneGamepadFocus::IsNavigable(const UWidget* Widget)
{
	const UWidget* Current = Widget;
	while (Current)
	{
		const ESlateVisibility Visibility = Current->GetVisibility();
		if (!Current->GetIsEnabled() || Visibility == ESlateVisibility::Collapsed || Visibility == ESlateVisibility::Hidden)
		{
			return false;
		}
		if (const UPanelWidget* Parent = Current->GetParent())
		{
			Current = Parent;
			continue;
		}
		// WidgetTree의 맨 위에 닿으면 그 트리를 가진 UserWidget(다른 위젯 안에 들어 있으면 그 위젯)으로 올라간다.
		const UUserWidget* OwnerWidget = Current->GetTypedOuter<UUserWidget>();
		Current = OwnerWidget != Current ? OwnerWidget : nullptr;
	}
	return Widget != nullptr;
}

bool FDroneGamepadFocus::HasFocus(const UWidget* Widget, APlayerController* Player)
{
	return Widget && ((Player && Widget->HasUserFocus(Player)) || Widget->HasKeyboardFocus());
}

UWidget* FDroneGamepadFocus::FindFocused(APlayerController* Player, const TArray<UWidget*>& Candidates)
{
	for (UWidget* Candidate : Candidates)
	{
		if (HasFocus(Candidate, Player))
		{
			return Candidate;
		}
	}
	return nullptr;
}

void FDroneGamepadFocus::Tick(APlayerController* Player, const TArray<UWidget*>& Highlightable)
{
	if (!PendingCandidates.IsEmpty() && Player)
	{
		UWidget* Target = nullptr;
		for (const TWeakObjectPtr<UWidget>& Candidate : PendingCandidates)
		{
			if (Candidate.IsValid() && IsNavigable(Candidate.Get()))
			{
				Target = Candidate.Get();
				break;
			}
		}
		if (Target)
		{
			Target->SetUserFocus(Player);
			if (HasFocus(Target, Player))
			{
				CancelPendingFocus();
			}
		}
		if (!PendingCandidates.IsEmpty() && ++PendingAttempts > DroneGamepadFocus::MaxPendingAttempts)
		{
			UE_LOG(LogDrone, Warning, TEXT("[UI-PAD] %d프레임 동안 초기 포커스를 잡지 못했다(대상 %s)."),
				DroneGamepadFocus::MaxPendingAttempts, *GetNameSafe(Target));
			CancelPendingFocus();
		}
	}

	UWidget* Focused = FindFocused(Player, Highlightable);
	if (Focused != Highlighted.Get())
	{
		ClearHighlight();
		if (Focused)
		{
			ApplyHighlight(Focused, true);
			Highlighted = Focused;
		}
	}
}

void FDroneGamepadFocus::ClearHighlight()
{
	if (UWidget* Previous = Highlighted.Get())
	{
		ApplyHighlight(Previous, false);
	}
	Highlighted.Reset();
}

void FDroneGamepadFocus::ApplyHighlight(UWidget* Widget, const bool bHighlighted) const
{
	// 그리기 변환만 바꾸므로 주변 위젯 배치와 글자 줄바꿈은 그대로다.
	Widget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Widget->SetRenderScale(bHighlighted ? FVector2D(FocusScale, FocusScale) : FVector2D(1.0f, 1.0f));
	if (UButton* Button = Cast<UButton>(Widget))
	{
		Button->SetColorAndOpacity(bHighlighted ? FocusTint : FLinearColor::White);
	}
}
