#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "PACS_NetworkMonitorLibrary.generated.h"

// Forward declarations
struct FSpawnNetworkStats;

/**
 * Blueprint function library for accessing UPACS_NetworkMonitorSubsystem.
 *
 * Epic Pattern: Separates blueprint exposure from subsystem implementation
 * to prevent editor startup assertion failures with server-only WorldSubsystems.
 *
 * @see UReplicationGraphBlueprintLibrary for Epic's pattern reference
 */
UCLASS()
class POLAIR_CS_API UPACS_NetworkMonitorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Queue a spawn request for network batching. Server-only operation.
	 * @param WorldContextObject - World context for subsystem lookup
	 * @param SpawnTag - Gameplay tag identifying spawn type
	 * @param Transform - Spawn transform
	 */
	UFUNCTION(BlueprintCallable, Category = "POLAIR|Network",
	          meta = (WorldContext = "WorldContextObject"))
	static void QueueSpawnRequest(UObject* WorldContextObject,
	                              FGameplayTag SpawnTag,
	                              const FTransform& Transform);

	/**
	 * Force immediate processing of all batched spawns. Server-only.
	 * @param WorldContextObject - World context for subsystem lookup
	 */
	UFUNCTION(BlueprintCallable, Category = "POLAIR|Network",
	          meta = (WorldContext = "WorldContextObject"))
	static void FlushSpawnBatch(UObject* WorldContextObject);

	/**
	 * Get current network bandwidth usage in KB/s.
	 * @param WorldContextObject - World context for subsystem lookup
	 * @return Current bandwidth usage, or 0.0 if subsystem not available
	 */
	UFUNCTION(BlueprintPure, Category = "POLAIR|Network",
	          meta = (WorldContext = "WorldContextObject"))
	static float GetCurrentBandwidth(UObject* WorldContextObject);

	/**
	 * Get peak network bandwidth recorded during this session.
	 * @param WorldContextObject - World context for subsystem lookup
	 * @return Peak bandwidth in KB/s, or 0.0 if subsystem not available
	 */
	UFUNCTION(BlueprintPure, Category = "POLAIR|Network",
	          meta = (WorldContext = "WorldContextObject"))
	static float GetPeakBandwidth(UObject* WorldContextObject);

	/**
	 * Check if current bandwidth exceeds the configured limit.
	 * @param WorldContextObject - World context for subsystem lookup
	 * @return True if over bandwidth limit
	 */
	UFUNCTION(BlueprintPure, Category = "POLAIR|Network",
	          meta = (WorldContext = "WorldContextObject"))
	static bool IsOverBandwidthLimit(UObject* WorldContextObject);

	/**
	 * Get network statistics for a specific spawn type.
	 * @param WorldContextObject - World context for subsystem lookup
	 * @param SpawnTag - Gameplay tag to query
	 * @return Network stats structure (zeroed if not available)
	 */
	UFUNCTION(BlueprintPure, Category = "POLAIR|Network",
	          meta = (WorldContext = "WorldContextObject"))
	static FSpawnNetworkStats GetSpawnNetworkStats(UObject* WorldContextObject, FGameplayTag SpawnTag);

	/**
	 * Enable or disable spawn batching for network optimization.
	 * @param WorldContextObject - World context for subsystem lookup
	 * @param bEnable - True to enable batching, false to disable
	 */
	UFUNCTION(BlueprintCallable, Category = "POLAIR|Network",
	          meta = (WorldContext = "WorldContextObject"))
	static void SetBatchingEnabled(UObject* WorldContextObject, bool bEnable);

	/**
	 * Set the bandwidth limit in KB/s per client.
	 * @param WorldContextObject - World context for subsystem lookup
	 * @param LimitKBps - New bandwidth limit
	 */
	UFUNCTION(BlueprintCallable, Category = "POLAIR|Network",
	          meta = (WorldContext = "WorldContextObject"))
	static void SetBandwidthLimit(UObject* WorldContextObject, float LimitKBps);

private:
	// Helper to get subsystem with null-safety
	static class UPACS_NetworkMonitorSubsystem* GetSubsystem(UObject* WorldContextObject);
};