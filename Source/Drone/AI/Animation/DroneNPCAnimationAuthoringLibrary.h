#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DroneNPCAnimationAuthoringLibrary.generated.h"

/** 프로젝트 소유 NPC AnimBP에 재현 가능한 시선 Bone 체인을 구성하는 Editor 작성 도구다. */
UCLASS()
class DRONE_API UDroneNPCAnimationAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Rifle Greybox AnimBP를 Drone AnimInstance로 바꾸고 Spine/Neck/Head 시선 체인을 추가한다. */
	UFUNCTION(BlueprintCallable, Category="Drone|AI|Animation|Authoring")
	static bool UpgradeRifleAnimBlueprintForDroneGaze(const FString& AssetPath);

	/** Parent Class, Bone 노드와 출력 Pose 연결이 AI-GAZE 계약과 맞는지 검사한다. */
	UFUNCTION(BlueprintPure, Category="Drone|AI|Animation|Authoring")
	static bool ValidateRifleAnimBlueprintDroneGaze(const FString& AssetPath);
};
