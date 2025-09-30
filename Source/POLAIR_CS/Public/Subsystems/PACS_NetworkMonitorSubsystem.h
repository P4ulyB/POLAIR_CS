#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "PACS_NetworkMonitorSubsystem.generated.h"

class AActor;

/**
 * Network bandwidth statistics for a spawn type
 */
USTRUCT(BlueprintType)
struct FSpawnNetworkStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SpawnMessagesSent = 0;

	UPROPERTY(BlueprintReadOnly)
	float TotalBytesSent = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float BytesPerSecond = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float PeakBytesPerSecond = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float AverageBytesPerSpawn = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FDateTime LastMeasured;

	void Reset()
	{
		SpawnMessagesSent = 0;
		TotalBytesSent = 0.0f;
		BytesPerSecond = 0.0f;
		PeakBytesPerSecond = 0.0f;
		AverageBytesPerSpawn = 0.0f;
	}
};

/**
 * Batched spawn request for network optimization
 */
USTRUCT(BlueprintType)
struct FBatchedSpawnRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag SpawnTag;

	UPROPERTY(BlueprintReadWrite)
	TArray<FTransform> SpawnTransforms;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> SpawnedActors;

	UPROPERTY(BlueprintReadWrite)
	float RequestTime = 0.0f;

	int32 GetCount() const { return SpawnTransforms.Num(); }
};

/**
 * Network monitoring subsystem for spawn system
 * Tracks bandwidth usage to ensure compliance with 100KB/s per client target
 * Server-only subsystem (does not exist on clients)
 *
 * Epic Pattern: Pure C++ API, no BlueprintCallable to avoid editor startup assertions.
 * Blueprint access provided through UPACS_NetworkMonitorLibrary.
 */
UCLASS()
class POLAIR_CS_API UPACS_NetworkMonitorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// C++ API - NO BlueprintCallable to prevent editor startup assertion failures
	void QueueSpawnRequest(const FGameplayTag& SpawnTag, const FTransform& Transform);
	void FlushSpawnBatch();
	bool ShouldBatchSpawns() const { return bEnableBatching; }

	// Network tracking
	void RecordSpawnMessage(const FGameplayTag& SpawnTag, int32 MessageSizeBytes);
	void RecordActorReplication(AActor* Actor, int32 BytesReplicated);

	// Bandwidth queries - C++ API only
	float GetCurrentBandwidthKBps() const { return CurrentBandwidthKBps; }
	float GetPeakBandwidthKBps() const { return PeakBandwidthKBps; }
	bool IsBandwidthExceeded() const { return CurrentBandwidthKBps > BandwidthLimitKBps; }
	FSpawnNetworkStats GetSpawnNetworkStats(const FGameplayTag& SpawnTag) const;

	// Throttling
	void SetBandwidthLimit(float LimitKBps) { BandwidthLimitKBps = LimitKBps; }
	bool ShouldThrottleSpawns() const;
	float GetThrottleDelaySeconds() const;

	// Optimization features
	void EnableBatching(bool bEnable) { bEnableBatching = bEnable; }
	void SetBatchWindow(float WindowSeconds) { BatchWindowSeconds = WindowSeconds; }

	// Performance alerts
	void CheckBandwidthCompliance(float TargetKBps = 100.0f);

protected:
	// Tick function called by timer
	void TickMonitor();

	// Batch processing
	void ProcessPendingBatches();
	void ExecuteBatch(const FBatchedSpawnRequest& Batch);
	int32 EstimateBatchSize(const FBatchedSpawnRequest& Batch) const;

	// Bandwidth calculation
	void UpdateBandwidthMetrics(float DeltaTime);
	void OnBandwidthWarning(float CurrentKBps, float LimitKBps);
	void OnBandwidthCritical(float CurrentKBps, float LimitKBps);

private:
	// Batching settings (runtime-only, no EditAnywhere for WorldSubsystems)
	bool bEnableBatching = true;
	float BatchWindowSeconds = 0.1f; // Batch spawns within 100ms window
	int32 MaxBatchSize = 20; // Maximum spawns per batch

	// Bandwidth settings
	float BandwidthLimitKBps = 100.0f; // 100KB/s per client target
	float BandwidthWarningThreshold = 0.8f; // Warn at 80% usage
	float BandwidthCriticalThreshold = 0.95f; // Critical at 95% usage

	// Throttling settings
	bool bEnableThrottling = true;
	float MinThrottleDelay = 0.05f; // Minimum 50ms between spawns when throttling
	float MaxThrottleDelay = 1.0f; // Maximum 1s between spawns when throttling

	// Runtime state - Transient UPROPERTYs for GC tracking and serialization safety
	UPROPERTY(Transient)
	TMap<FGameplayTag, FBatchedSpawnRequest> PendingBatches;

	UPROPERTY(Transient)
	TMap<FGameplayTag, FSpawnNetworkStats> SpawnStats;

	// Bandwidth tracking (primitives safe without UPROPERTY)
	float CurrentBandwidthKBps = 0.0f;
	float PeakBandwidthKBps = 0.0f;
	float BytesSentThisSecond = 0.0f;
	float TimeSinceLastMeasure = 0.0f;

	// Batch timing
	float TimeSinceLastBatch = 0.0f;
	float LastSpawnTime = 0.0f;

	// Ring buffer for bandwidth smoothing (last 10 seconds)
	TArray<float> BandwidthHistory;
	int32 HistoryIndex = 0;
	static constexpr int32 HistorySize = 10;

	// Timer for ticking - MUST be Transient UPROPERTY to avoid serialization issues
	UPROPERTY(Transient)
	FTimerHandle TickTimerHandle;
	float LastTickTime = 0.0f;
};