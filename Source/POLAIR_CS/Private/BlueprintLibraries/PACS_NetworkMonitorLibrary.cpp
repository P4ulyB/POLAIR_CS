#include "BlueprintLibraries/PACS_NetworkMonitorLibrary.h"
#include "Subsystems/PACS_NetworkMonitorSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UPACS_NetworkMonitorSubsystem* UPACS_NetworkMonitorLibrary::GetSubsystem(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UPACS_NetworkMonitorSubsystem>();
}

void UPACS_NetworkMonitorLibrary::QueueSpawnRequest(UObject* WorldContextObject,
                                                     FGameplayTag SpawnTag,
                                                     const FTransform& Transform)
{
	if (UPACS_NetworkMonitorSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->QueueSpawnRequest(SpawnTag, Transform);
	}
}

void UPACS_NetworkMonitorLibrary::FlushSpawnBatch(UObject* WorldContextObject)
{
	if (UPACS_NetworkMonitorSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->FlushSpawnBatch();
	}
}

float UPACS_NetworkMonitorLibrary::GetCurrentBandwidth(UObject* WorldContextObject)
{
	if (UPACS_NetworkMonitorSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		return Subsystem->GetCurrentBandwidthKBps();
	}
	return 0.0f;
}

float UPACS_NetworkMonitorLibrary::GetPeakBandwidth(UObject* WorldContextObject)
{
	if (UPACS_NetworkMonitorSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		return Subsystem->GetPeakBandwidthKBps();
	}
	return 0.0f;
}

bool UPACS_NetworkMonitorLibrary::IsOverBandwidthLimit(UObject* WorldContextObject)
{
	if (UPACS_NetworkMonitorSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		return Subsystem->IsBandwidthExceeded();
	}
	return false;
}

FSpawnNetworkStats UPACS_NetworkMonitorLibrary::GetSpawnNetworkStats(UObject* WorldContextObject,
                                                                       FGameplayTag SpawnTag)
{
	if (UPACS_NetworkMonitorSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		return Subsystem->GetSpawnNetworkStats(SpawnTag);
	}
	return FSpawnNetworkStats();
}

void UPACS_NetworkMonitorLibrary::SetBatchingEnabled(UObject* WorldContextObject, bool bEnable)
{
	if (UPACS_NetworkMonitorSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->EnableBatching(bEnable);
	}
}

void UPACS_NetworkMonitorLibrary::SetBandwidthLimit(UObject* WorldContextObject, float LimitKBps)
{
	if (UPACS_NetworkMonitorSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->SetBandwidthLimit(LimitKBps);
	}
}