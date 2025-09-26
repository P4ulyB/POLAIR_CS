// Copyright POLAIR_CS Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/PACS_CharacterPool.h"
#include "PACS_NPCSpawnManager.generated.h"

class APACS_NPCSpawnPoint;
class IPACS_SelectableCharacterInterface;
class UPACS_SpawnConfiguration;
class APACSGameMode;

/**
 * Simple spawn manager that populates spawn points with pooled characters
 * Automatically finds all spawn points in the level and spawns NPCs at them
 */
UCLASS()
class POLAIR_CS_API UPACS_NPCSpawnManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // UWorldSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Spawn NPCs at all spawn points in the level
     * Called automatically by GameMode at BeginPlay
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    void SpawnAllNPCs();

    /**
     * Spawn NPCs asynchronously to prevent server tick blocking
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    void SpawnAllNPCsAsync();

    /**
     * Clear all spawned NPCs and return them to pool
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    void DespawnAllNPCs();

    /**
     * Get total number of NPCs spawned
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    int32 GetSpawnedNPCCount() const { return SpawnedCharacters.Num(); }

    /**
     * Find all spawn points in the level
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    TArray<APACS_NPCSpawnPoint*> GetAllSpawnPoints() const;

protected:
    /**
     * Spawn a single NPC at the given spawn point using configuration
     */
    bool SpawnNPCAtPoint(APACS_NPCSpawnPoint* SpawnPoint);

    /**
     * Load spawn configuration from GameMode
     */
    void LoadSpawnConfiguration();

    /**
     * Cache all spawn points in the level for performance
     */
    void CacheSpawnPoints();

    /**
     * Get character type to spawn based on configuration
     */
    EPACSCharacterType GetCharacterTypeForSpawnPoint(APACS_NPCSpawnPoint* SpawnPoint) const;

    /**
     * Async spawning helper - spawns batch of NPCs per tick
     */
    void SpawnNextBatch();

private:
    // Reference to character pool subsystem
    UPROPERTY()
    UPACS_CharacterPool* CharacterPool = nullptr;

    // Reference to spawn configuration from GameMode
    UPROPERTY()
    UPACS_SpawnConfiguration* SpawnConfiguration = nullptr;

    // Unified tracking system using interface
    UPROPERTY()
    TArray<TScriptInterface<IPACS_SelectableCharacterInterface>> SpawnedCharacters;

    // Unified spawn point mapping
    UPROPERTY()
    TMap<APACS_NPCSpawnPoint*, TScriptInterface<IPACS_SelectableCharacterInterface>> SpawnPointMapping;

    // Cached spawn points for performance
    UPROPERTY()
    TArray<APACS_NPCSpawnPoint*> CachedSpawnPoints;

    // Performance tracking
    bool bSpawnPointsCached = false;
    int32 CurrentSpawnIndex = 0;

    // Async spawning state
    bool bAsyncSpawningActive = false;
    FTimerHandle AsyncSpawnTimerHandle;
};