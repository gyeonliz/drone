#include "Weather/DroneWeatherProfile.h"

FPrimaryAssetId UDroneWeatherProfile::GetPrimaryAssetId() const
{
	return WeatherId.IsNone()
		? Super::GetPrimaryAssetId()
		: FPrimaryAssetId(FPrimaryAssetType(TEXT("DroneWeatherProfile")), WeatherId);
}

bool UDroneWeatherProfile::ValidateProfile(FString& OutError) const
{
	if (WeatherId.IsNone())
	{
		OutError = TEXT("WeatherId가 비어 있습니다.");
		return false;
	}
	if (!FMath::IsFinite(GameplayUpdateHertz)
		|| !FMath::IsWithinInclusive(GameplayUpdateHertz, 1.0f, 30.0f))
	{
		OutError = TEXT("Gameplay Update Hertz는 1~30 범위여야 합니다.");
		return false;
	}
	if (!FMath::IsFinite(TransitionSeconds)
		|| !FMath::IsWithinInclusive(TransitionSeconds, 0.0f, 60.0f))
	{
		OutError = TEXT("Transition Seconds는 0~60초 범위여야 합니다.");
		return false;
	}
	if (!FMath::IsFinite(Wind.DirectionYawDegrees)
		|| !FMath::IsFinite(Wind.BaseSpeedMetersPerSecond)
		|| !FMath::IsFinite(Wind.GustAdditionalSpeedMetersPerSecond)
		|| !FMath::IsFinite(Wind.GustIntervalSeconds.X)
		|| !FMath::IsFinite(Wind.GustIntervalSeconds.Y)
		|| !FMath::IsFinite(Wind.Turbulence01)
		|| !FMath::IsFinite(Wind.VerticalGustMetersPerSecond)
		|| !FMath::IsFinite(Wind.DroneWindResponseMultiplier)
		|| Wind.BaseSpeedMetersPerSecond < 0.0f
		|| Wind.GustAdditionalSpeedMetersPerSecond < 0.0f
		|| Wind.GustIntervalSeconds.X < 0.1f
		|| Wind.GustIntervalSeconds.Y < Wind.GustIntervalSeconds.X
		|| !FMath::IsWithinInclusive(Wind.Turbulence01, 0.0f, 1.0f)
		|| Wind.VerticalGustMetersPerSecond < 0.0f
		|| !FMath::IsWithinInclusive(Wind.DroneWindResponseMultiplier, 0.0f, 2.0f))
	{
		OutError = TEXT("Wind 설정에 음수, 비정상 값 또는 뒤집힌 Gust 간격이 있습니다.");
		return false;
	}

	const bool bRainScalarsValid = FMath::IsWithinInclusive(Rain.Intensity01, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(Rain.SpawnScale01, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(Rain.ScreenDropletIntensity01, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(Rain.SurfaceWetness01, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(Rain.GroundSplashScale01, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(Rain.IndoorRainAttenuation01, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(Rain.AudioVolume01, 0.0f, 1.0f);
	if (!bRainScalarsValid
		|| !FMath::IsFinite(Rain.VisibilityDistanceMeters)
		|| Rain.VisibilityDistanceMeters < 1.0f)
	{
		OutError = TEXT("Rain 0~1 값 또는 Visibility Distance가 잘못됐습니다.");
		return false;
	}

	OutError.Reset();
	return true;
}

bool UDroneWeatherProfile::IsProfileValid() const
{
	FString Error;
	return ValidateProfile(Error);
}
