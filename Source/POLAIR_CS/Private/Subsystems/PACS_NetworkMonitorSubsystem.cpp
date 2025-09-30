#include "Subsystems/PACS_NetworkMonitorSubsystem.h"
#include "Subsystems/PACS_SpawnOrchestrator.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Net/UnrealNetwork.h"

DECLARE_STATS_GROUP(TEXT("PACS_NetworkMonitorSubsystem"), STATGROUP_PACSNetworkMonitor, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("NetworkMonitorSubsystem Tick"), STAT_PACSNetworkMonitor_Tick, STATGROUP_PACSNetworkMonitor);

void UPACS_NetworkMonitorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Initialize bandwidth history
	BandwidthHistory.SetNum(HistorySize);
	for (int32 i = 0; i < HistorySize; i++)
	{
		BandwidthHistory[i] = 0.0f;
	}

	// Set up timer for ticking (every frame at ~60fps = 0.016s)
	LastTickTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(
		TickTimerHandle,
		this,
		&UPACS_NetworkMonitorSubsystem::TickMonitor,
		0.016f,
		true // Loop
	);

	UE_LOG(LogTemp, Log, TEXT("PACS_NetworkMonitorSubsystem: Initialized with %.1f KB/s bandwidth limit for World %s"),
		BandwidthLimitKBps, *GetWorld()->GetName());
}

void UPACS_NetworkMonitorSubsystem::Deinitialize()
{
	// CRITICAL: Clear timer BEFORE Super::Deinitialize()
	if (UWorld* World = GetWorld())
	{
		if (World->GetTimerManager().IsTimerActive(TickTimerHandle))
		{
			World->GetTimerManager().ClearTimer(TickTimerHandle);
		}
	}

	// Clear all transient data to prevent dangling references
	PendingBatches.Reset();
	SpawnStats.Reset();
	BandwidthHistory.Reset();

	Super::Deinitialize();
}

bool UPACS_NetworkMonitorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create on server in networked games, or always in standalone
	UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}

	// Create for dedicated servers, listen servers, and standalone games
	return World->GetNetMode() != NM_Client;
}

void UPACS_NetworkMonitorSubsystem::TickMonitor()
{
	SCOPE_CYCLE_COUNTER(STAT_PACSNetworkMonitor_Tick);

	// Calculate delta time manually
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float DeltaTime = CurrentTime - LastTickTime;
	LastTickTime = CurrentTime;

	TimeSinceLastBatch += DeltaTime;

	// Process pending batches if window exceeded
	if (TimeSinceLastBatch >= BatchWindowSeconds)
	{
		ProcessPendingBatches();
		TimeSinceLastBatch = 0.0f;
	}

	// Update bandwidth metrics
	UpdateBandwidthMetrics(DeltaTime);
}

void UPACS_NetworkMonitorSubsystem::QueueSpawnRequest(const FGameplayTag& SpawnTag, const FTransform& Transform)
{
	if (!bEnableBatching)
	{
		// Execute immediately if batching disabled
		FBatchedSpawnRequest SingleRequest;
		SingleRequest.SpawnTag = SpawnTag;
		SingleRequest.SpawnTransforms.Add(Transform);
		SingleRequest.RequestTime = GetWorld()->GetTimeSeconds();
		ExecuteBatch(SingleRequest);
		return;
	}

	// Add to pending batch
	FBatchedSpawnRequest& Batch = PendingBatches.FindOrAdd(SpawnTag);
	Batch.SpawnTag = SpawnTag;
	Batch.SpawnTransforms.Add(Transform);

	if (Batch.RequestTime == 0.0f)
	{
		Batch.RequestTime = GetWorld()->GetTimeSeconds();
	}

	// Execute if batch is full
	if (Batch.GetCount() >= MaxBatchSize)
	{
		ExecuteBatch(Batch);
		PendingBatches.Remove(SpawnTag);
	}

	UE_LOG(LogTemp, Verbose, TEXT("PACS_NetworkMonitor: Queued spawn for tag %s (batch size: %d)"),
		*SpawnTag.ToString(), Batch.GetCount());
}

void UPACS_NetworkMonitorSubsystem::FlushSpawnBatch()
{
	ProcessPendingBatches();
}

void UPACS_NetworkMonitorSubsystem::RecordSpawnMessage(const FGameplayTag& SpawnTag, int32 MessageSizeBytes)
{
	FSpawnNetworkStats& Stats = SpawnStats.FindOrAdd(SpawnTag);
	Stats.SpawnMessagesSent++;
	Stats.TotalBytesSent += MessageSizeBytes;
	Stats.AverageBytesPerSpawn = Stats.TotalBytesSent / Stats.SpawnMessagesSent;
	Stats.LastMeasured = FDateTime::Now();

	// Update bandwidth tracking
	BytesSentThisSecond += MessageSizeBytes;

	UE_LOG(LogTemp, Verbose, TEXT("PACS_NetworkMonitor: Recorded %d bytes for spawn tag %s"),
		MessageSizeBytes, *SpawnTag.ToString());
}

void UPACS_NetworkMonitorSubsystem::RecordActorReplication(AActor* Actor, int32 BytesReplicated)
{
	if (!Actor)
	{
		return;
	}

	// Update bandwidth tracking
	BytesSentThisSecond += BytesReplicated;

	// Find spawn tag for this actor if tracked
	if (UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>())
	{
		// Note: Would need to add a method to get tag for actor
	}
}

FSpawnNetworkStats UPACS_NetworkMonitorSubsystem::GetSpawnNetworkStats(const FGameplayTag& SpawnTag) const
{
	if (const FSpawnNetworkStats* Stats = SpawnStats.Find(SpawnTag))
	{
		return *Stats;
	}

	return FSpawnNetworkStats();
}

bool UPACS_NetworkMonitorSubsystem::ShouldThrottleSpawns() const
{
	if (!bEnableThrottling)
	{
		return false;
	}

	// Throttle if bandwidth exceeded
	return CurrentBandwidthKBps > (BandwidthLimitKBps * BandwidthWarningThreshold);
}

float UPACS_NetworkMonitorSubsystem::GetThrottleDelaySeconds() const
{
	if (!ShouldThrottleSpawns())
	{
		return 0.0f;
	}

	// Calculate delay based on how much over limit we are
	float OverageRatio = CurrentBandwidthKBps / BandwidthLimitKBps;
	float Delay = FMath::Lerp(MinThrottleDelay, MaxThrottleDelay,
		FMath::Clamp((OverageRatio - 1.0f), 0.0f, 1.0f));

	return Delay;
}

void UPACS_NetworkMonitorSubsystem::CheckBandwidthCompliance(float TargetKBps)
{
	if (CurrentBandwidthKBps > TargetKBps)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NetworkMonitor: Bandwidth %.1f KB/s exceeds %.1f KB/s target"),
			CurrentBandwidthKBps, TargetKBps);

		// Log worst offenders
		FGameplayTag WorstTag;
		float MaxBytesPerSecond = 0.0f;

		for (const auto& Pair : SpawnStats)
		{
			if (Pair.Value.BytesPerSecond > MaxBytesPerSecond)
			{
				MaxBytesPerSecond = Pair.Value.BytesPerSecond;
				WorstTag = Pair.Key;
			}
		}

		if (WorstTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Worst offender: %s at %.1f KB/s"),
				*WorstTag.ToString(), MaxBytesPerSecond / 1024.0f);
		}
	}
}

void UPACS_NetworkMonitorSubsystem::ProcessPendingBatches()
{
	// Check throttling
	if (ShouldThrottleSpawns())
	{
		float ThrottleDelay = GetThrottleDelaySeconds();
		float TimeSinceLastSpawn = GetWorld()->GetTimeSeconds() - LastSpawnTime;

		if (TimeSinceLastSpawn < ThrottleDelay)
		{
			UE_LOG(LogTemp, Verbose, TEXT("PACS_NetworkMonitor: Throttling spawns (%.2fs remaining)"),
				ThrottleDelay - TimeSinceLastSpawn);
			return;
		}
	}

	// Process all pending batches
	for (auto It = PendingBatches.CreateIterator(); It; ++It)
	{
		ExecuteBatch(It->Value);
		It.RemoveCurrent();
	}
}

void UPACS_NetworkMonitorSubsystem::ExecuteBatch(const FBatchedSpawnRequest& Batch)
{
	if (Batch.GetCount() == 0)
	{
		return;
	}

	// Get spawn orchestrator
	UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>();
	if (!Orchestrator)
	{
		return;
	}

	// Spawn all actors in batch
	TArray<AActor*> SpawnedActors;
	SpawnedActors.Reserve(Batch.GetCount());

	for (const FTransform& Transform : Batch.SpawnTransforms)
	{
		FSpawnRequestParams Params;
		Params.Transform = Transform;

		if (AActor* SpawnedActor = Orchestrator->AcquireActor(Batch.SpawnTag, Params))
		{
			SpawnedActors.Add(SpawnedActor);
		}
	}

	// Record spawn metrics if any spawns succeeded
	if (SpawnedActors.Num() > 0)
	{
		// Estimate message size (for tracking purposes only - actual replication via ReplicationGraph)
		int32 EstimatedBytes = EstimateBatchSize(Batch);
		RecordSpawnMessage(Batch.SpawnTag, EstimatedBytes);

		UE_LOG(LogTemp, Log, TEXT("PACS_NetworkMonitor: Executed batch of %d spawns for tag %s (est. %d bytes)"),
			SpawnedActors.Num(), *Batch.SpawnTag.ToString(), EstimatedBytes);
	}

	LastSpawnTime = GetWorld()->GetTimeSeconds();
}

int32 UPACS_NetworkMonitorSubsystem::EstimateBatchSize(const FBatchedSpawnRequest& Batch) const
{
	// Estimate network message size
	int32 HeaderSize = 32; // RPC header overhead
	int32 TagSize = 8; // Gameplay tag
	int32 TransformSize = sizeof(FTransform); // 48 bytes per transform
	int32 ArrayOverhead = 8; // TArray overhead

	return HeaderSize + TagSize + ArrayOverhead + (Batch.GetCount() * TransformSize);
}

void UPACS_NetworkMonitorSubsystem::UpdateBandwidthMetrics(float DeltaTime)
{
	TimeSinceLastMeasure += DeltaTime;

	// Update every second
	if (TimeSinceLastMeasure >= 1.0f)
	{
		// Calculate current bandwidth
		CurrentBandwidthKBps = BytesSentThisSecond / 1024.0f;

		// Update history for smoothing
		BandwidthHistory[HistoryIndex] = CurrentBandwidthKBps;
		HistoryIndex = (HistoryIndex + 1) % HistorySize;

		// Calculate smoothed average
		float SumKBps = 0.0f;
		for (float Value : BandwidthHistory)
		{
			SumKBps += Value;
		}
		float SmoothedKBps = SumKBps / HistorySize;

		// Update peak
		if (CurrentBandwidthKBps > PeakBandwidthKBps)
		{
			PeakBandwidthKBps = CurrentBandwidthKBps;
		}

		// Update per-tag stats
		for (auto& Pair : SpawnStats)
		{
			Pair.Value.BytesPerSecond = Pair.Value.TotalBytesSent /
				GetWorld()->GetTimeSeconds();

			if (Pair.Value.BytesPerSecond > Pair.Value.PeakBytesPerSecond)
			{
				Pair.Value.PeakBytesPerSecond = Pair.Value.BytesPerSecond;
			}
		}

		// Check thresholds
		float UsagePercent = CurrentBandwidthKBps / BandwidthLimitKBps;
		if (UsagePercent >= BandwidthCriticalThreshold)
		{
			OnBandwidthCritical(CurrentBandwidthKBps, BandwidthLimitKBps);
		}
		else if (UsagePercent >= BandwidthWarningThreshold)
		{
			OnBandwidthWarning(CurrentBandwidthKBps, BandwidthLimitKBps * BandwidthWarningThreshold);
		}

		// Reset for next second
		BytesSentThisSecond = 0.0f;
		TimeSinceLastMeasure = 0.0f;

		UE_LOG(LogTemp, Verbose, TEXT("PACS_NetworkMonitor: Bandwidth %.1f KB/s (smoothed: %.1f KB/s)"),
			CurrentBandwidthKBps, SmoothedKBps);
	}
}

void UPACS_NetworkMonitorSubsystem::OnBandwidthWarning(float CurrentKBps, float LimitKBps)
{
	UE_LOG(LogTemp, Warning, TEXT("PACS_NetworkMonitor: Bandwidth warning - %.1f KB/s approaching %.1f KB/s limit"),
		CurrentKBps, LimitKBps);

	// Enable throttling if not already
	if (!bEnableThrottling)
	{
		bEnableThrottling = true;
		UE_LOG(LogTemp, Warning, TEXT("PACS_NetworkMonitor: Auto-enabling throttling"));
	}
}

void UPACS_NetworkMonitorSubsystem::OnBandwidthCritical(float CurrentKBps, float LimitKBps)
{
	UE_LOG(LogTemp, Error, TEXT("PACS_NetworkMonitor: CRITICAL bandwidth - %.1f KB/s exceeds %.1f KB/s limit!"),
		CurrentKBps, LimitKBps);

	// Force flush and throttling
	FlushSpawnBatch();
	bEnableThrottling = true;

	// Could implement more aggressive measures like:
	// - Temporarily disabling spawns
	// - Reducing replication frequency
	// - Culling distant actors
}