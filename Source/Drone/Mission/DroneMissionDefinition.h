#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Mission/DroneMissionObjectiveTypes.h"
#include "UObject/SoftObjectPath.h"
#include "DroneMissionDefinition.generated.h"

class UTexture2D;
class UWorld;
class USoundBase;

/** MISSION-BRIEFING-02: 브리핑 화면에서 차례로 보여 주는 대사 한 줄(Figma: 오퍼레이터 허브 대사 → HUD 목표 요약 순). */
USTRUCT(BlueprintType)
struct DRONE_API FDroneMissionBriefingLine
{
	GENERATED_BODY()

	/** 말하는 사람. 예: 허브 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Briefing")
	FText Speaker;

	/** 자막 원문. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Briefing", meta=(MultiLine="true"))
	FText Text;

	/** 음성(TTS·녹음). 비우면 자막만 보여 준다. 음원이 지급되면 여기에 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Briefing")
	TSoftObjectPtr<USoundBase> Voice;

	/** 이 줄을 보여 줄 시간(초). 0이면 음성 길이, 음성이 없으면 글자 수로 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Briefing", meta=(ClampMin="0"))
	float DurationSeconds = 0.0f;

	/** 스토리 분기에 따라 이 줄을 넣거나 뺀다(목표 Rule과 같은 규칙). 예: M3 "오마르는 처리됐다"는 Story.TargetEliminated가 있을 때만. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Briefing|Story Branch")
	EDroneMissionStoryFactCondition StoryFactCondition = EDroneMissionStoryFactCondition::Always;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission Briefing|Story Branch")
	FName StoryFactId = NAME_None;
};

/** Auto는 기존 DA 호환용이다. 새 DA는 로비에서 표시할 탭을 직접 선택한다. */
UENUM(BlueprintType)
enum class EDroneMissionCategory : uint8
{
	Auto,
	Tutorial UMETA(DisplayName="튜토리얼"),
	Racing UMETA(DisplayName="레이싱"),
	Mission UMETA(DisplayName="미션")
};

/** 로비 설명, 브리핑, Map, 허용 Drone과 시작 목표의 단일 Mission 데이터다. */
UCLASS(BlueprintType)
class DRONE_API UDroneMissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Identity")
	FName MissionId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	EDroneMissionCategory LobbyCategory = EDroneMissionCategory::Auto;

	/** 분류는 맵과 별개다. 같은 맵을 쓰는 수업도 다른 탭에 배치할 수 있다. */
	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	EDroneMissionCategory GetLobbyCategory() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display", meta=(MultiLine="true"))
	FText LobbyDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	FText RegionText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	FText DifficultyText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Display")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/** 선택적 영상/Sequence 참조다. 비어 있으면 FLOW-04 정적 Briefing을 사용한다. 최종 Media 형식은 현재 미정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Briefing")
	FSoftObjectPath BriefingAsset;

	/** 브리핑 화면 자막·음성 대사. 비어 있으면 자막 영역을 숨기고 기존 정적 브리핑만 보여 준다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Briefing")
	TArray<FDroneMissionBriefingLine> BriefingLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Runtime")
	TSoftObjectPtr<UWorld> MissionMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Runtime")
	TArray<FName> AllowedDroneIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Runtime")
	FName DefaultDroneId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Runtime", meta=(MultiLine="true"))
	TArray<FText> InitialObjectives;

	/** 새 미션은 이 Rule 배열을 사용한다. 비어 있으면 기존 InitialObjectives를 1회 수동 목표로 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	TArray<FDroneMissionObjectiveRule> ObjectiveRules;

	/** Mission 성공 시 GameInstance 수명 Story Fact에 추가한다. 실패/재도전에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Story Branch")
	TArray<FName> StoryFactsGrantedOnSuccess;

	/** 서로 배타적인 분기를 바꿀 때 성공 시 제거할 기존 Story Fact다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Story Branch")
	TArray<FName> StoryFactsRemovedOnSuccess;

	/** 실제 Rule Object 형식은 Mission Director 카드에서 확정하고 지금은 안정적인 ID만 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	FName SuccessRuleId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	FName FailureRuleId = NAME_None;

	/**
	 * 실패 때 결과 화면으로 갈지, 체크포인트에서 다시 출격할지(Figma: M1·튜토리얼은 시작/픽업 지점 재시작, M3 UGV 재출격, M4 클리어까지 도전).
	 * 체크포인트는 맵에 놓은 DroneMissionCheckpoint를 기체가 지나가면 갱신된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules")
	EDroneMissionFailureResponse FailureResponse = EDroneMissionFailureResponse::ShowResult;

	/** 재출격 허용 횟수. 0이면 제한 없음. 다 쓰면 다음 실패는 결과 화면으로 간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Rules", meta=(ClampMin="0", EditCondition="FailureResponse==EDroneMissionFailureResponse::RestartFromCheckpoint"))
	int32 MaxCheckpointRestarts = 0;

	/**
	 * 성공 뒤 결과 화면 [다음 수업]으로 이어질 미션 ID(TUT-PROGRESS-01). 비우면 마지막(또는 연결 없음)이다.
	 * 이렇게 이어진 미션들이 한 과정(예: 튜토리얼 8개)이 되고, 로비 순서와 "수업 n/8" 표시도 이 연결을 따른다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mission Definition|Progression")
	FName NextMissionId = NAME_None;

	bool ValidateDefinition(FString& OutError) const;
	bool HasUsableObjectives() const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	bool IsDefinitionValid() const;
};
