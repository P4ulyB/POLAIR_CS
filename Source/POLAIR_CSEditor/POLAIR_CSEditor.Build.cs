// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class POLAIR_CSEditor : ModuleRules
{
	public POLAIR_CSEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] 
		{ 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"UnrealEd",
			"ToolMenus",
			"POLAIR_CS",
			"InputCore",
			"EnhancedInput"
		});

		PrivateDependencyModuleNames.AddRange(new string[] 
		{ 
			"EditorStyle", 
			"EditorWidgets", 
			"Slate", 
			"SlateCore",
			"AutomationTest"
		});

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"AutomationController",
				"FunctionalTesting"
			});
		}
	}
}