#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "UI/DroneAudioSettingsSubsystem.h"
#include "UI/DroneSettingsWidget.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSettingsContractTest,
	"Drone.Flow.SettingsContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneSettingsContractTest::RunTest(const FString& Parameters)
{
	// 실제 사용자의 설정 파일/AudioDevice/GameUserSettings를 변경하지 않는 계약 검사다.
	UGameInstance* Instance = NewObject<UGameInstance>();
	UDroneAudioSettingsSubsystem* Audio = NewObject<UDroneAudioSettingsSubsystem>(Instance);
	TestEqual(TEXT("Unconfigured audio starts at full volume"), Audio->GetMasterVolume(), 1.f);
	Audio->SetMasterVolume(-3.f);
	TestEqual(TEXT("Negative volume is clamped"), Audio->GetMasterVolume(), 0.f);
	Audio->SetMasterVolume(3.f);
	TestEqual(TEXT("Excess volume is clamped"), Audio->GetMasterVolume(), 1.f);
	Audio->SetMasterVolume(.35f);
	Audio->SetMasterVolume(std::numeric_limits<float>::quiet_NaN());
	TestEqual(TEXT("Invalid input preserves existing volume"), Audio->GetMasterVolume(), .35f);
	TestFalse(TEXT("Preview does not save preferences"), Audio->HasSavedMasterVolume());

	UDroneSettingsWidget* Widget = NewObject<UDroneSettingsWidget>(Instance);
	Widget->SetMasterVolume(-2.f);
	TestEqual(TEXT("Widget slider API clamps low input"), Widget->GetMasterVolume(), 0.f);
	Widget->SetMasterVolume(2.f);
	TestEqual(TEXT("Widget slider API clamps high input"), Widget->GetMasterVolume(), 1.f);
	TestFalse(TEXT("Worldless/Designer settings cannot alter display mode"), Widget->AreDisplaySettingsAvailable());

	UDroneAudioSettingsSaveGame* Saved = NewObject<UDroneAudioSettingsSaveGame>();
	Saved->MasterVolume = .35f;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Audio preferences serialize without touching disk"), UGameplayStatics::SaveGameToMemory(Saved, Bytes));
	const UDroneAudioSettingsSaveGame* Reloaded = Cast<UDroneAudioSettingsSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("Audio preference class restores"), Reloaded);
	if (Reloaded) TestEqual(TEXT("Saved volume survives serialization"), Reloaded->MasterVolume, .35f);
	return !HasAnyErrors();
}

#endif
