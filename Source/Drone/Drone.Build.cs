// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Drone : ModuleRules
{
	public Drone(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"GameplayTags",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"SmartObjectsModule",
			"GameplayInteractionsModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		if (Target.bBuildEditor)
		{
			// Editor 자동화 테스트와 프로젝트 소유 StateTree 작성 도구에서만 사용하는 모듈이다.
			// Runtime/Game 빌드에는 Editor 전용 의존성을 포함하지 않는다.
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"AssetRegistry",
				"AnimGraph",
				"BlueprintEditorLibrary",
				"BlueprintGraph",
				"Kismet",
				"PropertyBindingUtils",
				"StateTreeEditorModule"
			});
		}

		PublicIncludePaths.AddRange(new string[] {
			"Drone",
			"Drone/Variant_Platforming",
			"Drone/Variant_Platforming/Animation",
			"Drone/Variant_Combat",
			"Drone/Variant_Combat/AI",
			"Drone/Variant_Combat/Animation",
			"Drone/Variant_Combat/Gameplay",
			"Drone/Variant_Combat/Interfaces",
			"Drone/Variant_Combat/UI",
			"Drone/Variant_SideScrolling",
			"Drone/Variant_SideScrolling/AI",
			"Drone/Variant_SideScrolling/Gameplay",
			"Drone/Variant_SideScrolling/Interfaces",
			"Drone/Variant_SideScrolling/UI"
		});

		// Drone Flight HUD가 UMG와 native Slate 글꼴/색상 타입을 사용하므로
		// UMG, Slate, SlateCore는 위 PublicDependencyModuleNames에 이미 포함되어 있다.

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
