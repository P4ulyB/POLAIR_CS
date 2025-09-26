#include "Subsystems/PACS_MemoryTracker.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "RenderingThread.h"
#include "HAL/PlatformMemory.h"

void UPACS_MemoryTracker::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("PACS_MemoryTracker: Initialized with budget %.2f MB"), MemoryBudgetMB);
}

void UPACS_MemoryTracker::Deinitialize()
{
	// Clear all tracking data
	ClassMemoryProfiles.Empty();
	PoolMemoryStats.Empty();
	ActorToPoolMap.Empty();
	ActorMemoryCache.Empty();

	Super::Deinitialize();
}

bool UPACS_MemoryTracker::ShouldCreateSubsystem(UObject* Outer) const
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

float UPACS_MemoryTracker::MeasureActorMemory(AActor* Actor)
{
	if (!Actor)
	{
		return 0.0f;
	}

	// Check cache first
	if (TWeakObjectPtr<AActor> WeakActor = Actor; ActorMemoryCache.Contains(WeakActor))
	{
		return ActorMemoryCache[WeakActor];
	}

	float TotalMemoryMB = 0.0f;

	// Base actor memory (rough estimate)
	TotalMemoryMB += sizeof(AActor) / (1024.0f * 1024.0f);

	// Measure all components
	TArray<UActorComponent*> Components = Actor->GetComponents().Array();
	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		// Mesh components
		if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Component))
		{
			TotalMemoryMB += CalculateMeshMemory(MeshComp);

			// Additional animation memory for skeletal meshes
			if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(MeshComp))
			{
				TotalMemoryMB += CalculateAnimationMemory(SkelMeshComp);
			}
		}
		else
		{
			// Generic component memory
			TotalMemoryMB += CalculateComponentMemory(Component);
		}
	}

	// Cache the result
	ActorMemoryCache.Add(Actor, TotalMemoryMB);

	return TotalMemoryMB;
}

FActorMemoryProfile UPACS_MemoryTracker::ProfileActorMemory(AActor* Actor)
{
	FActorMemoryProfile Profile;

	if (!Actor)
	{
		return Profile;
	}

	// Base actor memory
	float BaseMemory = sizeof(AActor) / (1024.0f * 1024.0f);

	// Measure components
	TArray<UActorComponent*> Components = Actor->GetComponents().Array();
	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Component))
		{
			Profile.MeshMemoryMB += CalculateMeshMemory(MeshComp);

			if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(MeshComp))
			{
				Profile.AnimationMemoryMB += CalculateAnimationMemory(SkelMeshComp);
			}
		}
		else
		{
			Profile.ComponentMemoryMB += CalculateComponentMemory(Component);
		}
	}

	Profile.EstimatedMemoryMB = BaseMemory + Profile.MeshMemoryMB +
		Profile.AnimationMemoryMB + Profile.ComponentMemoryMB;
	Profile.LastMeasured = FDateTime::Now();

	// Cache in class profiles
	if (UClass* ActorClass = Actor->GetClass())
	{
		ClassMemoryProfiles.Add(ActorClass, Profile);
	}

	return Profile;
}

void UPACS_MemoryTracker::RegisterPooledActor(FGameplayTag PoolTag, AActor* Actor)
{
	if (!Actor || !PoolTag.IsValid())
	{
		return;
	}

	// Measure memory
	float MemoryMB = MeasureActorMemory(Actor);

	// Update pool stats
	FPoolMemoryStats& Stats = PoolMemoryStats.FindOrAdd(PoolTag);
	Stats.PooledActors++;
	Stats.PooledMemoryMB += MemoryMB;
	Stats.TotalMemoryMB = Stats.ActiveMemoryMB + Stats.PooledMemoryMB;
	Stats.MemoryEfficiency = Stats.ActiveActors > 0 ?
		Stats.ActiveMemoryMB / Stats.TotalMemoryMB : 0.0f;

	// Track actor
	ActorToPoolMap.Add(Actor, PoolTag);

	// Check thresholds
	CheckMemoryThresholds();

	UE_LOG(LogTemp, Verbose, TEXT("PACS_MemoryTracker: Registered actor %s (%.2f MB) to pool %s"),
		*Actor->GetName(), MemoryMB, *PoolTag.ToString());
}

void UPACS_MemoryTracker::UnregisterPooledActor(FGameplayTag PoolTag, AActor* Actor)
{
	if (!Actor || !PoolTag.IsValid())
	{
		return;
	}

	// Get cached memory
	float MemoryMB = 0.0f;
	if (TWeakObjectPtr<AActor> WeakActor = Actor; ActorMemoryCache.Contains(WeakActor))
	{
		MemoryMB = ActorMemoryCache[WeakActor];
		ActorMemoryCache.Remove(WeakActor);
	}

	// Update pool stats
	if (FPoolMemoryStats* Stats = PoolMemoryStats.Find(PoolTag))
	{
		Stats->PooledActors = FMath::Max(0, Stats->PooledActors - 1);
		Stats->PooledMemoryMB = FMath::Max(0.0f, Stats->PooledMemoryMB - MemoryMB);
		Stats->TotalMemoryMB = Stats->ActiveMemoryMB + Stats->PooledMemoryMB;
		Stats->MemoryEfficiency = Stats->ActiveActors > 0 ?
			Stats->ActiveMemoryMB / Stats->TotalMemoryMB : 0.0f;
	}

	// Remove tracking
	ActorToPoolMap.Remove(Actor);
}

void UPACS_MemoryTracker::MarkActorActive(FGameplayTag PoolTag, AActor* Actor, bool bActive)
{
	if (!Actor || !PoolTag.IsValid())
	{
		return;
	}

	// Get cached memory
	float MemoryMB = MeasureActorMemory(Actor);

	// Update pool stats
	if (FPoolMemoryStats* Stats = PoolMemoryStats.Find(PoolTag))
	{
		if (bActive)
		{
			Stats->ActiveActors++;
			Stats->ActiveMemoryMB += MemoryMB;
			Stats->PooledActors = FMath::Max(0, Stats->PooledActors - 1);
			Stats->PooledMemoryMB = FMath::Max(0.0f, Stats->PooledMemoryMB - MemoryMB);
		}
		else
		{
			Stats->ActiveActors = FMath::Max(0, Stats->ActiveActors - 1);
			Stats->ActiveMemoryMB = FMath::Max(0.0f, Stats->ActiveMemoryMB - MemoryMB);
			Stats->PooledActors++;
			Stats->PooledMemoryMB += MemoryMB;
		}

		Stats->TotalMemoryMB = Stats->ActiveMemoryMB + Stats->PooledMemoryMB;
		Stats->MemoryEfficiency = Stats->ActiveActors > 0 ?
			Stats->ActiveMemoryMB / Stats->TotalMemoryMB : 0.0f;
	}

	// Check thresholds
	CheckMemoryThresholds();
}

FPoolMemoryStats UPACS_MemoryTracker::GetPoolMemoryStats(FGameplayTag PoolTag) const
{
	if (const FPoolMemoryStats* Stats = PoolMemoryStats.Find(PoolTag))
	{
		return *Stats;
	}

	return FPoolMemoryStats();
}

float UPACS_MemoryTracker::GetTotalMemoryUsageMB() const
{
	float TotalMB = 0.0f;
	for (const auto& Pair : PoolMemoryStats)
	{
		TotalMB += Pair.Value.TotalMemoryMB;
	}
	return TotalMB;
}

bool UPACS_MemoryTracker::IsMemoryBudgetExceeded() const
{
	return GetTotalMemoryUsageMB() > MemoryBudgetMB;
}

bool UPACS_MemoryTracker::CanAllocateMemoryMB(float RequiredMB) const
{
	float CurrentUsage = GetTotalMemoryUsageMB();
	return (CurrentUsage + RequiredMB) <= MemoryBudgetMB;
}

void UPACS_MemoryTracker::CheckMemoryCompliance(float TargetPerActorMB)
{
	int32 NonCompliantActors = 0;
	float MaxMemory = 0.0f;
	AActor* WorstOffender = nullptr;

	for (const auto& Pair : ActorMemoryCache)
	{
		if (Pair.Key.IsValid() && Pair.Value > TargetPerActorMB)
		{
			NonCompliantActors++;
			if (Pair.Value > MaxMemory)
			{
				MaxMemory = Pair.Value;
				WorstOffender = Pair.Key.Get();
			}
		}
	}

	if (NonCompliantActors > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_MemoryTracker: %d actors exceed %.2f MB target. Worst: %s at %.2f MB"),
			NonCompliantActors, TargetPerActorMB,
			WorstOffender ? *WorstOffender->GetName() : TEXT("Unknown"), MaxMemory);
	}
}

float UPACS_MemoryTracker::CalculateMeshMemory(UMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return 0.0f;
	}

	float MemoryMB = 0.0f;

	// Static mesh memory
	if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(MeshComp))
	{
		if (UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh())
		{
			// Get render data size (approximate)
			if (StaticMesh->GetRenderData() && StaticMesh->GetRenderData()->LODResources.Num() > 0)
			{
				const FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];

				// Vertex buffer size
				MemoryMB += LOD.GetNumVertices() * sizeof(FStaticMeshBuildVertex) / (1024.0f * 1024.0f);

				// Index buffer size
				MemoryMB += LOD.GetNumTriangles() * 3 * sizeof(uint32) / (1024.0f * 1024.0f);
			}
		}
	}
	// Skeletal mesh memory
	else if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(MeshComp))
	{
		if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(SkelMeshComp->GetSkinnedAsset()))
		{
			// Get resource size
			MemoryMB += SkelMesh->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal) / (1024.0f * 1024.0f);
		}
	}

	return MemoryMB;
}

float UPACS_MemoryTracker::CalculateAnimationMemory(USkeletalMeshComponent* SkelMeshComp) const
{
	if (!SkelMeshComp)
	{
		return 0.0f;
	}

	float MemoryMB = 0.0f;

	// Animation instance memory
	if (UAnimInstance* AnimInstance = SkelMeshComp->GetAnimInstance())
	{
		// Base anim instance size
		MemoryMB += AnimInstance->GetClass()->GetStructureSize() / (1024.0f * 1024.0f);

		// Pose memory (approximate - 4x4 matrix per bone)
		if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(SkelMeshComp->GetSkinnedAsset()))
		{
			if (SkelMesh->GetRefSkeleton().GetNum() > 0)
			{
				int32 NumBones = SkelMesh->GetRefSkeleton().GetNum();
				MemoryMB += NumBones * sizeof(FMatrix) / (1024.0f * 1024.0f);
			}
		}
	}

	return MemoryMB;
}

float UPACS_MemoryTracker::CalculateComponentMemory(UActorComponent* Component) const
{
	if (!Component)
	{
		return 0.0f;
	}

	// Base component size
	return Component->GetClass()->GetStructureSize() / (1024.0f * 1024.0f);
}

void UPACS_MemoryTracker::CheckMemoryThresholds()
{
	float CurrentUsage = GetTotalMemoryUsageMB();
	float UsagePercent = CurrentUsage / MemoryBudgetMB;

	if (UsagePercent >= MemoryCriticalThreshold)
	{
		OnMemoryCritical(CurrentUsage, MemoryBudgetMB);
	}
	else if (UsagePercent >= MemoryWarningThreshold)
	{
		OnMemoryWarning(CurrentUsage, MemoryBudgetMB * MemoryWarningThreshold);
	}
}

void UPACS_MemoryTracker::OnMemoryWarning(float CurrentMB, float ThresholdMB)
{
	UE_LOG(LogTemp, Warning, TEXT("PACS_MemoryTracker: Memory usage warning - %.2f MB of %.2f MB threshold"),
		CurrentMB, ThresholdMB);

	// Could trigger memory optimization here
}

void UPACS_MemoryTracker::OnMemoryCritical(float CurrentMB, float LimitMB)
{
	UE_LOG(LogTemp, Error, TEXT("PACS_MemoryTracker: CRITICAL memory usage - %.2f MB exceeds %.2f MB limit!"),
		CurrentMB, LimitMB);

	// Force memory reduction - could hibernate actors, reduce pool sizes, etc.
}