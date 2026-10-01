#include "Weather/DroneRainPerformanceProbe.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "Weather/DroneRainVisualActor.h"
#include "Weather/DroneWeatherProfile.h"
#include "Weather/DroneWeatherWorldSubsystem.h"

ADroneRainPerformanceProbe::ADroneRainPerformanceProbe()
{
	PrimaryActorTick.bCanEverTick = true;
}
void ADroneRainPerformanceProbe::BeginPlay()
{
	Super::BeginPlay();
	// 이름이 비 시스템인 컴포넌트만 제어한다. 다른 Niagara(불꽃/연기 등)는 건드리지 않는다.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<UNiagaraComponent*> Components(*It);
		for (UNiagaraComponent* Component : Components)
		{
			if (Component->GetAsset() && Component->GetAsset()->GetPathName().Contains(TEXT("/OilRigPreview/Rain/Niagara/")))
			{
				OriginalRain.Add(Component);
				if (OriginalRain.Num() == 1 || !OriginalRain.ContainsByPredicate([Component](const UNiagaraComponent* Other)
					{ return Other != Component && Other->GetAsset() == Component->GetAsset(); }))
				{
					UNiagaraSystem* System = Component->GetAsset();
					UE_LOG(LogTemp, Display, TEXT("DRONE_RAIN_PROBE|System=%s|FixedBoundsEnabled=%d|EffectType=%s|Emitters=%d"),
						*System->GetName(), System->bFixedBounds, *GetNameSafe(System->GetEffectType()), System->GetNumEmitters());
					for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
					{
						if (const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
							UE_LOG(LogTemp, Display, TEXT("DRONE_RAIN_PROBE|Emitter=%s|Enabled=%d|SimTarget=%d|LocalSpace=%d|Renderers=%d"),
								*Handle.GetName().ToString(), Handle.GetIsEnabled(), static_cast<int32>(Data->SimTarget), Data->bLocalSpace, Data->GetRenderers().Num());
					}
				}
			}
		}
	}
	if (UDroneWeatherWorldSubsystem* Weather = GetWorld()->GetSubsystem<UDroneWeatherWorldSubsystem>())
	{
		Weather->ApplyWeatherProfile(LoadObject<UDroneWeatherProfile>(nullptr,
			TEXT("/Game/Drone/Data/Weather/DA_Weather_RainStorm_Greybox.DA_Weather_RainStorm_Greybox")), true);
		Weather->SetRuntimeWindOverride(0.f, 0.f); // 비교 중에는 바람/카메라 움직임을 통제한다.
	}
	UClass* RainClass = LoadClass<ADroneRainVisualActor>(nullptr,
		TEXT("/Game/Drone/Weather/Blueprints/BP_DroneRainVisual.BP_DroneRainVisual_C"));
	if (RainClass) ProjectRain = GetWorld()->SpawnActor<ADroneRainVisualActor>(RainClass);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (bUseFixedComparisonCamera) PC->SetViewTarget(this);
		EnableInput(PC);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ADroneRainPerformanceProbe::CycleMode);
	}
	bAutomated = FParse::Param(FCommandLine::Get(), TEXT("DroneRainBenchmark"));
	PreviousFrameTime = FPlatformTime::Seconds();
	SetRainComparisonMode(1);
	UE_LOG(LogTemp, Display, TEXT("DRONE_RAIN_PROBE|Ready|OriginalSystems=%d|R cycles 1/0/2/3"), OriginalRain.Num());
}
void ADroneRainPerformanceProbe::SetRainComparisonMode(const int32 Mode)
{
	CurrentMode = FMath::Clamp(Mode, 0, 3);
	TArray<UNiagaraComponent*> Nearby;
	const FVector CameraLocation = UGameplayStatics::GetPlayerCameraManager(this, 0)
		? UGameplayStatics::GetPlayerCameraManager(this, 0)->GetCameraLocation() : GetActorLocation();
	for (UNiagaraComponent* Component : OriginalRain)
	{
		if (IsValid(Component)) Nearby.Add(Component);
	}
	Nearby.Sort([CameraLocation](const UNiagaraComponent& A, const UNiagaraComponent& B)
	{
		return FVector::DistSquared(A.GetComponentLocation(), CameraLocation)
			< FVector::DistSquared(B.GetComponentLocation(), CameraLocation);
	});
	ActiveOriginalCount = 0;
	for (int32 Index = 0; Index < Nearby.Num(); ++Index)
	{
		UNiagaraComponent* Component = Nearby[Index];
		const bool bEnabled = CurrentMode == 1 || (CurrentMode == 2
			&& Index < MaximumNearbyOriginalSystems
			&& FVector::DistSquared(Component->GetComponentLocation(), CameraLocation) <= FMath::Square(OriginalRainCullDistanceCentimeters));
		Component->SetVisibility(bEnabled, true);
		// Deactivate만 하면 기존 입자가 죽기 전까지 업데이트한다. 즉시 중단해 비용 차이를 분리한다.
		if (bEnabled) { Component->Activate(true); ++ActiveOriginalCount; }
		else Component->DeactivateImmediate();
	}
	if (ProjectRain) ProjectRain->SetRainEnabled(CurrentMode == 3);
	UE_LOG(LogTemp, Display, TEXT("DRONE_RAIN_PROBE|Mode=%d|OriginalActive=%d|ProjectRain=%d"), CurrentMode, ActiveOriginalCount, CurrentMode == 3);
}
void ADroneRainPerformanceProbe::CycleMode()
{
	const int32 Next = CurrentMode == 1 ? 0 : CurrentMode == 0 ? 2 : CurrentMode == 2 ? 3 : 1;
	SetRainComparisonMode(Next);
}
void ADroneRainPerformanceProbe::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = FPlatformTime::Seconds();
	const float FrameMs = static_cast<float>((Now - PreviousFrameTime) * 1000.0);
	PreviousFrameTime = Now;
	if (!bAutomated) return;
	StageElapsedSeconds += FrameMs * .001f;
	// 단계마다 10초 안정화 후 10초 측정. 셰이더 컴파일/타 앱 부하는 별도 관찰해야 한다.
	if (StageElapsedSeconds >= 10.f) FrameSamples.Add(FrameMs);
	if (StageElapsedSeconds < 20.f) return;
	FrameSamples.Sort();
	if (!FrameSamples.IsEmpty())
	{
		double Sum = 0.; for (float Sample : FrameSamples) Sum += Sample;
		const float Median = FrameSamples[FrameSamples.Num() / 2];
		const float P95 = FrameSamples[FMath::Min(FrameSamples.Num() - 1, FMath::FloorToInt(FrameSamples.Num() * .95f))];
		const FString Row = FString::Printf(TEXT("%d,%d,%d,%d,%.3f,%.3f,%.3f"), AutomatedStage, CurrentMode,
			ActiveOriginalCount, FrameSamples.Num(), Sum / FrameSamples.Num(), Median, P95);
		MeasurementRows.Add(Row);
		UE_LOG(LogTemp, Display, TEXT("DRONE_RAIN_PROBE|Sample|%s"), *Row);
	}
	FrameSamples.Reset(); StageElapsedSeconds = 0.f;
	// 순서에 따른 캐시/온도 편향을 줄이도록 역순을 포함해 두 번 측정한다.
	static const int32 Modes[] = {1, 0, 2, 3, 3, 2, 0, 1};
	if (++AutomatedStage < UE_ARRAY_COUNT(Modes)) SetRainComparisonMode(Modes[AutomatedStage]);
	else
	{
		SaveMeasurements(); bAutomated = false;
		UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
	}
}
void ADroneRainPerformanceProbe::SaveMeasurements()
{
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/GameReadiness");
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Data = TEXT("stage,mode,original_active,samples,mean_frame_ms,median_frame_ms,p95_frame_ms\n")
		+ FString::Join(MeasurementRows, TEXT("\n"));
	FFileHelper::SaveStringToFile(Data, *(Directory / TEXT("oilrig_rain_frame_times.csv")));
	UE_LOG(LogTemp, Display, TEXT("DRONE_RAIN_PROBE|SUCCESS|Frame intervals only; not isolated CPU/GPU timings"));
}
