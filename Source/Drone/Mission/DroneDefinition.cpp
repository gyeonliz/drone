#include "Mission/DroneDefinition.h"

bool FDroneFlightProfile::ValidateProfile(FString& OutError) const
{
	if (!FMath::IsFinite(MaxSpeedCentimetersPerSecond) || MaxSpeedCentimetersPerSecond <= 0.0f)
	{
		OutError = TEXT("최대 속도는 0보다 커야 합니다.");
		return false;
	}
	if (!FMath::IsFinite(AccelerationCentimetersPerSecondSquared)
		|| AccelerationCentimetersPerSecondSquared <= 0.0f)
	{
		OutError = TEXT("가속도는 0보다 커야 합니다.");
		return false;
	}
	if (!FMath::IsFinite(DecelerationCentimetersPerSecondSquared)
		|| DecelerationCentimetersPerSecondSquared <= 0.0f)
	{
		OutError = TEXT("감속도는 0보다 커야 합니다.");
		return false;
	}
	if (!FMath::IsFinite(TurningBoost) || TurningBoost < 0.0f)
	{
		OutError = TEXT("Turning Boost는 0 이상이어야 합니다.");
		return false;
	}
	if (!FMath::IsFinite(YawRateDegreesPerSecond) || YawRateDegreesPerSecond <= 0.0f)
	{
		OutError = TEXT("Yaw 회전 속도는 0보다 커야 합니다.");
		return false;
	}
	if (!FMath::IsWithinInclusive(MaximumVisualBankRollDegrees, 0.0f, 45.0f)
		|| !FMath::IsWithinInclusive(MaximumVisualTiltPitchDegrees, 0.0f, 45.0f))
	{
		OutError = TEXT("외형 Pitch/Roll 기울기는 0~45도 범위여야 합니다.");
		return false;
	}
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 1.0f)
	{
		OutError = TEXT("최대 체력은 1 이상이어야 합니다.");
		return false;
	}

	OutError.Reset();
	return true;
}

FPrimaryAssetId UDroneDefinition::GetPrimaryAssetId() const
{
	return DroneId.IsNone()
		? Super::GetPrimaryAssetId()
		: FPrimaryAssetId(FPrimaryAssetType(TEXT("DroneDefinition")), DroneId);
}

bool UDroneDefinition::ValidateDefinition(FString& OutError) const
{
	if (DroneId.IsNone())
	{
		OutError = TEXT("DroneId가 비어 있습니다.");
		return false;
	}
	if (DisplayName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Drone '%s'의 표시명이 비어 있습니다."), *DroneId.ToString());
		return false;
	}
	if (bPlayerControllableInCurrentBuild && PawnClass.IsNull())
	{
		OutError = FString::Printf(TEXT("플레이 가능한 Drone '%s'의 PawnClass가 비어 있습니다."), *DroneId.ToString());
		return false;
	}

	for (const EDroneGameplayCapability ImplementedCapability : ImplementedCapabilities)
	{
		if (!PlannedCapabilities.Contains(ImplementedCapability))
		{
			OutError = FString::Printf(
				TEXT("Drone '%s'의 구현 기능은 먼저 PlannedCapabilities에도 등록해야 합니다."),
				*DroneId.ToString());
			return false;
		}
	}

	FString ProfileError;
	if (!FlightProfile.ValidateProfile(ProfileError))
	{
		OutError = FString::Printf(
			TEXT("Drone '%s'의 FlightProfile이 잘못됐습니다: %s"),
			*DroneId.ToString(),
			*ProfileError);
		return false;
	}

	OutError.Reset();
	return true;
}

bool UDroneDefinition::IsDefinitionValid() const
{
	FString Error;
	return ValidateDefinition(Error);
}
