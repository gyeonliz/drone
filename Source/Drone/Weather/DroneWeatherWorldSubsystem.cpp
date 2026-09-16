#include "Weather/DroneWeatherWorldSubsystem.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Weather/DroneWeatherProfile.h"

namespace DroneWeather
{
constexpr float DefaultUpdateHertz = 10.0f;
constexpr float MaximumTurbulenceYawDegrees = 20.0f;

float ResponseAlpha(const float DeltaSeconds, const float ResponseSeconds)
{
	return 1.0f - FMath::Exp(-FMath::Max(0.0f, DeltaSeconds) / FMath::Max(0.01f, ResponseSeconds));
}

FDroneWeatherSnapshot MakeClearSnapshot()
{
	FDroneWeatherSnapshot Clear;
	Clear.VisibilityDistanceCentimeters = 100000.0f;
	Clear.IndoorRainAttenuation01 = 1.0f;
	return Clear;
}
}

void UDroneWeatherWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WeatherUpdateTimer);
	}
	ActiveProfile = nullptr;
	Super::Deinitialize();
}

bool UDroneWeatherWorldSubsystem::ApplyWeatherProfile(
	UDroneWeatherProfile* Profile,
	const bool bInstantTransition)
{
	FString Error;
	if (!IsValid(Profile) || !Profile->ValidateProfile(Error))
	{
		return false;
	}

	TransitionStartSnapshot = Snapshot;
	TransitionElapsedSeconds = bInstantTransition ? Profile->TransitionSeconds : 0.0f;
	ActiveProfile = Profile;
	bClearingWeather = false;
	GustRandomStream.Initialize(Profile->RandomSeed);
	SecondsUntilNextGust = 0.0f;
	CurrentGustSpeedMetersPerSecond = 0.0f;
	TargetGustSpeedMetersPerSecond = 0.0f;
	CurrentGustYawOffsetDegrees = 0.0f;
	TargetGustYawOffsetDegrees = 0.0f;
	CurrentVerticalGustMetersPerSecond = 0.0f;
	TargetVerticalGustMetersPerSecond = 0.0f;
	RestartUpdateTimer();
	ForceWeatherUpdate(bInstantTransition ? FMath::Max(Profile->TransitionSeconds, 0.0f) : 0.0f);
	return true;
}

void UDroneWeatherWorldSubsystem::ClearWeather(const bool bInstantTransition)
{
	TransitionStartSnapshot = Snapshot;
	TransitionElapsedSeconds = 0.0f;
	bClearingWeather = true;
	if (bInstantTransition || !ActiveProfile || ActiveProfile->TransitionSeconds <= 0.0f)
	{
		ActiveProfile = nullptr;
		bClearingWeather = false;
		PublishSnapshot(DroneWeather::MakeClearSnapshot());
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(WeatherUpdateTimer);
		}
		return;
	}
	RestartUpdateTimer();
}

void UDroneWeatherWorldSubsystem::RestartUpdateTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Hertz = ActiveProfile
		? FMath::Clamp(ActiveProfile->GameplayUpdateHertz, 1.0f, 30.0f)
		: DroneWeather::DefaultUpdateHertz;
	const float IntervalSeconds = 1.0f / Hertz;
	World->GetTimerManager().SetTimer(
		WeatherUpdateTimer,
		this,
		&UDroneWeatherWorldSubsystem::HandleWeatherTimer,
		IntervalSeconds,
		true,
		IntervalSeconds);
}

void UDroneWeatherWorldSubsystem::HandleWeatherTimer()
{
	const float Hertz = ActiveProfile
		? FMath::Clamp(ActiveProfile->GameplayUpdateHertz, 1.0f, 30.0f)
		: DroneWeather::DefaultUpdateHertz;
	ForceWeatherUpdate(1.0f / Hertz);
}

void UDroneWeatherWorldSubsystem::ForceWeatherUpdate(const float DeltaSeconds)
{
	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	FDroneWeatherSnapshot Target = bClearingWeather
		? DroneWeather::MakeClearSnapshot()
		: BuildTargetSnapshot(SafeDeltaSeconds);

	const float TransitionDuration = ActiveProfile
		? FMath::Max(0.0f, ActiveProfile->TransitionSeconds)
		: 0.0f;
	TransitionElapsedSeconds += SafeDeltaSeconds;
	const float Alpha = TransitionDuration <= UE_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(TransitionElapsedSeconds / TransitionDuration, 0.0f, 1.0f);
	Target.TransitionAlpha = Alpha;
	PublishSnapshot(InterpolateSnapshot(TransitionStartSnapshot, Target, Alpha));

	if (bClearingWeather && Alpha >= 1.0f)
	{
		ActiveProfile = nullptr;
		bClearingWeather = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(WeatherUpdateTimer);
		}
	}
}

FDroneWeatherSnapshot UDroneWeatherWorldSubsystem::BuildTargetSnapshot(const float DeltaSeconds)
{
	if (!ActiveProfile)
	{
		return DroneWeather::MakeClearSnapshot();
	}

	const FDroneWindSettings& Wind = ActiveProfile->Wind;
	SecondsUntilNextGust -= DeltaSeconds;
	if (SecondsUntilNextGust <= 0.0f)
	{
		TargetGustSpeedMetersPerSecond = GustRandomStream.FRandRange(
			0.0f,
			FMath::Max(0.0f, Wind.GustAdditionalSpeedMetersPerSecond));
		TargetGustYawOffsetDegrees = GustRandomStream.FRandRange(-1.0f, 1.0f)
			* DroneWeather::MaximumTurbulenceYawDegrees
			* FMath::Clamp(Wind.Turbulence01, 0.0f, 1.0f);
		TargetVerticalGustMetersPerSecond = GustRandomStream.FRandRange(
			-FMath::Max(0.0f, Wind.VerticalGustMetersPerSecond),
			FMath::Max(0.0f, Wind.VerticalGustMetersPerSecond));
		SecondsUntilNextGust = GustRandomStream.FRandRange(
			FMath::Max(0.1f, Wind.GustIntervalSeconds.X),
			FMath::Max(Wind.GustIntervalSeconds.X, Wind.GustIntervalSeconds.Y));
	}

	const float GustResponseSeconds = TargetGustSpeedMetersPerSecond > CurrentGustSpeedMetersPerSecond
		? Wind.GustAttackSeconds
		: Wind.GustReleaseSeconds;
	CurrentGustSpeedMetersPerSecond = FMath::Lerp(
		CurrentGustSpeedMetersPerSecond,
		TargetGustSpeedMetersPerSecond,
		DroneWeather::ResponseAlpha(DeltaSeconds, GustResponseSeconds));

	const float YawDeltaDegrees = FMath::FindDeltaAngleDegrees(
		CurrentGustYawOffsetDegrees,
		TargetGustYawOffsetDegrees);
	CurrentGustYawOffsetDegrees = FMath::UnwindDegrees(
		CurrentGustYawOffsetDegrees
		+ YawDeltaDegrees * DroneWeather::ResponseAlpha(DeltaSeconds, Wind.DirectionResponseSeconds));

	const float VerticalResponseSeconds = FMath::Abs(TargetVerticalGustMetersPerSecond)
		> FMath::Abs(CurrentVerticalGustMetersPerSecond)
			? Wind.GustAttackSeconds
			: Wind.GustReleaseSeconds;
	CurrentVerticalGustMetersPerSecond = FMath::Lerp(
		CurrentVerticalGustMetersPerSecond,
		TargetVerticalGustMetersPerSecond,
		DroneWeather::ResponseAlpha(DeltaSeconds, VerticalResponseSeconds));

	const float WindYawRadians = FMath::DegreesToRadians(Wind.DirectionYawDegrees + CurrentGustYawOffsetDegrees);
	const float HorizontalSpeedCentimetersPerSecond =
		(Wind.BaseSpeedMetersPerSecond + CurrentGustSpeedMetersPerSecond) * 100.0f;

	FDroneWeatherSnapshot Target;
	Target.WeatherId = ActiveProfile->WeatherId;
	Target.WindVelocityCentimetersPerSecond = FVector(
		FMath::Cos(WindYawRadians) * HorizontalSpeedCentimetersPerSecond,
		FMath::Sin(WindYawRadians) * HorizontalSpeedCentimetersPerSecond,
		CurrentVerticalGustMetersPerSecond * 100.0f);
	Target.DroneWindResponseMultiplier = Wind.DroneWindResponseMultiplier;
	Target.RainIntensity01 = ActiveProfile->Rain.Intensity01;
	Target.RainSpawnScale01 = ActiveProfile->Rain.SpawnScale01;
	Target.VisibilityDistanceCentimeters = ActiveProfile->Rain.VisibilityDistanceMeters * 100.0f;
	Target.ScreenDropletIntensity01 = ActiveProfile->Rain.ScreenDropletIntensity01;
	Target.SurfaceWetness01 = ActiveProfile->Rain.SurfaceWetness01;
	Target.GroundSplashScale01 = ActiveProfile->Rain.GroundSplashScale01;
	Target.IndoorRainAttenuation01 = ActiveProfile->Rain.IndoorRainAttenuation01;
	Target.RainAudioVolume01 = ActiveProfile->Rain.AudioVolume01;
	return Target;
}

void UDroneWeatherWorldSubsystem::PublishSnapshot(const FDroneWeatherSnapshot& NewSnapshot)
{
	const bool bChanged = Snapshot.WeatherId != NewSnapshot.WeatherId
		|| !Snapshot.WindVelocityCentimetersPerSecond.Equals(NewSnapshot.WindVelocityCentimetersPerSecond, 0.1f)
		|| !FMath::IsNearlyEqual(Snapshot.DroneWindResponseMultiplier, NewSnapshot.DroneWindResponseMultiplier)
		|| !FMath::IsNearlyEqual(Snapshot.RainIntensity01, NewSnapshot.RainIntensity01)
		|| !FMath::IsNearlyEqual(Snapshot.RainSpawnScale01, NewSnapshot.RainSpawnScale01)
		|| !FMath::IsNearlyEqual(Snapshot.VisibilityDistanceCentimeters, NewSnapshot.VisibilityDistanceCentimeters)
		|| !FMath::IsNearlyEqual(Snapshot.ScreenDropletIntensity01, NewSnapshot.ScreenDropletIntensity01)
		|| !FMath::IsNearlyEqual(Snapshot.SurfaceWetness01, NewSnapshot.SurfaceWetness01)
		|| !FMath::IsNearlyEqual(Snapshot.GroundSplashScale01, NewSnapshot.GroundSplashScale01)
		|| !FMath::IsNearlyEqual(Snapshot.IndoorRainAttenuation01, NewSnapshot.IndoorRainAttenuation01)
		|| !FMath::IsNearlyEqual(Snapshot.RainAudioVolume01, NewSnapshot.RainAudioVolume01)
		|| !FMath::IsNearlyEqual(Snapshot.TransitionAlpha, NewSnapshot.TransitionAlpha);
	Snapshot = NewSnapshot;
	if (bChanged)
	{
		OnWeatherSnapshotChanged.Broadcast(Snapshot);
	}
}

FDroneWeatherSnapshot UDroneWeatherWorldSubsystem::InterpolateSnapshot(
	const FDroneWeatherSnapshot& From,
	const FDroneWeatherSnapshot& To,
	const float Alpha)
{
	const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
	FDroneWeatherSnapshot Result;
	Result.WeatherId = T >= 1.0f ? To.WeatherId : From.WeatherId;
	Result.WindVelocityCentimetersPerSecond = FMath::Lerp(From.WindVelocityCentimetersPerSecond, To.WindVelocityCentimetersPerSecond, T);
	Result.DroneWindResponseMultiplier = FMath::Lerp(From.DroneWindResponseMultiplier, To.DroneWindResponseMultiplier, T);
	Result.RainIntensity01 = FMath::Lerp(From.RainIntensity01, To.RainIntensity01, T);
	Result.RainSpawnScale01 = FMath::Lerp(From.RainSpawnScale01, To.RainSpawnScale01, T);
	Result.VisibilityDistanceCentimeters = FMath::Lerp(From.VisibilityDistanceCentimeters, To.VisibilityDistanceCentimeters, T);
	Result.ScreenDropletIntensity01 = FMath::Lerp(From.ScreenDropletIntensity01, To.ScreenDropletIntensity01, T);
	Result.SurfaceWetness01 = FMath::Lerp(From.SurfaceWetness01, To.SurfaceWetness01, T);
	Result.GroundSplashScale01 = FMath::Lerp(From.GroundSplashScale01, To.GroundSplashScale01, T);
	Result.IndoorRainAttenuation01 = FMath::Lerp(From.IndoorRainAttenuation01, To.IndoorRainAttenuation01, T);
	Result.RainAudioVolume01 = FMath::Lerp(From.RainAudioVolume01, To.RainAudioVolume01, T);
	Result.TransitionAlpha = T;
	return Result;
}
