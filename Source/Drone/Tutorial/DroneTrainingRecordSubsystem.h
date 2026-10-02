#pragma once

#include "CoreMinimal.h"
#include "Prototype/DroneFlightControlTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DroneTrainingRecordSubsystem.generated.h"

/** 저장 파일을 읽은 결과. 디버그·테스트용. */
UENUM(BlueprintType)
enum class EDroneTrainingRecordLoadState : uint8
{
	NotLoaded,
	/** 저장 파일이 아직 없다(첫 실행). */
	NoSave,
	Loaded,
	/** 다른 형식 버전이라 백업하고 새로 시작했다. */
	OutdatedReset,
	/** 읽을 수 없는 파일이라 백업하고 새로 시작했다. */
	CorruptReset
};

/**
 * TUT-BEST-01: 유효한 완주만 "코스 + 기체 + 조작 방식" 조합별 최고 기록으로 저장·복원한다.
 * 조종 감도 프리셋(HandlingPreset)은 키에 넣지 않는다. 평균은 저장하지 않고 실행 중 History로만 쓴다.
 *
 * 저장 형식은 Saved/SaveGames/<슬롯>.json 텍스트다. USaveGame 바이너리는 손상된 파일을 읽을 때
 * 실패를 돌려주지 않고 엔진이 멈춰서(2026-10-02 테스트에서 FName 길이 Assert로 확인) 쓰지 않는다.
 * 음량 설정 SaveGame과는 다른 파일이다.
 */
UCLASS()
class DRONE_API UDroneTrainingRecordSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 1;
	static const TCHAR* DefaultSlotName;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 이 슬롯에서 다시 읽는다. 테스트는 다른 슬롯을 써서 실제 기록을 건드리지 않는다. */
	void UseSlot(const FString& InSlotName);

	static FString MakeRecordKey(FName CourseId, FName DroneId, EDroneControlMode ControlMode);
	/** 슬롯의 실제 파일 경로. */
	static FString GetSlotFilePath(const FString& InSlotName);

	/** 저장된 최고 기록. 없으면 false. */
	bool GetBestLap(FName CourseId, FName DroneId, EDroneControlMode ControlMode, double& OutSeconds) const;

	/**
	 * 완주 기록을 낸다. 유한한 양수만 받는다. 기존보다 빠르면(또는 처음이면) 저장하고 true.
	 * OutPreviousBest는 이번 기록을 반영하기 전 값(없으면 bOutHadPrevious=false).
	 */
	bool SubmitLap(FName CourseId, FName DroneId, EDroneControlMode ControlMode, double Seconds,
		bool& bOutHadPrevious, double& OutPreviousBest);

	EDroneTrainingRecordLoadState GetLoadState() const { return LoadState; }
	const FString& GetSlotName() const { return SlotName; }
	int32 GetRecordCount() const { return BestLapSeconds.Num(); }

	/** 지금 기록과 파일을 지운다(테스트용). */
	void ClearAllRecordsForTesting();

private:
	void LoadRecords();
	bool SaveRecords() const;
	void BackupUnreadableFile(const TCHAR* Reason) const;

	FString SlotName;
	TMap<FString, double> BestLapSeconds;
	EDroneTrainingRecordLoadState LoadState = EDroneTrainingRecordLoadState::NotLoaded;
};
