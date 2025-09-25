// Copyright Notice: Property of Particular Audience Limited

#include "Data/Settings/PACS_CustomSignificanceManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UPACS_CustomSignificanceManager::UPACS_CustomSignificanceManager()
{
    // CRITICAL: Enable creation on client instances
    // This is required for client-side optimization systems to work
    bCreateOnClient = true;

    // Disable on dedicated servers as they don't need visual optimizations
    bCreateOnServer = false;

    // Note: SignificanceManagerClassName is private in UE5 and set by the module
    // The DefaultEngine.ini configuration will handle the class reference

    // Log the configuration
    UE_LOG(LogTemp, Log, TEXT("PACS_CustomSignificanceManager: Configured for client-side creation"));
}

void UPACS_CustomSignificanceManager::BeginDestroy()
{
    UE_LOG(LogTemp, Verbose, TEXT("PACS_CustomSignificanceManager: Destroying significance manager"));
    Super::BeginDestroy();
}