// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * POLAIR_CS Game Module
 * Handles startup and shutdown for the POLAIR_CS training simulation
 * Registers PACS Selection System settings
 */
class FPOLAIR_CSModule : public IModuleInterface
{
public:
	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	// Settings registration management
	void RegisterSelectionSystemSettings();
	void UnregisterSelectionSystemSettings();
};

