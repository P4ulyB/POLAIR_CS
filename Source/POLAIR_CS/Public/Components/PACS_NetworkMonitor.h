#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PACS_NetworkMonitor.generated.h"

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
 * Network monitoring component for spawn system
 * Tracks bandwidth usage to ensure compliance with 100KB/s per client target
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POLAIR_CS_API UPACS_NetworkMonitor : public UActorComponent
{
	GENERATED_BODY()

public:
	UPACS_NetworkMonitor();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Spawn batching
	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	void QueueSpawnRequest(FGameplayTag SpawnTag, const FTransform& Transform);

	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	void FlushSpawnBatch();

	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	bool ShouldBatchSpawns() const { return bEnableBatching; }

	// Network tracking
	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	void RecordSpawnMessage(FGameplayTag SpawnTag, int32 MessageSizeBytes);

	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	void RecordActorReplication(AActor* Actor, int32 BytesReplicated);

	// Bandwidth queries
	UFUNCTION(BlueprintPure, Category = "Network Monitor")
	float GetCurrentBandwidthKBps() const { return CurrentBandwidthKBps; }

	UFUNCTION(BlueprintPure, Category = "Network Monitor")
	float GetPeakBandwidthKBps() const { return PeakBandwidthKBps; }

	UFUNCTION(BlueprintPure, Category = "Network Monitor")
	bool IsBandwidthExceeded() const { return CurrentBandwidthKBps > BandwidthLimitKBps; }

	UFUNCTION(BlueprintPure, Category = "Network Monitor")
	FSpawnNetworkStats GetSpawnNetworkStats(FGameplayTag SpawnTag) const;

	// Throttling
	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	void SetBandwidthLimit(float LimitKBps) { BandwidthLimitKBps = LimitKBps; }

	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	bool ShouldThrottleSpawns() const;

	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	float GetThrottleDelaySeconds() const;

	// Optimization features
	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	void EnableBatching(bool bEnable) { bEnableBatching = bEnable; }

	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	void SetBatchWindow(float WindowSeconds) { BatchWindowSeconds = WindowSeconds; }

	// Performance alerts
	UFUNCTION(BlueprintCallable, Category = "Network Monitor")
	void CheckBandwidthCompliance(float TargetKBps = 100.0f);

protected:
	// Batch processing
	void ProcessPendingBatches();
	void ExecuteBatch(const FBatchedSpawnRequest& Batch);
	int32 EstimateBatchSize(const FBatchedSpawnRequest& Batch) const;

	// Bandwidth calculation
	void UpdateBandwidthMetrics(float DeltaTime);
	void OnBandwidthWarning(float CurrentKBps, float LimitKBps);
	void OnBandwidthCritical(float CurrentKBps, float LimitKBps);

	// Multicast RPC for batched spawns
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBatchedSpawn(FGameplayTag SpawnTag, const TArray<FTransform>& Transforms);

private:
	// Batching settings
	UPROPERTY(EditAnywhere, Category = "Batching")
	bool bEnableBatching = true;

	UPROPERTY(EditAnywhere, Category = "Batching", meta = (ClampMin = 0.01, ClampMax = 1.0))
	float BatchWindowSeconds = 0.1f; // Batch spawns within 100ms window

	UPROPERTY(EditAnywhere, Category = "Batching", meta = (ClampMin = 1, ClampMax = 50))
	int32 MaxBatchSize = 20; // Maximum spawns per batch

	// Bandwidth settings
	UPROPERTY(EditAnywhere, Category = "Bandwidth")
	float BandwidthLimitKBps = 100.0f; // 100KB/s per client target

	UPROPERTY(EditAnywhere, Category = "Bandwidth")
	float BandwidthWarningThreshold = 0.8f; // Warn at 80% usage

	UPROPERTY(EditAnywhere, Category = "Bandwidth")
	float BandwidthCriticalThreshold = 0.95f; // Critical at 95% usage

	// Throttling settings
	UPROPERTY(EditAnywhere, Category = "Throttling")
	bool bEnableThrottling = true;

	UPROPERTY(EditAnywhere, Category = "Throttling", meta = (ClampMin = 0.01, ClampMax = 1.0))
	float MinThrottleDelay = 0.05f; // Minimum 50ms between spawns when throttling

	UPROPERTY(EditAnywhere, Category = "Throttling", meta = (ClampMin = 0.1, ClampMax = 5.0))
	float MaxThrottleDelay = 1.0f; // Maximum 1s between spawns when throttling

	// Runtime state
	UPROPERTY()
	TMap<FGameplayTag, FBatchedSpawnRequest> PendingBatches;

	UPROPERTY()
	TMap<FGameplayTag, FSpawnNetworkStats> SpawnStats;

	// Bandwidth tracking
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
};