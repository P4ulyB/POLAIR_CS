// Copyright Epic Games, Inc. All Rights Reserved.

#include "POLAIR_CS.h"
#include "Modules/ModuleManager.h"

void FPOLAIR_CSModule::StartupModule()
{
	// UDeveloperSettings automatically registers itself, no manual registration needed
}

void FPOLAIR_CSModule::ShutdownModule()
{
	// UDeveloperSettings handles its own cleanup
}

IMPLEMENT_PRIMARY_GAME_MODULE(FPOLAIR_CSModule, POLAIR_CS, "POLAIR_CS");
