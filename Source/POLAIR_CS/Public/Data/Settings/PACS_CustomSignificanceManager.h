// Copyright Notice: Property of Particular Audience Limited

#pragma once

#include "CoreMinimal.h"
#include "SignificanceManager.h"
#include "PACS_CustomSignificanceManager.generated.h"

/**
 * Custom SignificanceManager for PACS that ensures proper creation on clients
 * This class configures the SignificanceManager to be created on client instances
 * which is required for our client-side optimization systems to work properly
 *
 * This is NOT the game logic manager - that's PACS_SignificanceManager.
 * This class only configures the ENGINE's SignificanceManager system.
 */
UCLASS(config=Engine)
class POLAIR_CS_API UPACS_CustomSignificanceManager : public USignificanceManager
{
    GENERATED_BODY()

public:
    UPACS_CustomSignificanceManager();

    // Override to provide custom initialization if needed
    virtual void BeginDestroy() override;

    // Optional: Override to customize update frequency
    // virtual void Update(const FTransform& ViewTransform) override;

protected:
    // Configuration properties that could be exposed if needed:

    // Maximum number of objects to process per frame
    // UPROPERTY(Config, EditAnywhere, Category = "Significance")
    // int32 MaxObjectsToProcessPerFrame = 100;

    // Update frequency in seconds
    // UPROPERTY(Config, EditAnywhere, Category = "Significance")
    // float UpdateFrequency = 0.1f;

    // Distance thresholds for quick rejection
    // UPROPERTY(Config, EditAnywhere, Category = "Significance")
    // float MaximumDistance = 15000.0f;
};