#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DroneAudioSettingsSubsystem.generated.h"

class UWorld;

/** GameUserSettings에는 없는 전체 음량만 프로젝트 소유 SaveGame에 저장한다. */
UCLASS()
class DRONE_API UDroneAudioSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	float MasterVolume = 1.0f;
};

/** 로비에서 설정한 전체 음량을 같은 GameInstance의 다음 맵에서도 유지한다. */
UCLASS()
class DRONE_API UDroneAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Drone|Settings|Audio")
	void SetMasterVolume(float InVolume);

	UFUNCTION(BlueprintPure, Category="Drone|Settings|Audio")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category="Drone|Settings|Audio")
	bool HasSavedMasterVolume() const { return bHasSavedMasterVolume; }

	/** 미리보기 중에는 저장하지 않는다. 적용 버튼을 누를 때만 호출한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|Settings|Audio")
	bool SaveMasterVolume();

private:
	void HandleMapLoaded(UWorld* LoadedWorld);
	void HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	void ApplyVolumeToWorld(UWorld* World);

	float MasterVolume = 1.0f;
	bool bHasSavedMasterVolume = false;
	bool bHasExplicitVolume = false;
	TWeakObjectPtr<UWorld> AppliedWorld;
	uint32 AppliedAudioDeviceId = MAX_uint32;
	FDelegateHandle MapLoadedHandle;
	FDelegateHandle WorldTickHandle;
};
