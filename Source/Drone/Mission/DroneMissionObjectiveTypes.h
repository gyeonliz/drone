#pragma once

#include "CoreMinimal.h"
#include "DroneMissionObjectiveTypes.generated.h"

/** 목표를 진행시키는 실제 사건 종류다. Manual은 기존 Training Greybox 호출을 유지한다. */
UENUM(BlueprintType)
enum class EDroneMissionObjectiveEvent : uint8
{
	Manual UMETA(DisplayName="Manual / Blueprint"),
	TrainingLap UMETA(DisplayName="Training Lap"),
	ReconScan UMETA(DisplayName="Recon Scan"),
	PayloadDelivered UMETA(DisplayName="Payload Delivered"),
	TargetDestroyed UMETA(DisplayName="Target Destroyed"),
	JammingExited UMETA(DisplayName="Jamming Zone Exited"),
	JammerDisabled UMETA(DisplayName="Jammer Disabled"),
	ReturnToBase UMETA(DisplayName="Return To Base"),
	/** 기존 저장 Asset의 열거형 숫자를 유지하기 위해 새 Tutorial Event는 항상 뒤에 추가한다. */
	HoverMaintained UMETA(DisplayName="Hover Maintained"),
	HeadingAligned UMETA(DisplayName="Heading Aligned")
};

/** 이전 Mission이 남긴 Story Fact에 따라 같은 Definition 안의 목표를 선택적으로 활성화한다. */
/** 실패(기체 파괴·제한 시간 초과·실패 Trigger) 때의 처리(MISSION-CHECKPOINT-01). */
UENUM(BlueprintType)
enum class EDroneMissionFailureResponse : uint8
{
	/** 기존 동작: 결과 화면을 띄운다. [다시 하기]는 맵을 다시 불러온다. */
	ShowResult UMETA(DisplayName="결과 화면"),
	/** 맵·완료 목표·부서진 표적은 그대로 두고, 마지막 체크포인트(없으면 출격 지점)에서 같은 기체로 다시 출격한다. 현재 목표 제한 시간은 다시 센다. */
	RestartFromCheckpoint UMETA(DisplayName="체크포인트에서 재출격")
};

UENUM(BlueprintType)
enum class EDroneMissionStoryFactCondition : uint8
{
	Always UMETA(DisplayName="Always"),
	FactPresent UMETA(DisplayName="Story Fact Present"),
	FactAbsent UMETA(DisplayName="Story Fact Absent")
};

/** Data Asset의 한 목표. TargetId가 있으면 사건 Actor의 Actor Tags에 같은 이름을 넣는다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneMissionObjectiveRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Objective")
	FName ObjectiveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Objective", meta=(MultiLine="true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Objective")
	EDroneMissionObjectiveEvent Event = EDroneMissionObjectiveEvent::Manual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Objective", meta=(ClampMin="1"))
	int32 RequiredProgress = 1;

	/** 0이면 시간 제한이 없다. 목표가 활성화된 순간부터 세고 만료 시 Mission 실패다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Objective", meta=(ClampMin="0.0", ForceUnits="s"))
	float TimeLimitSeconds = 0.0f;

	/** 비어 있으면 종류가 맞는 모든 대상 Actor를 허용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Objective")
	FName TargetId = NAME_None;

	/** 미끼/실제 탑승처럼 이전 Mission 결과에 따라 이 목표 자체를 포함하거나 제외한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Objective|Story Branch")
	EDroneMissionStoryFactCondition StoryFactCondition = EDroneMissionStoryFactCondition::Always;

	/** 조건이 Always가 아닐 때 Game Flow에 존재/부재해야 하는 안정적인 Fact ID다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Objective|Story Branch")
	FName StoryFactId = NAME_None;
};
