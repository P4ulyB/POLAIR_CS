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
			"POLAIR_CS"
		});

		PrivateDependencyModuleNames.AddRange(new string[] 
		{ 
			"EditorStyle", 
			"EditorWidgets", 
			"Slate", 
			"SlateCore",
			"AutomationTest"
		});
	}
}