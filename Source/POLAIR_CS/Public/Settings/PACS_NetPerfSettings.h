#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PACS_NetPerfSettings.generated.h"

/**
 * Developer settings for PACS Network and Performance configuration
 * Accessible via Project Settings -> PACS -> NetPerf
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="PACS - NetPerf"))
class POLAIR_CS_API UPACS_NetPerfSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPACS_NetPerfSettings();

    // --- Selection Visual Settings ---

    UPROPERTY(config, EditAnywhere, Category="Selection|Visual",
        meta=(DisplayName="Enable Selection Planes",
        ToolTip="Enable/disable selection plane indicators for NPCs"))
    bool bEnableSelectionPlanes = true;

    UPROPERTY(config, EditAnywhere, Category="Selection|Visual",
        meta=(DisplayName="Default Plane Scale", ClampMin=0.5, ClampMax=3.0,
        ToolTip="Default scale multiplier for selection planes relative to actor bounds"))
    float DefaultSelectionPlaneScale = 1.5f;

    UPROPERTY(config, EditAnywhere, Category="Selection|Visual",
        meta=(DisplayName="Plane Z Offset", ClampMin=0.0, ClampMax=10.0,
        ToolTip="Z-offset from ground to prevent z-fighting (in cm)"))
    float SelectionPlaneZOffset = 2.0f;

    UPROPERTY(config, EditAnywhere, Category="Selection|Visual",
        meta=(DisplayName="Hide Available State",
        ToolTip="If true, selection planes are hidden when NPCs are in Available state"))
    bool bHideSelectionWhenAvailable = true;

    // --- Significance Settings ---

    UPROPERTY(config, EditAnywhere, Category="Significance|VR",
        meta=(DisplayName="VR Cull Distance", ClampMin=1000.0, ClampMax=20000.0,
        ToolTip="Distance at which NPCs are culled in VR (in cm)"))
    float VRCullDistance = 5000.0f;

    UPROPERTY(config, EditAnywhere, Category="Significance|VR",
        meta=(DisplayName="VR Max LOD", ClampMin=0, ClampMax=5,
        ToolTip="Maximum LOD level for NPCs in VR"))
    int32 VRMaxLOD = 2;

    UPROPERTY(config, EditAnywhere, Category="Significance|Selection",
        meta=(DisplayName="Max Visible Selection Planes", ClampMin=1, ClampMax=100,
        ToolTip="Maximum number of selection planes visible at once"))
    int32 MaxVisibleSelectionPlanes = 32;

    UPROPERTY(config, EditAnywhere, Category="Significance|Selection",
        meta=(DisplayName="Selection Plane Max Distance", ClampMin=1000.0, ClampMax=50000.0,
        ToolTip="Maximum distance at which selection planes are visible (in cm)"))
    float SelectionPlaneMaxDistance = 15000.0f;

    // --- Network Optimization ---

    UPROPERTY(config, EditAnywhere, Category="Network|Replication",
        meta=(DisplayName="Selection State Rep Rate", ClampMin=1.0, ClampMax=60.0,
        ToolTip="Replication rate for selection state changes (per second)"))
    float SelectionStateReplicationRate = 10.0f;

    UPROPERTY(config, EditAnywhere, Category="Network|Replication",
        meta=(DisplayName="Use Reliable Selection RPCs",
        ToolTip="If true, selection state changes use reliable RPCs"))
    bool bUseReliableSelectionRPCs = true;

    UPROPERTY(config, EditAnywhere, Category="Network|Batching",
        meta=(DisplayName="Selection Batch Size", ClampMin=1, ClampMax=50,
        ToolTip="Number of selection updates to batch together"))
    int32 SelectionBatchSize = 20;

    UPROPERTY(config, EditAnywhere, Category="Network|Batching",
        meta=(DisplayName="Batch Window Time", ClampMin=0.01, ClampMax=1.0,
        ToolTip="Time window for batching selection updates (in seconds)"))
    float SelectionBatchWindowTime = 0.1f;

    // --- Performance Monitoring ---

    UPROPERTY(config, EditAnywhere, Category="Performance|Monitoring",
        meta=(DisplayName="Enable Performance Logging",
        ToolTip="Enable detailed performance logging for selection system"))
    bool bEnableSelectionPerformanceLogging = false;

    UPROPERTY(config, EditAnywhere, Category="Performance|Monitoring",
        meta=(DisplayName="Log Interval", ClampMin=1.0, ClampMax=60.0,
        ToolTip="Interval for performance logging (in seconds)"))
    float PerformanceLogInterval = 5.0f;

    UPROPERTY(config, EditAnywhere, Category="Performance|Memory",
        meta=(DisplayName="Memory Warning Threshold", ClampMin=10, ClampMax=500,
        ToolTip="Memory usage threshold for warnings (in MB)"))
    int32 MemoryWarningThresholdMB = 80;

    UPROPERTY(config, EditAnywhere, Category="Performance|Memory",
        meta=(DisplayName="Memory Critical Threshold", ClampMin=10, ClampMax=500,
        ToolTip="Memory usage threshold for critical alerts (in MB)"))
    int32 MemoryCriticalThresholdMB = 100;

    // --- Optimization Features ---

    UPROPERTY(config, EditAnywhere, Category="Optimization|Pooling",
        meta=(DisplayName="Pool Selection Components",
        ToolTip="If true, selection plane components are pooled for reuse"))
    bool bPoolSelectionComponents = true;

    UPROPERTY(config, EditAnywhere, Category="Optimization|Pooling",
        meta=(DisplayName="Selection Pool Initial Size", ClampMin=0, ClampMax=100,
        ToolTip="Initial size of selection component pool"))
    int32 SelectionPoolInitialSize = 10;

    UPROPERTY(config, EditAnywhere, Category="Optimization|Pooling",
        meta=(DisplayName="Selection Pool Max Size", ClampMin=1, ClampMax=500,
        ToolTip="Maximum size of selection component pool"))
    int32 SelectionPoolMaxSize = 100;

    UPROPERTY(config, EditAnywhere, Category="Optimization|Updates",
        meta=(DisplayName="Throttle Selection Updates",
        ToolTip="If true, selection visual updates are throttled based on significance"))
    bool bThrottleSelectionUpdates = true;

    UPROPERTY(config, EditAnywhere, Category="Optimization|Updates",
        meta=(DisplayName="Near Update Rate", ClampMin=10.0, ClampMax=120.0,
        ToolTip="Update rate for nearby selection planes (per second)"))
    float NearSelectionUpdateRate = 60.0f;

    UPROPERTY(config, EditAnywhere, Category="Optimization|Updates",
        meta=(DisplayName="Far Update Rate", ClampMin=1.0, ClampMax=60.0,
        ToolTip="Update rate for distant selection planes (per second)"))
    float FarSelectionUpdateRate = 10.0f;

    UPROPERTY(config, EditAnywhere, Category="Optimization|Updates",
        meta=(DisplayName="Near Distance Threshold", ClampMin=100.0, ClampMax=10000.0,
        ToolTip="Distance threshold for near/far update rates (in cm)"))
    float NearDistanceThreshold = 2000.0f;

    // --- Debug Settings ---

    UPROPERTY(config, EditAnywhere, Category="Debug",
        meta=(DisplayName="Show Selection Debug Info",
        ToolTip="Display debug information for selection system"))
    bool bShowSelectionDebugInfo = false;

    UPROPERTY(config, EditAnywhere, Category="Debug",
        meta=(DisplayName="Debug Selection Color",
        ToolTip="Color used for debug visualization"))
    FLinearColor DebugSelectionColor = FLinearColor::Yellow;

    UPROPERTY(config, EditAnywhere, Category="Debug",
        meta=(DisplayName="Log Selection Events",
        ToolTip="Log all selection state changes"))
    bool bLogSelectionEvents = false;

public:
    // Get singleton instance
    static const UPACS_NetPerfSettings* Get()
    {
        return GetDefault<UPACS_NetPerfSettings>();
    }

    // Validate settings
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    // Get category name for settings
    virtual FName GetCategoryName() const override { return TEXT("PACS"); }
};