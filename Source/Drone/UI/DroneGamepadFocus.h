#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class APlayerController;
class UWidget;

/**
 * 패드(게임패드)로 UI를 조작하기 위한 공통 처리(UI-PAD-01).
 *
 * - 화면이 열릴 때 후보 중 첫 "조작 가능한" 위젯에 포커스를 준다. 방금 보이게 된 위젯은 다음 프레임에야
 *   Slate에 배치되므로, 포커스가 실제로 잡힐 때까지 몇 프레임 다시 시도한다.
 * - 포커스를 가진 위젯을 강조한다. RenderScale(그리기 변환)만 바꿔 레이아웃 폭·줄바꿈은 흔들지 않는다(UI-LAYOUT-01 보존).
 *
 * 방향 이동(D-Pad·왼쪽 스틱)과 확인(A)은 UE Slate 기본 동작을 그대로 쓴다. 뒤로(B)는 각 위젯의 NativeOnPreviewKeyDown이 처리한다.
 * 위젯 하나에 이 구조체를 멤버로 두고, NativeTick에서 Tick()을 부른다.
 */
struct DRONE_API FDroneGamepadFocus
{
	/** 강조 배율. 각 위젯의 UPROPERTY 값을 Tick 전에 넣는다. */
	float FocusScale = 1.06f;
	/** 강조 중인 버튼의 내용(글자) 색 배율. */
	FLinearColor FocusTint = FLinearColor(1.0f, 0.86f, 0.42f, 1.0f);

	/** 다음 Tick부터 Candidates 순서대로 첫 조작 가능 위젯에 포커스를 준다. 앞선 요청은 덮어쓴다. */
	void RequestFocus(const TArray<UWidget*>& Candidates);
	void CancelPendingFocus();
	bool HasPendingFocus() const { return !PendingCandidates.IsEmpty(); }

	/** 대기 중인 포커스를 처리하고 강조를 갱신한다. Highlightable은 이 화면에서 강조할 수 있는 위젯 전부. */
	void Tick(APlayerController* Player, const TArray<UWidget*>& Highlightable);

	/** 강조를 모두 해제한다(위젯 파괴·화면 전환 전). */
	void ClearHighlight();

	/** 위젯과 모든 조상이 보이고 활성 상태인지. 다른 UserWidget 안에 들어 있는 경우도 따라 올라간다. */
	static bool IsNavigable(const UWidget* Widget);
	static bool HasFocus(const UWidget* Widget, APlayerController* Player);
	/** Candidates 중 포커스를 가진 위젯. 없으면 nullptr. */
	static UWidget* FindFocused(APlayerController* Player, const TArray<UWidget*>& Candidates);

	UWidget* GetHighlighted() const { return Highlighted.Get(); }

private:
	void ApplyHighlight(UWidget* Widget, bool bHighlighted) const;

	TArray<TWeakObjectPtr<UWidget>> PendingCandidates;
	int32 PendingAttempts = 0;
	TWeakObjectPtr<UWidget> Highlighted;
};
