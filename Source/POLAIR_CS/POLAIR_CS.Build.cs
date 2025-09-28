// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class POLAIR_CS : ModuleRules
{
	public POLAIR_CS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"HeadMountedDisplay",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities",
			"HTTP",
			"PlayFabGSDK",
			"NetCore",
			"DeveloperSettings", // Required for PACS Selection System settings
			"AIModule", // Required for AIController and UAIBlueprintHelperLibrary
			"NavigationSystem", // Required for AI pathfinding
			"ReplicationGraph", // Required for PACS_ReplicationGraph optimization
			"SignificanceManager", // Required for PACS_SignificanceManager
			"GameplayTags", // Required for spawn system gameplay tags
			"ChaosVehicles", // Required for vehicle base class
			"Niagara", // Required for UNiagaraComponent in NPC base class
			"WorldDirectorPRO",
            "NPC_Optimizator"

        });

		PrivateDependencyModuleNames.AddRange(new string[] {
			"PlayFab",
			"PlayFabCpp",
			"PlayFabCommon",
			"XRBase", // Required for UHeadMountedDisplayFunctionLibrary
			"Settings", // Required for ISettingsModule registration
			"RenderCore", // Required for threading globals (GIsThreadedRendering, etc.)
			"RHI", // Required for RHI diagnostics
			"AnimationBudgetAllocator" // Required for animation optimization
		});

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "AutomationTest" });
		}
		
		// Add UnrealEd only for Editor targets
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
