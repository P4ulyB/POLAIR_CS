#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "PACS_MemoryTracker.generated.h"

class AActor;

/**
 * Memory usage data for a single actor type
 */
USTRUCT(BlueprintType)
struct FActorMemoryProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float EstimatedMemoryMB = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float MeshMemoryMB = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float AnimationMemoryMB = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float ComponentMemoryMB = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FDateTime LastMeasured;

	float GetTotalMemoryMB() const { return EstimatedMemoryMB; }
};

/**
 * Pool memory statistics
 */
USTRUCT(BlueprintType)
struct FPoolMemoryStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 ActiveActors = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PooledActors = 0;

	UPROPERTY(BlueprintReadOnly)
	float ActiveMemoryMB = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float PooledMemoryMB = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float TotalMemoryMB = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float MemoryEfficiency = 0.0f; // Active / Total ratio
};

/**
 * Memory tracking subsystem for spawn pools
 * Monitors memory usage to ensure compliance with 1MB per actor target
 */
UCLASS()
class POLAIR_CS_API UPACS_MemoryTracker : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// Memory measurement
	UFUNCTION(BlueprintCallable, Category = "Memory Tracking")
	float MeasureActorMemory(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Memory Tracking")
	FActorMemoryProfile ProfileActorMemory(AActor* Actor);

	// Pool memory tracking
	UFUNCTION(BlueprintCallable, Category = "Memory Tracking")
	void RegisterPooledActor(FGameplayTag PoolTag, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Memory Tracking")
	void UnregisterPooledActor(FGameplayTag PoolTag, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Memory Tracking")
	void MarkActorActive(FGameplayTag PoolTag, AActor* Actor, bool bActive);

	// Memory queries
	UFUNCTION(BlueprintPure, Category = "Memory Tracking")
	FPoolMemoryStats GetPoolMemoryStats(FGameplayTag PoolTag) const;

	UFUNCTION(BlueprintPure, Category = "Memory Tracking")
	float GetTotalMemoryUsageMB() const;

	UFUNCTION(BlueprintPure, Category = "Memory Tracking")
	bool IsMemoryBudgetExceeded() const;

	// Memory budget management
	UFUNCTION(BlueprintCallable, Category = "Memory Tracking")
	void SetMemoryBudgetMB(float BudgetMB) { MemoryBudgetMB = BudgetMB; }

	UFUNCTION(BlueprintPure, Category = "Memory Tracking")
	float GetMemoryBudgetMB() const { return MemoryBudgetMB; }

	UFUNCTION(BlueprintCallable, Category = "Memory Tracking")
	bool CanAllocateMemoryMB(float RequiredMB) const;

	// Performance alerts
	UFUNCTION(BlueprintCallable, Category = "Memory Tracking")
	void CheckMemoryCompliance(float TargetPerActorMB = 1.0f);

protected:
	// Memory calculation helpers
	float CalculateMeshMemory(class UMeshComponent* MeshComp) const;
	float CalculateAnimationMemory(class USkeletalMeshComponent* SkelMeshComp) const;
	float CalculateComponentMemory(UActorComponent* Component) const;

	// Alert thresholds
	void CheckMemoryThresholds();
	void OnMemoryWarning(float CurrentMB, float ThresholdMB);
	void OnMemoryCritical(float CurrentMB, float LimitMB);

private:
	// Memory profiles by actor class
	UPROPERTY()
	TMap<UClass*, FActorMemoryProfile> ClassMemoryProfiles;

	// Pool memory tracking
	UPROPERTY()
	TMap<FGameplayTag, FPoolMemoryStats> PoolMemoryStats;

	// Active actor tracking
	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, FGameplayTag> ActorToPoolMap;

	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, float> ActorMemoryCache;

	// Memory budget settings
	UPROPERTY()
	float MemoryBudgetMB = 100.0f; // Default 100MB for all pools

	UPROPERTY()
	float MemoryWarningThreshold = 0.8f; // Warn at 80% usage

	UPROPERTY()
	float MemoryCriticalThreshold = 0.95f; // Critical at 95% usage

	// Update tracking
	float LastMemoryCheck = 0.0f;
	float MemoryCheckInterval = 1.0f; // Check every second
};