#include "Components/PACS_NetworkMonitor.h"
#include "Subsystems/PACS_SpawnOrchestrator.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Net/UnrealNetwork.h"

UPACS_NetworkMonitor::UPACS_NetworkMonitor()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // Tick every 100ms

	// Initialize bandwidth history
	BandwidthHistory.SetNum(HistorySize);
	for (int32 i = 0; i < HistorySize; i++)
	{
		BandwidthHistory[i] = 0.0f;
	}
}

void UPACS_NetworkMonitor::BeginPlay()
{
	Super::BeginPlay();

	// Only run on server
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		SetComponentTickEnabled(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NetworkMonitor: Initialized with %.1f KB/s bandwidth limit"),
		BandwidthLimitKBps);
}

void UPACS_NetworkMonitor::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only monitor on server
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

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

void UPACS_NetworkMonitor::QueueSpawnRequest(FGameplayTag SpawnTag, const FTransform& Transform)
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

void UPACS_NetworkMonitor::FlushSpawnBatch()
{
	ProcessPendingBatches();
}

void UPACS_NetworkMonitor::RecordSpawnMessage(FGameplayTag SpawnTag, int32 MessageSizeBytes)
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

void UPACS_NetworkMonitor::RecordActorReplication(AActor* Actor, int32 BytesReplicated)
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

FSpawnNetworkStats UPACS_NetworkMonitor::GetSpawnNetworkStats(FGameplayTag SpawnTag) const
{
	if (const FSpawnNetworkStats* Stats = SpawnStats.Find(SpawnTag))
	{
		return *Stats;
	}

	return FSpawnNetworkStats();
}

bool UPACS_NetworkMonitor::ShouldThrottleSpawns() const
{
	if (!bEnableThrottling)
	{
		return false;
	}

	// Throttle if bandwidth exceeded
	return CurrentBandwidthKBps > (BandwidthLimitKBps * BandwidthWarningThreshold);
}

float UPACS_NetworkMonitor::GetThrottleDelaySeconds() const
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

void UPACS_NetworkMonitor::CheckBandwidthCompliance(float TargetKBps)
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

void UPACS_NetworkMonitor::ProcessPendingBatches()
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

void UPACS_NetworkMonitor::ExecuteBatch(const FBatchedSpawnRequest& Batch)
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

	// Send batched multicast if any spawns succeeded
	if (SpawnedActors.Num() > 0)
	{
		// Estimate message size
		int32 EstimatedBytes = EstimateBatchSize(Batch);
		RecordSpawnMessage(Batch.SpawnTag, EstimatedBytes);

		// Send batched multicast
		MulticastBatchedSpawn(Batch.SpawnTag, Batch.SpawnTransforms);

		UE_LOG(LogTemp, Log, TEXT("PACS_NetworkMonitor: Executed batch of %d spawns for tag %s (est. %d bytes)"),
			SpawnedActors.Num(), *Batch.SpawnTag.ToString(), EstimatedBytes);
	}

	LastSpawnTime = GetWorld()->GetTimeSeconds();
}

int32 UPACS_NetworkMonitor::EstimateBatchSize(const FBatchedSpawnRequest& Batch) const
{
	// Estimate network message size
	int32 HeaderSize = 32; // RPC header overhead
	int32 TagSize = 8; // Gameplay tag
	int32 TransformSize = sizeof(FTransform); // 48 bytes per transform
	int32 ArrayOverhead = 8; // TArray overhead

	return HeaderSize + TagSize + ArrayOverhead + (Batch.GetCount() * TransformSize);
}

void UPACS_NetworkMonitor::UpdateBandwidthMetrics(float DeltaTime)
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

void UPACS_NetworkMonitor::OnBandwidthWarning(float CurrentKBps, float LimitKBps)
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

void UPACS_NetworkMonitor::OnBandwidthCritical(float CurrentKBps, float LimitKBps)
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

void UPACS_NetworkMonitor::MulticastBatchedSpawn_Implementation(FGameplayTag SpawnTag,
	const TArray<FTransform>& Transforms)
{
	// This would be received by clients
	// Clients would process the batched spawn visuals/effects
	// Note: Actual actor spawning is server-authoritative via SpawnOrchestrator

	UE_LOG(LogTemp, Log, TEXT("PACS_NetworkMonitor: Client received batch of %d spawns for tag %s"),
		Transforms.Num(), *SpawnTag.ToString());
}