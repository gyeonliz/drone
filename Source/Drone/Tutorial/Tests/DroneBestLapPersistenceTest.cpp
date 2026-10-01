#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tutorial/DroneTrainingRecordSubsystem.h"

/**
 * TUT-BEST-01: 코스·기체·조작 방식별 최고 기록 저장·복원과 저장 없음/구버전/손상 처리.
 * 실제 사용자 기록과 섞이지 않게 테스트 전용 슬롯을 쓰고 끝나면 백업까지 지운다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBestLapPersistenceTest,
	"Drone.Tutorial.BestLapPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace DroneBestLapPersistence
{
const FString TestSlot(TEXT("DroneTrainingBestLaps_AutomationTest"));
const FName Course(TEXT("Course.Test.Circuit"));
const FName Scout(TEXT("Drone.Scout.Greybox"));

UDroneTrainingRecordSubsystem* MakeRecords()
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UDroneTrainingRecordSubsystem* Records = NewObject<UDroneTrainingRecordSubsystem>(GameInstance);
	Records->UseSlot(TestSlot);
	return Records;
}

void DeleteTestSlotsAndBackups()
{
	IFileManager::Get().Delete(*UDroneTrainingRecordSubsystem::GetSlotFilePath(TestSlot));
	TArray<FString> Files;
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
	IFileManager::Get().FindFiles(Files, *(Dir / (TestSlot + TEXT("_*.json"))), true, false);
	for (const FString& File : Files)
	{
		IFileManager::Get().Delete(*(Dir / File));
	}
}
}

bool FDroneBestLapPersistenceTest::RunTest(const FString& Parameters)
{
	using namespace DroneBestLapPersistence;
	DeleteTestSlotsAndBackups();

	UDroneTrainingRecordSubsystem* Records = MakeRecords();
	TestEqual(TEXT("First run has no save"), static_cast<int32>(Records->GetLoadState()), static_cast<int32>(EDroneTrainingRecordLoadState::NoSave));

	bool bHad = false;
	double Previous = 0.0;
	TestFalse(TEXT("Unknown course is not recorded"), Records->SubmitLap(NAME_None, Scout, EDroneControlMode::AssistedEasy, 30.0, bHad, Previous));
	TestFalse(TEXT("Non-positive time is not recorded"), Records->SubmitLap(Course, Scout, EDroneControlMode::AssistedEasy, 0.0, bHad, Previous));
	TestFalse(TEXT("NaN time is not recorded"), Records->SubmitLap(Course, Scout, EDroneControlMode::AssistedEasy, NAN, bHad, Previous));
	TestEqual(TEXT("Invalid laps leave no record"), Records->GetRecordCount(), 0);

	TestTrue(TEXT("First valid lap becomes the best"), Records->SubmitLap(Course, Scout, EDroneControlMode::AssistedEasy, 30.0, bHad, Previous));
	TestFalse(TEXT("First lap had no previous best"), bHad);
	TestFalse(TEXT("Slower lap does not replace the best"), Records->SubmitLap(Course, Scout, EDroneControlMode::AssistedEasy, 31.0, bHad, Previous));
	TestTrue(TEXT("Slower lap still reports the previous best"), bHad && FMath::IsNearlyEqual(Previous, 30.0));
	TestTrue(TEXT("Faster lap replaces the best"), Records->SubmitLap(Course, Scout, EDroneControlMode::AssistedEasy, 29.5, bHad, Previous));
	TestTrue(TEXT("Control mode is part of the key"), Records->SubmitLap(Course, Scout, EDroneControlMode::ManualRealisticGreybox, 40.0, bHad, Previous) && !bHad);

	// 재실행 복원
	UDroneTrainingRecordSubsystem* Reloaded = MakeRecords();
	TestEqual(TEXT("Saved file loads"), static_cast<int32>(Reloaded->GetLoadState()), static_cast<int32>(EDroneTrainingRecordLoadState::Loaded));
	double Best = 0.0;
	TestTrue(TEXT("Easy-mode best restores"), Reloaded->GetBestLap(Course, Scout, EDroneControlMode::AssistedEasy, Best) && FMath::IsNearlyEqual(Best, 29.5));
	TestTrue(TEXT("Manual-mode best restores separately"), Reloaded->GetBestLap(Course, Scout, EDroneControlMode::ManualRealisticGreybox, Best) && FMath::IsNearlyEqual(Best, 40.0));

	// 손상: JSON이 아닌 내용 → 엔진이 멈추지 않고 백업 후 새로 시작
	FFileHelper::SaveStringToFile(TEXT(" not json {{"), *UDroneTrainingRecordSubsystem::GetSlotFilePath(TestSlot));
	UDroneTrainingRecordSubsystem* AfterCorrupt = MakeRecords();
	TestEqual(TEXT("Corrupt file resets"), static_cast<int32>(AfterCorrupt->GetLoadState()), static_cast<int32>(EDroneTrainingRecordLoadState::CorruptReset));
	TestEqual(TEXT("Corrupt reset starts empty"), AfterCorrupt->GetRecordCount(), 0);
	TArray<FString> Backups;
	IFileManager::Get().FindFiles(Backups, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (TestSlot + TEXT("_Corrupt_*.json"))), true, false);
	TestTrue(TEXT("Corrupt file is backed up, not lost"), Backups.Num() >= 1);

	// 구버전
	FFileHelper::SaveStringToFile(TEXT("{\"schemaVersion\": 99, \"bestLapSeconds\": {\"x|y|0\": 10}}"),
		*UDroneTrainingRecordSubsystem::GetSlotFilePath(TestSlot));
	UDroneTrainingRecordSubsystem* AfterOutdated = MakeRecords();
	TestEqual(TEXT("Other schema version resets"), static_cast<int32>(AfterOutdated->GetLoadState()), static_cast<int32>(EDroneTrainingRecordLoadState::OutdatedReset));
	TestEqual(TEXT("Outdated reset starts empty"), AfterOutdated->GetRecordCount(), 0);

	DeleteTestSlotsAndBackups();
	return true;
}

#endif
