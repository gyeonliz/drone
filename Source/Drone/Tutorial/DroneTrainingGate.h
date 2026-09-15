#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tutorial/DroneTrainingGateTypes.h"
#include "DroneTrainingGate.generated.h"

class UBoxComponent;
class UDroneTrainingGateSequenceComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Tutorial의 사각 Greybox Frame과 판정용 Box Trigger를 분리한 Gate Actor.
 *
 * 이 Actor는 Overlap의 진입/이탈 위치를 Sequence Component에 전달할 뿐,
 * 현재 Gate가 무엇인지 직접 결정하지 않는다. Visual은 항상 비충돌이고
 * Trigger만 Pawn 채널 Query Overlap을 사용한다.
 */
UCLASS(Blueprintable)
class ADroneTrainingGate : public AActor
{
	GENERATED_BODY()

public:
	ADroneTrainingGate();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	FName GetCourseId() const { return CourseId; }

	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	int32 GetGateIndex() const { return GateIndex; }

	/** TUT-02에서는 저장·표시만 하며 Timing이나 자동 정렬 계산에는 사용하지 않는다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	float GetSegmentDistance() const { return SegmentDistance; }

	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	FVector GetForwardDirectionWorld() const;

	/** 화면 Frame과 Box Trigger가 공유하는 정사각형 통과 영역의 반쪽 크기다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	float GetTriggerApertureHalfSizeCentimeters() const { return FMath::Max(TriggerHalfSizeCentimeters, 1.0f); }

	/** 기존 Blueprint 호출 호환용 별칭. 신규 로직은 HalfSize 명칭을 사용한다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	float GetTriggerApertureRadiusCentimeters() const { return FMath::Max(TriggerHalfSizeCentimeters, 1.0f); }

	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	EDroneTrainingGateVisualState GetGateVisualState() const { return GateVisualState; }

	/** 통과 전/현재 목표/통과 후 상태에 실제 사용할 BP 설정 색을 반환한다. */
	UFUNCTION(BlueprintPure, Category="Tutorial|Gate|Visual")
	FLinearColor GetColorForVisualState(EDroneTrainingGateVisualState State) const;

	/** Blueprint에서 세 상태 색을 한 번에 바꾸고 현재 표시 Material에 즉시 반영한다. */
	UFUNCTION(BlueprintCallable, Category="Tutorial|Gate|Visual")
	void SetGateStateColors(
		FLinearColor BeforePassColor,
		FLinearColor CurrentTargetColor,
		FLinearColor AfterPassColor);

	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	UBoxComponent* GetGateTrigger() const { return GateTrigger; }

	UFUNCTION(BlueprintPure, Category="Tutorial|Gate")
	int32 GetRingVisualSegmentCount() const { return RingVisualSegments.Num(); }

	/** 자동화와 디버그에서 아직 EndOverlap되지 않은 진입 기록을 확인한다. */
	int32 GetPendingTraversalCount() const { return PendingEntryLocations.Num(); }

	UDroneTrainingGateSequenceComponent* GetAssignedGateSequence() const { return AssignedGateSequence.Get(); }

	/** 자동화와 Editor 생성 도구가 같은 메타데이터 계약을 사용하게 한다. */
	void ConfigureGateDefinition(
		FName InCourseId,
		int32 InGateIndex,
		float InSegmentDistance);

	/** Sequence만 호출해 Current/Completed/Inactive 표시 상태를 바꾼다. */
	void SetGateVisualState(EDroneTrainingGateVisualState NewState);

private:
	friend class UDroneTrainingGateSequenceComponent;

	/** Sequence 연결은 Course 구성 검증을 통과한 Gate에만 설정한다. */
	void AssignGateSequence(UDroneTrainingGateSequenceComponent* InSequence);

	/** Reset이나 Sequence 종료 전에 진행 중인 BeginOverlap 기록을 폐기한다. */
	void CancelPendingTraversals();

	/** BP 직렬화 뒤에도 Visual과 Trigger의 서로 다른 Collision 계약을 복원한다. */
	void ApplyComponentRules();

	/** Engine Cube 네 변을 Box Trigger 안쪽 경계에 맞춰 구매 에셋 없는 Greybox Frame을 만든다. */
	void RefreshRingVisual();

	/** 상태별 색을 프로젝트 Material의 Color Parameter에 적용한다. */
	void RefreshStateMaterial();

	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> GateRoot;

	/** 실제 판정 전용. 화면 Frame과 별개이며 Pawn만 Overlap한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> GateTrigger;

	/** 기존 Blueprint 직렬화 호환을 위해 16개를 보존하며 앞의 4개만 Frame으로 표시한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Components", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<UStaticMeshComponent>> RingVisualSegments;

	/** Course의 OrderedGates 배열에서 자동으로 동기화되며 직접 편집하지 않는다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Tutorial|Gate|Definition", meta=(AllowPrivateAccess="true"))
	FName CourseId = TEXT("DroneTrainingCourse");

	/** Course의 OrderedGates 배열 위치에서 자동으로 정해진다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Tutorial|Gate|Definition", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 GateIndex = 0;

	/** 후속 Segment 기록용 메타데이터. TUT-02 판정에는 사용하지 않는다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Tutorial|Gate|Definition", meta=(ClampMin="0.0", Units="cm", AllowPrivateAccess="true"))
	float SegmentDistance = 0.0f;

	/** 이전 16각형 Ring 설정 호환용이며 현재 사각 Frame 크기에는 사용하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Legacy", meta=(ClampMin="50.0", Units="cm", DisplayName="Legacy 원형 반지름 (미사용)", AllowPrivateAccess="true"))
	float GateRadiusCentimeters = 220.0f;

	/** Frame 네 변의 굵기와 앞뒤 깊이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Visual", meta=(ClampMin="2.0", Units="cm", DisplayName="프레임 굵기", AllowPrivateAccess="true"))
	float RingThicknessCentimeters = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Trigger", meta=(ClampMin="1.0", Units="cm", AllowPrivateAccess="true"))
	float TriggerHalfDepthCentimeters = 60.0f;

	/** Frame 안쪽과 실제 통과 판정이 공유하는 정사각형 반쪽 크기. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Trigger", meta=(ClampMin="1.0", Units="cm", DisplayName="통과 영역 반쪽 크기", AllowPrivateAccess="true"))
	float TriggerHalfSizeCentimeters = 175.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMesh> RingSegmentMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tutorial|Gate|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMaterialInterface> RingMaterial;

	/** 아직 순서가 오지 않은 Gate의 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tutorial|Gate|Visual",
		meta=(DisplayName="통과 전 색상", AllowPrivateAccess="true"))
	FLinearColor InactiveColor = FLinearColor(0.02f, 0.08f, 0.18f, 1.0f);

	/** 지금 통과해야 하는 Gate의 강조 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tutorial|Gate|Visual",
		meta=(DisplayName="현재 목표 색상", AllowPrivateAccess="true"))
	FLinearColor CurrentColor = FLinearColor(0.10f, 1.0f, 0.18f, 1.0f);

	/** 정상 통과가 끝난 Gate의 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tutorial|Gate|Visual",
		meta=(DisplayName="통과 후 색상", AllowPrivateAccess="true"))
	FLinearColor CompletedColor = FLinearColor(0.02f, 0.70f, 1.0f, 1.0f);

	UPROPERTY(Transient)
	EDroneTrainingGateVisualState GateVisualState = EDroneTrainingGateVisualState::Inactive;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicRingMaterial;

	UPROPERTY(Transient)
	TWeakObjectPtr<UDroneTrainingGateSequenceComponent> AssignedGateSequence;

	/** 한 Actor의 Begin 위치를 최초 한 번만 보존해 중복 Begin Event가 방향을 바꾸지 못하게 한다. */
	TMap<TWeakObjectPtr<AActor>, FVector> PendingEntryLocations;
};
