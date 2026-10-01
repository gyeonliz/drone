#include "UI/DroneAudioSettingsSubsystem.h"

#include "AudioDevice.h"
#include "AudioDeviceHandle.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

namespace DroneAudioSettings
{
const FString SaveSlot(TEXT("DroneAudioSettings"));
constexpr int32 SaveUserIndex = 0;
}

void UDroneAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGameplayStatics::DoesSaveGameExist(DroneAudioSettings::SaveSlot, DroneAudioSettings::SaveUserIndex))
	{
		if (const UDroneAudioSettingsSaveGame* Saved = Cast<UDroneAudioSettingsSaveGame>(
			UGameplayStatics::LoadGameFromSlot(DroneAudioSettings::SaveSlot, DroneAudioSettings::SaveUserIndex)))
		{
			if (FMath::IsFinite(Saved->MasterVolume))
			{
				MasterVolume = FMath::Clamp(Saved->MasterVolume, 0.0f, 1.0f);
				bHasSavedMasterVolume = true;
				bHasExplicitVolume = true;
			}
		}
	}
	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UDroneAudioSettingsSubsystem::HandleMapLoaded);
	// 초기 맵/Seamless Travel과 맵 로드 뒤 AudioDevice 생성 지연도 같은 경계에서 처리한다.
	WorldTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UDroneAudioSettingsSubsystem::HandleWorldPostActorTick);
	ApplyVolumeToWorld(GetWorld());
}

void UDroneAudioSettingsSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
	FWorldDelegates::OnWorldPostActorTick.Remove(WorldTickHandle);
	AppliedWorld.Reset();
	Super::Deinitialize();
}

void UDroneAudioSettingsSubsystem::SetMasterVolume(const float InVolume)
{
	if (!FMath::IsFinite(InVolume)) return;
	MasterVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	bHasExplicitVolume = true;
	AppliedWorld.Reset();
	ApplyVolumeToWorld(GetWorld());
}

bool UDroneAudioSettingsSubsystem::SaveMasterVolume()
{
	UDroneAudioSettingsSaveGame* Saved = Cast<UDroneAudioSettingsSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UDroneAudioSettingsSaveGame::StaticClass()));
	if (!Saved) return false;
	Saved->MasterVolume = MasterVolume;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		Saved, DroneAudioSettings::SaveSlot, DroneAudioSettings::SaveUserIndex);
	if (bSaved) bHasSavedMasterVolume = true;
	return bSaved;
}

void UDroneAudioSettingsSubsystem::HandleMapLoaded(UWorld* LoadedWorld)
{
	ApplyVolumeToWorld(LoadedWorld);
}

void UDroneAudioSettingsSubsystem::HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	ApplyVolumeToWorld(World);
}

void UDroneAudioSettingsSubsystem::ApplyVolumeToWorld(UWorld* World)
{
	// 다중 PIE에서는 다른 GameInstance나 Editor의 AudioDevice에 접근하지 않는다.
	if (!bHasExplicitVolume || !World || !GetGameInstance() || World->GetGameInstance() != GetGameInstance()) return;
	FAudioDeviceHandle AudioDevice = World->GetAudioDevice();
	if (!AudioDevice.IsValid()) return;
	if (AppliedWorld.Get() == World && AppliedAudioDeviceId == AudioDevice.GetDeviceID()) return;
	AudioDevice->SetTransientPrimaryVolume(MasterVolume);
	AppliedWorld = World;
	AppliedAudioDeviceId = AudioDevice.GetDeviceID();
}
