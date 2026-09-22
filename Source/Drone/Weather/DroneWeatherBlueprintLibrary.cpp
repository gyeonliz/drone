#include "Weather/DroneWeatherBlueprintLibrary.h"

float UDroneWeatherBlueprintLibrary::GetHorizontalWindSpeedMetersPerSecond(
	const FVector WindVelocityCentimetersPerSecond)
{
	return FVector2D(
		WindVelocityCentimetersPerSecond.X,
		WindVelocityCentimetersPerSecond.Y).Size() / 100.0f;
}

FString UDroneWeatherBlueprintLibrary::GetWindCardinalLabel(
	const FVector WindVelocityCentimetersPerSecond)
{
	if (GetHorizontalWindSpeedMetersPerSecond(WindVelocityCentimetersPerSecond) < 0.05f)
	{
		return TEXT("CALM");
	}
	static const TCHAR* Labels[] = {
		TEXT("E"), TEXT("NE"), TEXT("N"), TEXT("NW"),
		TEXT("W"), TEXT("SW"), TEXT("S"), TEXT("SE")
	};
	const float Yaw = FRotator::ClampAxis(FMath::RadiansToDegrees(FMath::Atan2(
		WindVelocityCentimetersPerSecond.Y,
		WindVelocityCentimetersPerSecond.X)));
	const int32 Index = FMath::RoundToInt(Yaw / 45.0f) % UE_ARRAY_COUNT(Labels);
	return Labels[Index];
}

FText UDroneWeatherBlueprintLibrary::FormatWindReadout(
	const FVector WindVelocityCentimetersPerSecond)
{
	return FText::FromString(FString::Printf(
		TEXT("풍향 %s  |  풍속 %.1f m/s"),
		*GetWindCardinalLabel(WindVelocityCentimetersPerSecond),
		GetHorizontalWindSpeedMetersPerSecond(WindVelocityCentimetersPerSecond)));
}
