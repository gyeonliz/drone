#include "Tutorial/DroneTrainingRecordSubsystem.h"

#include "Dom/JsonObject.h"
#include "Drone.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

const TCHAR* UDroneTrainingRecordSubsystem::DefaultSlotName = TEXT("DroneTrainingBestLaps");

namespace DroneTrainingRecord
{
const TCHAR* VersionField = TEXT("schemaVersion");
const TCHAR* LapsField = TEXT("bestLapSeconds");
}

void UDroneTrainingRecordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 자동화 테스트가 도는 랩이 사용자의 실제 최고 기록을 덮지 않게 다른 파일을 쓴다.
	UseSlot(GIsAutomationTesting ? FString(DefaultSlotName) + TEXT("_Automation") : FString(DefaultSlotName));
}

void UDroneTrainingRecordSubsystem::UseSlot(const FString& InSlotName)
{
	SlotName = InSlotName;
	LoadRecords();
}

FString UDroneTrainingRecordSubsystem::GetSlotFilePath(const FString& InSlotName)
{
	return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (InSlotName + TEXT(".json"));
}

FString UDroneTrainingRecordSubsystem::MakeRecordKey(const FName CourseId, const FName DroneId, const EDroneControlMode ControlMode)
{
	// 조작 방식은 숫자로 넣는다. 표시 이름을 바꿔도 기존 기록 키가 바뀌지 않는다.
	return FString::Printf(TEXT("%s|%s|%d"), *CourseId.ToString(), *DroneId.ToString(), static_cast<int32>(ControlMode));
}

bool UDroneTrainingRecordSubsystem::GetBestLap(const FName CourseId, const FName DroneId, const EDroneControlMode ControlMode,
	double& OutSeconds) const
{
	if (const double* Found = BestLapSeconds.Find(MakeRecordKey(CourseId, DroneId, ControlMode)))
	{
		OutSeconds = *Found;
		return true;
	}
	return false;
}

bool UDroneTrainingRecordSubsystem::SubmitLap(const FName CourseId, const FName DroneId, const EDroneControlMode ControlMode,
	const double Seconds, bool& bOutHadPrevious, double& OutPreviousBest)
{
	bOutHadPrevious = GetBestLap(CourseId, DroneId, ControlMode, OutPreviousBest);
	// 코스·기체를 알 수 없거나 시간이 이상하면 기록하지 않는다(유효한 완주만).
	if (CourseId.IsNone() || DroneId.IsNone() || !FMath::IsFinite(Seconds) || Seconds <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}
	if (bOutHadPrevious && Seconds >= OutPreviousBest)
	{
		return false;
	}
	BestLapSeconds.Add(MakeRecordKey(CourseId, DroneId, ControlMode), Seconds);
	if (!SaveRecords())
	{
		UE_LOG(LogDrone, Warning, TEXT("[TUT-BEST] '%s' 저장 실패. 이번 실행 동안만 기록이 유지된다."), *GetSlotFilePath(SlotName));
	}
	return true;
}

void UDroneTrainingRecordSubsystem::LoadRecords()
{
	BestLapSeconds.Reset();
	const FString Path = GetSlotFilePath(SlotName);
	if (!IFileManager::Get().FileExists(*Path))
	{
		LoadState = EDroneTrainingRecordLoadState::NoSave;
		return;
	}
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		BackupUnreadableFile(TEXT("Corrupt"));
		LoadState = EDroneTrainingRecordLoadState::CorruptReset;
		return;
	}
	int32 Version = 0;
	if (!Root->TryGetNumberField(DroneTrainingRecord::VersionField, Version) || Version != CurrentSchemaVersion)
	{
		BackupUnreadableFile(*FString::Printf(TEXT("V%d"), Version));
		LoadState = EDroneTrainingRecordLoadState::OutdatedReset;
		return;
	}
	const TSharedPtr<FJsonObject>* Laps = nullptr;
	if (Root->TryGetObjectField(DroneTrainingRecord::LapsField, Laps) && Laps && Laps->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Laps)->Values)
		{
			double Seconds = 0.0;
			// 손으로 고친 파일 등 이상한 값은 버린다.
			if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Seconds) && FMath::IsFinite(Seconds) && Seconds > UE_DOUBLE_SMALL_NUMBER)
			{
				BestLapSeconds.Add(Pair.Key, Seconds);
			}
		}
	}
	LoadState = EDroneTrainingRecordLoadState::Loaded;
}

bool UDroneTrainingRecordSubsystem::SaveRecords() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(DroneTrainingRecord::VersionField, CurrentSchemaVersion);
	const TSharedRef<FJsonObject> Laps = MakeShared<FJsonObject>();
	for (const TPair<FString, double>& Pair : BestLapSeconds)
	{
		Laps->SetNumberField(Pair.Key, Pair.Value);
	}
	Root->SetObjectField(DroneTrainingRecord::LapsField, Laps);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	return FJsonSerializer::Serialize(Root, Writer) && FFileHelper::SaveStringToFile(Text, *GetSlotFilePath(SlotName),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UDroneTrainingRecordSubsystem::BackupUnreadableFile(const TCHAR* Reason) const
{
	// 읽을 수 없는 파일을 지우지 않고 옆으로 옮긴 뒤 새로 시작한다. 필요하면 사람이 되살릴 수 있다.
	const FString Path = GetSlotFilePath(SlotName);
	const FString BackupPath = GetSlotFilePath(FString::Printf(TEXT("%s_%s_%s"), *SlotName, Reason,
		*FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
	IFileManager::Get().Move(*BackupPath, *Path, /*bReplace*/ true);
	UE_LOG(LogDrone, Warning, TEXT("[TUT-BEST] '%s'(%s)를 읽을 수 없어 '%s'로 옮기고 새로 시작한다."), *Path, Reason, *BackupPath);
}

void UDroneTrainingRecordSubsystem::ClearAllRecordsForTesting()
{
	BestLapSeconds.Reset();
	IFileManager::Get().Delete(*GetSlotFilePath(SlotName));
	LoadState = EDroneTrainingRecordLoadState::NoSave;
}
