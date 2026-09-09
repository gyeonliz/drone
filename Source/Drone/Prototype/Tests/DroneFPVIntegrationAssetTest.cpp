#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/DroneImpactDetonationComponent.h"
#include "Prototype/DronePrototypeGameMode.h"
#include "Prototype/DronePrototypePawn.h"
#include "Tests/AutomationCommon.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

namespace DroneFPVIntegrationAssets
{
constexpr const TCHAR* PawnClassPath =
	TEXT("/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration.BP_DroneFPVIntegration_C");
constexpr const TCHAR* GameModeClassPath =
	TEXT("/Game/Drone/Prototype/Blueprints/BP_DronePrototypeGameMode.BP_DronePrototypeGameMode_C");
constexpr const TCHAR* EngineSoundPath =
	TEXT("/Game/Drone/ThirdParty/DroneSounds/SC_DroneStandardLoop.SC_DroneStandardLoop");
constexpr const TCHAR* ExplosionEffectPath =
	TEXT("/Game/Drone/ThirdParty/ArmyVFX/Niagara/Destroyed/NS_Expl_Tank_1.NS_Expl_Tank_1");
constexpr const TCHAR* ExplosionSoundPath =
	TEXT("/Game/Drone/ThirdParty/InfantrySFX/Explosions/Cues/Cue_Explosion01_Cue.Cue_Explosion01_Cue");

const TSet<FString> ExpectedMeshPaths = {
	TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_DroneFPVBody.SM_DroneFPVBody"),
	TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_RotorA.SM_RotorA"),
	TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_RotorB.SM_RotorB"),
	TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_RotorC.SM_RotorC"),
	TEXT("/Game/Drone/ThirdParty/DronePackFPV/SM_RotorD.SM_RotorD")
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFPVIntegrationAssetTest,
	"Drone.Integration.FPVAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneFPVIntegrationAssetTest::RunTest(const FString& Parameters)
{
	UClass* PawnClass = LoadClass<ADronePrototypePawn>(nullptr, DroneFPVIntegrationAssets::PawnClassPath);
	TestNotNull(TEXT("BP_DroneFPVIntegration generated Class loads"), PawnClass);
	if (!PawnClass)
	{
		return false;
	}

	TestTrue(
		TEXT("FPV Integration derives from the native Prototype Pawn"),
		PawnClass->IsChildOf(ADronePrototypePawn::StaticClass()));

	const ADronePrototypePawn* PawnDefaults = Cast<ADronePrototypePawn>(PawnClass->GetDefaultObject());
	TestNotNull(TEXT("FPV Integration CDO exists"), PawnDefaults);
	if (!PawnDefaults)
	{
		return false;
	}

	TestTrue(
		TEXT("Native Collision component remains the root"),
		PawnDefaults->GetRootComponent() == PawnDefaults->GetCollisionComponent());
	if (const USphereComponent* Collision = PawnDefaults->GetCollisionComponent())
	{
		TestEqual(TEXT("FPV Integration keeps the Pawn collision profile"), Collision->GetCollisionProfileName(), FName(TEXT("Pawn")));
		TestFalse(TEXT("FPV Integration collision does not simulate physics"), Collision->IsSimulatingPhysics());
	}

	// Blueprint SCS Component는 CDO의 GetComponents()에 나타나지 않으므로 실제 Actor를
	// transient World에 생성해 본체·Rotor·Audio가 런타임에도 인스턴스화되는지 검사한다.
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* TestWorld = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("FPV Integration transient test World exists"), TestWorld);
	if (!TestWorld)
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADronePrototypePawn* SpawnedPawn = TestWorld->SpawnActor<ADronePrototypePawn>(
		PawnClass,
		FTransform::Identity,
		SpawnParameters);
	TestNotNull(TEXT("BP_DroneFPVIntegration spawns in a transient World"), SpawnedPawn);
	if (!SpawnedPawn)
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	TInlineComponentArray<UStaticMeshComponent*> MeshComponents;
	SpawnedPawn->GetComponents(MeshComponents);

	TSet<FString> ActualMeshPaths;
	TSet<int32> OccupiedRotorQuadrants;
	int32 FPVVisualMeshCount = 0;
	for (const UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		TestNotNull(TEXT("FPV visual Mesh component exists"), MeshComponent);
		if (!MeshComponent)
		{
			continue;
		}
		// Editor가 CameraComponent 표시용으로 일시 생성하는 Proxy는 Drone 외형 자산이 아니다.
		if (MeshComponent->IsA<UCameraProxyMeshComponent>())
		{
			continue;
		}
		++FPVVisualMeshCount;

		const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
		TestNotNull(*FString::Printf(TEXT("%s has a Static Mesh"), *MeshComponent->GetName()), Mesh);
		if (Mesh)
		{
			ActualMeshPaths.Add(Mesh->GetPathName());
			if (MeshComponent->ComponentHasTag(TEXT("DroneRotor")))
			{
				const FVector RotorVisualCenter = MeshComponent->GetComponentTransform().TransformPosition(
					Mesh->GetBoundingBox().GetCenter());
				TestTrue(
					*FString::Printf(TEXT("%s visual center is placed away from the FPV body center"), *MeshComponent->GetName()),
					FVector2D(RotorVisualCenter.X, RotorVisualCenter.Y).Size() > 20.0f);
				const int32 Quadrant = (RotorVisualCenter.X >= 0.0f ? 1 : 0)
					| (RotorVisualCenter.Y >= 0.0f ? 2 : 0);
				OccupiedRotorQuadrants.Add(Quadrant);
			}
		}

		TestEqual(
			*FString::Printf(TEXT("%s has no collision"), *MeshComponent->GetName()),
			MeshComponent->GetCollisionEnabled(),
			ECollisionEnabled::NoCollision);
		TestFalse(
			*FString::Printf(TEXT("%s creates no overlaps"), *MeshComponent->GetName()),
			MeshComponent->GetGenerateOverlapEvents());
		TestFalse(
			*FString::Printf(TEXT("%s does not simulate physics"), *MeshComponent->GetName()),
			MeshComponent->IsSimulatingPhysics());
		TestFalse(
			*FString::Printf(TEXT("%s cannot affect Navigation"), *MeshComponent->GetName()),
			MeshComponent->CanEverAffectNavigation());
	}
	TestEqual(TEXT("FPV Integration has one body and four rotor Mesh components"), FPVVisualMeshCount, 5);
	TestTrue(
		TEXT("FPV Integration references exactly the selected body and four rotors"),
		ActualMeshPaths.Num() == DroneFPVIntegrationAssets::ExpectedMeshPaths.Num()
			&& ActualMeshPaths.Includes(DroneFPVIntegrationAssets::ExpectedMeshPaths));
	TestEqual(TEXT("FPV Rotor visuals occupy all four body-arm quadrants"), OccupiedRotorQuadrants.Num(), 4);

	TInlineComponentArray<UAudioComponent*> AudioComponents;
	SpawnedPawn->GetComponents(AudioComponents);
	TestEqual(TEXT("FPV Integration has exactly one engine Audio component"), AudioComponents.Num(), 1);
	if (AudioComponents.Num() == 1 && AudioComponents[0])
	{
		const UAudioComponent* EngineAudio = AudioComponents[0];
		TestNotNull(TEXT("Engine Audio has a Sound"), EngineAudio->Sound.Get());
		if (EngineAudio->Sound)
		{
			TestEqual(
				TEXT("Engine Audio uses the selected standard loop Cue"),
				EngineAudio->Sound->GetPathName(),
				FString(DroneFPVIntegrationAssets::EngineSoundPath));
			TestTrue(TEXT("Selected engine Cue is configured to loop"), EngineAudio->Sound->IsLooping());
		}
		TestTrue(TEXT("Engine Audio activates with the Pawn"), EngineAudio->bAutoActivate);
	}

	const UDroneImpactDetonationComponent* Impact = SpawnedPawn->GetImpactDetonationComponent();
	TestNotNull(TEXT("FPV integration has its impact detonation component"), Impact);
	if (Impact)
	{
		TestNotNull(TEXT("FPV detonation has a visible Niagara effect"), Impact->GetExplosionEffect());
		TestNotNull(TEXT("FPV detonation has an explosion sound"), Impact->GetExplosionSound());
		TestEqual(TEXT("FPV detonation uses the selected provided Niagara effect"),
			Impact->GetExplosionEffect() ? Impact->GetExplosionEffect()->GetPathName() : FString(),
			FString(DroneFPVIntegrationAssets::ExplosionEffectPath));
		TestEqual(TEXT("FPV detonation uses the selected provided explosion Cue"),
			Impact->GetExplosionSound() ? Impact->GetExplosionSound()->GetPathName() : FString(),
			FString(DroneFPVIntegrationAssets::ExplosionSoundPath));
	}

	UClass* GameModeClass = LoadClass<ADronePrototypeGameMode>(nullptr, DroneFPVIntegrationAssets::GameModeClassPath);
	TestNotNull(TEXT("BP_DronePrototypeGameMode generated Class loads"), GameModeClass);
	if (GameModeClass)
	{
		const ADronePrototypeGameMode* GameModeDefaults =
			Cast<ADronePrototypeGameMode>(GameModeClass->GetDefaultObject());
		TestNotNull(TEXT("Prototype BP GameMode CDO exists"), GameModeDefaults);
		if (GameModeDefaults)
		{
			TestTrue(
				TEXT("Prototype BP GameMode spawns BP_DroneFPVIntegration"),
				GameModeDefaults->DefaultPawnClass == PawnClass);
		}
	}

	WorldWrapper.TickTestWorld();
	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif
