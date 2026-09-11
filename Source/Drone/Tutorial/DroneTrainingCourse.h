#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneTrainingCourse.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UChildActorComponent;
class UDroneTrainingGateSequenceComponent;
class UDroneTrainingLapRecorderComponent;
class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class ADroneTrainingGate;

/**
 * Tutorial 비행 경로 표시와 명시적 Gate 구성 묶음을 담당하는 Course Actor.
 *
 * TUT-01의 편집 가능한 Spline과 안내선을 유지한다. TUT-02의 CourseId/명시적 Gate
 * 배열과 순서 판정을 보관하고, TUT-03의 Lap Recorder를 소유한다. Gate Trigger는
 * 별도 Actor이며 순서 판정과 기록도 각각 비-Primitive Component로 분리한다.
 *
 * 안내선은 Drone이 실제로 통과하는 공간에 놓이므로 Actor, Spline,
 * 생성되는 모든 SplineMesh의 Collision·Overlap·Physics·Navigation 영향을
 * 명시적으로 끈다. 외형 Mesh와 색은 Blueprint 자식에서 바꿀 수 있지만
 * 비간섭 설정은 C++이 매번 다시 적용한다.
 */
UCLASS(Blueprintable)
class ADroneTrainingCourse : public AActor
{
	GENERATED_BODY()

public:
	ADroneTrainingCourse();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** Level 또는 BP Viewport에서 점을 움직여 초기 Greybox 경로를 편집한다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course")
	USplineComponent* GetCourseSpline() const { return CourseSpline; }

	/** 현재 Spline 점 사이에 생성된 표시 Segment 수다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course")
	int32 GetCourseLineSegmentCount() const;

	/** 현재 Spline 길이와 표시 Segment 목표 길이로 계산한 실제 생성 예정 수다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course")
	int32 GetExpectedCourseLineSegmentCount() const;

	/** 곡선을 짧게 나눠 표시할 때 사용하는 Segment 목표 길이다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course")
	float GetCourseLineSegmentLengthCentimeters() const { return CourseLineSegmentLengthCentimeters; }

	/** 현재 표시 Segment에 적용할 Material. 기본값은 프로젝트 전용 Unlit 발광 재질이다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course")
	UMaterialInterface* GetCourseLineMaterial() const { return CourseLineMaterial; }

	UFUNCTION(BlueprintPure, Category="Tutorial|Course|Gates")
	FName GetCourseId() const { return CourseId; }

	/** 자동 모드에서 Spline을 따라 실제 생성된 Ring Gate 수다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course|Automatic Gates")
	int32 GetGeneratedAutomaticGateCount() const;

	/** 자동 Gate가 사용할 Spline 거리. Blueprint 미리보기와 Editor 검증에서 같은 계산을 쓴다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course|Automatic Gates")
	float GetAutomaticGateDistanceAlongSpline(int32 GateIndex) const;

	UFUNCTION(BlueprintPure, Category="Tutorial|Course|Automatic Gates")
	bool IsUsingAutomaticSplineGates() const { return bUseAutomaticSplineGates; }

	/** 켜져 있으면 CourseSpline의 제어점 하나가 Ring 하나의 직접 편집 위치가 된다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course|Automatic Gates")
	bool IsUsingSplinePointsAsAutomaticGatePositions() const
	{
		return bUseSplinePointsAsAutomaticGatePositions;
	}

	/** 현재 배치 방식에서 실제로 사용할 Ring 수. Spline Point 모드에서는 Point 수와 같다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course|Automatic Gates")
	int32 GetResolvedAutomaticGateCount() const;

	UFUNCTION(BlueprintPure, Category="Tutorial|Course|Gates")
	UDroneTrainingGateSequenceComponent* GetGateSequenceComponent() const { return GateSequenceComponent; }

	/** 정상 Gate Event로 시간·실제 이동 거리·평균 속도를 계산하는 TUT-03 기록기다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Course|Recording")
	UDroneTrainingLapRecorderComponent* GetLapRecorderComponent() const { return LapRecorderComponent; }

	/** 현재 Sequence가 실제 사용하는 Gate 배열. 자동 모드면 생성 Gate, 수동 모드면 저장된 OrderedGates다. */
	const TArray<TObjectPtr<ADroneTrainingGate>>& GetOrderedGates() const
	{
		return bUseAutomaticSplineGates ? ActiveOrderedGates : OrderedGates;
	}

	/** 자동화나 후속 Editor 도구가 명시적 배열을 설정한 뒤 같은 검증 경로를 사용한다. */
	void ConfigureOrderedGates(const TArray<ADroneTrainingGate*>& InOrderedGates);

	/** OrderedGates 배열 위치를 CourseId와 GateIndex의 단일 기준으로 다시 적용한다. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Tutorial|Course|Gates")
	void SynchronizeGateDefinitions();

	/** Details 값과 현재 Spline으로 자동 Gate를 즉시 다시 만든다. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Tutorial|Course|Automatic Gates")
	void RebuildAutomaticGates();

	/** 런타임/자동화에서도 한 번에 자동 배치 규칙을 설정할 수 있는 Blueprint 경계다. */
	UFUNCTION(BlueprintCallable, Category="Tutorial|Course|Automatic Gates")
	void ConfigureAutomaticGateLayout(
		bool bEnabled,
		int32 GateCount,
		bool bEvenlyDistribute,
		float StartDistanceCentimeters,
		float SpacingCentimeters,
		float EndPaddingCentimeters);

	/** Ring별 Spline 절대 거리와 로컬 위치 보정을 한 번에 적용한다. 빈 배열 항목은 자동 배치값을 유지한다. */
	UFUNCTION(BlueprintCallable, Category="Tutorial|Course|Automatic Gates")
	void ConfigureAutomaticGateOverrides(
		const TArray<float>& SplineDistancesCentimeters,
		const TArray<FVector>& LocalOffsets);

	/** 켜면 Spline Point를 이동·추가·삭제하는 작업이 Ring 위치·개수에 직접 반영된다. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Tutorial|Course|Automatic Gates")
	void ConfigureAutomaticGateSplinePointPlacement(bool bEnabled);

	/** 자동화와 후속 Gate 배치가 같은 이름 계약을 사용할 수 있게 공개한다. */
	static FName GetGeneratedSegmentTag();

	/** 자동 Gate ChildActorComponent를 식별하는 안정적인 Tag다. */
	static FName GetGeneratedGateComponentTag();

protected:
	/** Course Actor의 고정 기준점. 경로 자체는 이동하지 않으므로 Tick이 없다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tutorial|Course", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> CourseRoot;

	/** Designer가 점과 Tangent를 편집하는 경로 데이터. 이 Component 자체도 충돌하지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tutorial|Course", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USplineComponent> CourseSpline;

	/** Gate 진행 상태는 Primitive가 아닌 이 Component 한 곳에서만 바뀐다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Gates", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDroneTrainingGateSequenceComponent> GateSequenceComponent;

	/** Gate 판정과 분리된 원본 Segment/Lap 기록 Component. Tick과 별도 Timer를 만들지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Recording", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDroneTrainingLapRecorderComponent> LapRecorderComponent;

	/** Gate와 Course가 같은 구성에 속하는지 확인하는 명시적 식별자다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Tutorial|Course|Gates", meta=(AllowPrivateAccess="true"))
	FName CourseId = TEXT("DroneTrainingCourse");

	/** 배열 위치가 통과 순서의 단일 기준이며 각 GateIndex는 같은 위치를 미러링해야 한다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Tutorial|Course|Gates", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<ADroneTrainingGate>> OrderedGates;

	/** 켜면 수동 OrderedGates 대신 아래 수치로 Ring을 생성하고 Spline에 부착한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates", meta=(AllowPrivateAccess="true"))
	bool bUseAutomaticSplineGates = false;

	/**
	 * 켜면 Spline Point 1개를 Ring 1개로 사용한다. BP/Level Viewport에서 Point를 직접
	 * 이동·추가·삭제하면 Ring 위치와 개수가 그대로 따라가며 아래 개수/간격/절대거리 값은 무시한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates", DisplayName="Spline Point 1개당 Ring 1개", AllowPrivateAccess="true"))
	bool bUseSplinePointsAsAutomaticGatePositions = false;

	/** 생성할 Ring 수. Tutorial Lap은 시작 Gate와 종료 Gate가 필요하므로 최소 2개다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates && !bUseSplinePointsAsAutomaticGatePositions", ClampMin="2", ClampMax="64", UIMin="2", UIMax="32", AllowPrivateAccess="true"))
	int32 AutomaticGateCount = 4;

	/** 기본 Native Gate 또는 외형을 교체한 BP_DroneTrainingGate 자식을 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates", AllowPrivateAccess="true"))
	TSubclassOf<ADroneTrainingGate> AutomaticGateClass;

	/** 켜면 시작 거리부터 끝 여백 전까지 GateCount만큼 균등 배치한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates && !bUseSplinePointsAsAutomaticGatePositions", AllowPrivateAccess="true"))
	bool bEvenlyDistributeAutomaticGates = true;

	/** 첫 Gate의 기본 Spline 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates && !bUseSplinePointsAsAutomaticGatePositions", ClampMin="0.0", UIMin="0.0", Units="cm", AllowPrivateAccess="true"))
	float AutomaticGateStartDistanceCentimeters = 200.0f;

	/** 균등 분배를 끈 경우 Gate 사이 고정 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates && !bUseSplinePointsAsAutomaticGatePositions && !bEvenlyDistributeAutomaticGates", ClampMin="1.0", UIMin="1.0", Units="cm", AllowPrivateAccess="true"))
	float AutomaticGateSpacingCentimeters = 1200.0f;

	/** 균등 분배 시 마지막 Gate와 Spline 끝 사이 여백이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates && !bUseSplinePointsAsAutomaticGatePositions && bEvenlyDistributeAutomaticGates", ClampMin="0.0", UIMin="0.0", Units="cm", AllowPrivateAccess="true"))
	float AutomaticGateEndPaddingCentimeters = 200.0f;

	/** 모든 Gate를 Spline 앞/뒤로 함께 미는 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates", Units="cm", AllowPrivateAccess="true"))
	float AutomaticGateDistanceOffsetCentimeters = 0.0f;

	/** 배열 Index별 추가 이동 거리. 항목이 없는 Gate는 0을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates", AllowPrivateAccess="true"))
	TArray<float> AutomaticGateDistanceOffsetsCentimeters;

	/**
	 * 배열 Index별 Spline 시작점 기준 절대 거리(cm). 0 이상인 항목은 균등/고정 간격의 기본 거리를 대체한다.
	 * 항목이 없거나 음수면 해당 Ring은 기존 자동 배치값을 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates && !bUseSplinePointsAsAutomaticGatePositions", AllowPrivateAccess="true"))
	TArray<float> AutomaticGateSplineDistancesCentimeters;

	/** Spline 위치에서 Gate 로컬 축 기준으로 더하는 위치 보정이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates", Units="cm", AllowPrivateAccess="true"))
	FVector AutomaticGateLocalOffset = FVector::ZeroVector;

	/** 배열 Index별 Gate 로컬 위치 보정(cm). 공통 LocalOffset에 더하며 항목이 없는 Gate는 0을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates", AllowPrivateAccess="true"))
	TArray<FVector> AutomaticGateLocalOffsets;

	/** Spline 접선 회전에 더하는 Gate 회전 보정이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates", AllowPrivateAccess="true"))
	FRotator AutomaticGateRotationOffset = FRotator::ZeroRotator;

	/** 생성 Ring 전체 크기. Gate 자체 Radius/Trigger 값은 Gate Blueprint에서 별도로 조정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Automatic Gates",
		meta=(EditCondition="bUseAutomaticSplineGates", AllowPrivateAccess="true"))
	FVector AutomaticGateScale = FVector::OneVector;

	/**
	 * SplineMesh에 사용할 임시 Greybox Mesh.
	 * 현재 기본값은 Engine Cube이며 구매 에셋이나 최종 코스 외형으로 확정된 것이 아니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMesh> CourseLineMesh;

	/** 기본값은 프로젝트의 발광 Material이며 BP에서 다른 표시 Material로 교체할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMaterialInterface> CourseLineMaterial;

	/** 안내선 폭. Drone Collision보다 얇게 보여 주기 위한 초기 Greybox 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Visual",
		meta=(ClampMin="1.0", UIMin="1.0", Units="cm", AllowPrivateAccess="true"))
	float CourseLineWidthCentimeters = 32.0f;

	/** 안내선 두께. 지면이나 환경 Mesh와 Z-fighting이 생기지 않도록 별도로 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Visual",
		meta=(ClampMin="1.0", UIMin="1.0", Units="cm", AllowPrivateAccess="true"))
	float CourseLineThicknessCentimeters = 10.0f;

	/** Spline 점보다 안내선을 위로 띄우는 로컬 Z Offset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Visual",
		meta=(Units="cm", AllowPrivateAccess="true"))
	float CourseLineVerticalOffsetCentimeters = 0.0f;

	/**
	 * 발광 안내 Mesh 한 조각의 목표 길이. 제어점 사이를 그대로 늘이지 않고 이 길이마다
	 * Spline을 다시 표본화해 급커브에서도 육안으로 매끄럽게 보이게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Visual",
		meta=(ClampMin="25.0", UIMin="25.0", UIMax="1000.0", Units="cm", AllowPrivateAccess="true"))
	float CourseLineSegmentLengthCentimeters = 200.0f;

	/** 기본 발광 Material의 Color Parameter에 적용되는 초기 안내색이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Course|Visual", meta=(AllowPrivateAccess="true"))
	FLinearColor CourseLineColor = FLinearColor(0.02f, 0.70f, 1.0f, 1.0f);

private:
	/** BP/Level에서 값을 잘못 바꿔도 Construction과 BeginPlay에서 비간섭 계약을 복원한다. */
	void ApplyNonInterferenceRules();

	/** 구성 데이터와 비-Primitive Sequence 상태를 연결하고 Gate 외형을 초기화한다. */
	void ConfigureGateSequence();

	/** 자동 모드의 Child Actor Gate를 현재 Spline에서 재생성한다. */
	void RebuildAutomaticGateComponents();

	/** 이전 Construction에서 만든 Gate Component를 Tag 기준으로 제거한다. */
	void DestroyGeneratedAutomaticGateComponents();

	/** 자동/수동 모드 중 하나를 ActiveOrderedGates 단일 실행 배열로 선택한다. */
	void RefreshActiveOrderedGates();

	/** OnConstruction/BeginPlay에서 같은 안전 규칙으로 Segment를 재생성한다. */
	void RebuildCourseLineSegments();

	/** 표시 Material을 한 번 만들고 모든 Segment가 공유하게 한다. */
	UMaterialInterface* CreateCourseLineMaterial();

	/** Construction 재실행 때 이전 임시 MID가 남지 않도록 GC 추적만 유지한다. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicCourseLineMaterial;

	/** 생성 Component 수명은 Actor가 소유하며 배열은 현재 Construction 결과를 빠르게 찾는 Cache다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UChildActorComponent>> GeneratedAutomaticGateComponents;

	/** Gate Sequence와 Recorder가 실제 사용하는 자동/수동 통합 배열이다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADroneTrainingGate>> ActiveOrderedGates;
};
