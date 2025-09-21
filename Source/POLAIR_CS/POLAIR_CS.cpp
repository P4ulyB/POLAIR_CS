// Copyright Epic Games, Inc. All Rights Reserved.

#include "POLAIR_CS.h"
#include "Settings/PACS_SelectionSystemSettings.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "POLAIR_CSModule"

void FPOLAIR_CSModule::StartupModule()
{
	// Register PACS Selection System settings
	RegisterSelectionSystemSettings();
}

void FPOLAIR_CSModule::ShutdownModule()
{
	// Unregister settings on shutdown
	UnregisterSelectionSystemSettings();
}

void FPOLAIR_CSModule::RegisterSelectionSystemSettings()
{
	// Epic pattern: Register settings with ISettingsModule (from GameplayTagsModule)
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "PACS", "SelectionSystem",
			LOCTEXT("SelectionSystemSettingsName", "Selection System"),
			LOCTEXT("SelectionSystemSettingsDescription", "Configure global selection materials and parameters for PACS characters"),
			GetMutableDefault<UPACS_SelectionSystemSettings>()
		);
	}
}

void FPOLAIR_CSModule::UnregisterSelectionSystemSettings()
{
	// Epic pattern: Cleanup settings registration on shutdown
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "PACS", "SelectionSystem");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_PRIMARY_GAME_MODULE(FPOLAIR_CSModule, POLAIR_CS, "POLAIR_CS");
