#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Engine/StreamableManager.h"
#include "PACS_SpawnOrchestrator.generated.h"

class AActor;
class UPACS_SpawnConfig;
class IPACS_Poolable;

/**
 * Pool entry for managing a single spawnable type
 */
USTRUCT()
struct FPoolEntry
{
	GENERATED_BODY()

	// Available actors (returned to pool)
	TArray<TWeakObjectPtr<AActor>> AvailableActors;

	// Active actors (currently in use)
	TArray<TWeakObjectPtr<AActor>> ActiveActors;

	// Soft class reference for lazy loading
	TSoftClassPtr<AActor> ActorClass;

	// Resolved class (after async load)
	TSubclassOf<AActor> ResolvedClass;

	// Pool configuration
	int32 InitialSize = 5;
	int32 MaxSize = 20;
	int32 CurrentSize = 0;

	// Loading state
	bool bIsLoading = false;
	TArray<TPair<FTransform, TFunction<void(AActor*)>>> PendingRequests;

	void Reset()
	{
		AvailableActors.Empty();
		ActiveActors.Empty();
		ResolvedClass = nullptr;
		CurrentSize = 0;
		bIsLoading = false;
		PendingRequests.Empty();
	}
};

/**
 * Spawn request parameters
 */
USTRUCT(BlueprintType)
struct FSpawnRequestParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FTransform Transform;

	UPROPERTY(BlueprintReadWrite)
	AActor* Owner = nullptr;

	UPROPERTY(BlueprintReadWrite)
	APawn* Instigator = nullptr;

	UPROPERTY(BlueprintReadWrite)
	bool bDeferredSpawn = false;
};

/**
 * Single orchestrator for all spawn operations in dedicated server environment
 * Manages object pools with server-authoritative spawning and explicit replication control
 */
UCLASS()
class POLAIR_CS_API UPACS_SpawnOrchestrator : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// Core pool operations
	UFUNCTION(BlueprintCallable, Category = "Spawn System", meta = (CallInEditor = "true"))
	AActor* AcquireActor(FGameplayTag SpawnTag, const FSpawnRequestParams& Params);

	UFUNCTION(BlueprintCallable, Category = "Spawn System")
	void ReleaseActor(AActor* Actor);

	// Pool management
	UFUNCTION(BlueprintCallable, Category = "Spawn System")
	void PrewarmPool(FGameplayTag SpawnTag, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Spawn System")
	void FlushPool(FGameplayTag SpawnTag);

	UFUNCTION(BlueprintCallable, Category = "Spawn System")
	void FlushAllPools();

	// Configuration
	UFUNCTION(BlueprintCallable, Category = "Spawn System")
	void SetSpawnConfig(UPACS_SpawnConfig* InConfig);

	// Ready state check
	UFUNCTION(BlueprintPure, Category = "Spawn System")
	bool IsReady() const { return SpawnConfig != nullptr; }

	// Statistics
	UFUNCTION(BlueprintPure, Category = "Spawn System")
	void GetPoolStatistics(FGameplayTag SpawnTag, int32& OutActive, int32& OutAvailable, int32& OutTotal) const;

protected:
	// Internal pool management
	void InitializePool(FGameplayTag SpawnTag);
	AActor* CreatePooledActor(FGameplayTag SpawnTag, TSubclassOf<AActor> ActorClass);
	void ReturnActorToPool(AActor* Actor, FGameplayTag SpawnTag);
	void ResetActorForPool(AActor* Actor);
	void PrepareActorForUse(AActor* Actor, const FSpawnRequestParams& Params);

	// Async loading
	void LoadActorClass(FGameplayTag SpawnTag);
	void OnActorClassLoaded(FGameplayTag SpawnTag);
	void ProcessPendingRequests(FGameplayTag SpawnTag);

	// Replication state management
	void ResetReplicationState(AActor* Actor);
	void PrepareReplicationState(AActor* Actor);

	// Selection profile pre-loading
	void PreloadSelectionProfiles();

	// === PROFILE APPLICATION SECTION ===
	// Organized methods for applying selection profiles to actors
	void ApplySelectionProfileToActor(AActor* Actor, class UPACS_SelectionProfileAsset* Profile);
	void ApplyProfileToCharacterNPC(class APACS_NPC_Base_Char* CharNPC, class UPACS_SelectionProfileAsset* Profile);
	void ApplyProfileToVehicleNPC(class APACS_NPC_Base_Veh* VehNPC, class UPACS_SelectionProfileAsset* Profile);
	void ApplyProfileToLightweightNPC(class APACS_NPC_Base_LW* LightweightNPC, class UPACS_SelectionProfileAsset* Profile);
	bool VerifyProfileAssetsLoaded(class UPACS_SelectionProfileAsset* Profile);
	void LogProfileApplicationStatus(AActor* Actor, bool bSuccess, const FString& Reason);

private:
	// Pool storage by gameplay tag
	UPROPERTY()
	TMap<FGameplayTag, FPoolEntry> Pools;

	// Configuration reference
	UPROPERTY()
	TObjectPtr<UPACS_SpawnConfig> SpawnConfig;

	// Tag to actor mapping for release operations
	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, FGameplayTag> ActorToTagMap;

	// Streamable manager for async loading
	FStreamableManager StreamableManager;

	// Handles for async loads
	TMap<FGameplayTag, TSharedPtr<FStreamableHandle>> LoadHandles;
};