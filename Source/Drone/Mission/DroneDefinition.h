#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Prototype/DroneFlightControlTypes.h"
#include "DroneDefinition.generated.h"

class AActor;
class APawn;
class UStaticMesh;

/** Figma 기획에서 확인한 임무상 역할이다. 역할 존재와 현재 빌드의 구현 완료 여부는 별도로 관리한다. */
UENUM(BlueprintType)
enum class EDroneMissionRole : uint8
{
	Reconnaissance UMETA(DisplayName="정찰 드론"),
	DropDelivery UMETA(DisplayName="드랍 드론"),
	FPVStrike UMETA(DisplayName="FPV 자폭 드론"),
	FiberOpticStrike UMETA(DisplayName="광섬유 드론"),
	GroundUGV UMETA(DisplayName="지상 드론 UGV"),
	LongRangeStrike UMETA(DisplayName="장거리 타격 드론")
};

/** 기획된 기능과 실제 구현된 기능을 분리해 선택 UI가 미구현 기능을 완료된 것처럼 표시하지 않게 한다. */
UENUM(BlueprintType)
enum class EDroneGameplayCapability : uint8
{
	ReconScan UMETA(DisplayName="정찰 스캔"),
	PayloadDrop UMETA(DisplayName="물자/폭탄 투하"),
	ImpactDetonation UMETA(DisplayName="충돌 자폭"),
	JammingImmunity UMETA(DisplayName="재밍 면역"),
	GroundDrive UMETA(DisplayName="지상 주행"),
	LongRangeStrikeSequence UMETA(DisplayName="장거리 타격 연출")
};

/** 같은 Prototype Pawn에 적용해 실제 조종 차이를 만드는 데이터 기반 비행 수치다. */
USTRUCT(BlueprintType)
struct DRONE_API FDroneFlightProfile
{
	GENERATED_BODY()

	/** Spawn 직후 기본값일 뿐이며 플레이 중 Blueprint/UI에서 별도로 바꿀 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Control")
	EDroneControlMode DefaultControlMode = EDroneControlMode::AssistedEasy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Control")
	EDroneHandlingPreset DefaultHandlingPreset = EDroneHandlingPreset::Balanced;

	/** FPV Rate/Acro 모드에서 사용하는 각속도 곡선과 수직 속도 제한이다. 다른 조작 모드에는 영향이 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Control", meta=(ShowOnlyInnerProperties))
	FDroneAcroRateSettings AcroRateSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Movement", meta=(ClampMin="1.0", ForceUnits="cm/s"))
	float MaxSpeedCentimetersPerSecond = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Movement", meta=(ClampMin="1.0", ForceUnits="cm/s^2"))
	float AccelerationCentimetersPerSecondSquared = 2400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Movement", meta=(ClampMin="1.0", ForceUnits="cm/s^2"))
	float DecelerationCentimetersPerSecondSquared = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Movement", meta=(ClampMin="0.0"))
	float TurningBoost = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Movement", meta=(ClampMin="1.0", ForceUnits="deg/s"))
	float YawRateDegreesPerSecond = 90.0f;

	/** 이동 입력에 따른 외형 기울기이며 충돌과 실제 이동 방향에는 영향을 주지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Presentation", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg"))
	float MaximumVisualBankRollDegrees = 18.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Presentation", meta=(ClampMin="0.0", ClampMax="45.0", ForceUnits="deg"))
	float MaximumVisualTiltPitchDegrees = 14.0f;

	/** true면 Spawn 뒤 FPV, false면 3인칭 추적 시점으로 시작한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Presentation")
	bool bStartInFirstPersonView = false;

	/** 현재 공통 기본값은 100이며 후보 프리셋도 우선 동일하게 유지한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Survivability", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	/** FLOW-05 선택 카드가 구현된 차이만 사용자에게 설명할 수 있는 짧은 문구다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Flight Profile|Display")
	TArray<FText> FeatureHighlights;

	bool ValidateProfile(FString& OutError) const;
};

/** Drone 선택 화면과 실제 Spawn이 함께 참조하는 프로젝트 소유 데이터 계약이다. */
UCLASS(BlueprintType)
class DRONE_API UDroneDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** 비어 있지 않고 전체 Drone Catalog에서 유일해야 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Identity")
	FName DroneId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Display", meta=(MultiLine="true"))
	FText Description;

	/** 선택 화면의 가벼운 Preview 후보다. 비워 두고 PreviewActorClass를 사용할 수도 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Preview")
	TSoftObjectPtr<UStaticMesh> PreviewMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Preview")
	TSoftClassPtr<AActor> PreviewActorClass;

	/** Drone 확정 뒤 Spawn/Possess할 프로젝트 Integration Pawn Class다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	TSoftClassPtr<APawn> PawnClass;

	/** 기체 역할은 조작 방식 및 느림/보통/빠름 속도 단계와 독립적이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	EDroneMissionRole MissionRole = EDroneMissionRole::Reconnaissance;

	/** false인 기체는 기획/연출 후보로는 존재하지만 FLOW-05 플레이 기체 목록에는 노출하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	bool bPlayerControllableInCurrentBuild = true;

	/** 선택된 Definition이 Spawn된 Prototype Pawn에 적용할 실제 기능 차이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	FDroneFlightProfile FlightProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	FGameplayTagContainer RoleTags;

	/** 기획에는 있으나 아직 동작하지 않아도 되는 기능 목록이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Capabilities")
	TArray<EDroneGameplayCapability> PlannedCapabilities;

	/** 자동화 테스트까지 통과해 현재 빌드에서 실제 사용할 수 있는 기능만 넣는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Capabilities")
	TArray<EDroneGameplayCapability> ImplementedCapabilities;

	/** 잠긴 Drone은 Mission 허용 목록에 있어도 선택할 수 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drone Definition|Runtime")
	bool bLocked = false;

	bool ValidateDefinition(FString& OutError) const;

	UFUNCTION(BlueprintPure, Category="Drone|Flow|Data")
	bool IsDefinitionValid() const;
};
