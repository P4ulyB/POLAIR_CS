// Copyright POLAIR_CS Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Systems/PACS_CharacterPool.h"
#include "PACS_NPCSpawnManager.generated.h"

class APACS_NPCSpawnPoint;
class APACS_NPCCharacter;

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
     * Clear all spawned NPCs and return them to pool
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    void DespawnAllNPCs();

    /**
     * Get total number of NPCs spawned
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    int32 GetSpawnedNPCCount() const { return SpawnedNPCs.Num(); }

    /**
     * Find all spawn points in the level
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    TArray<APACS_NPCSpawnPoint*> GetAllSpawnPoints() const;

protected:
    /**
     * Spawn a single NPC at the given spawn point
     */
    bool SpawnNPCAtPoint(APACS_NPCSpawnPoint* SpawnPoint);

private:
    // Reference to character pool subsystem
    UPROPERTY()
    UPACS_CharacterPool* CharacterPool = nullptr;

    // Track all spawned NPCs for cleanup
    UPROPERTY()
    TArray<APACS_NPCCharacter*> SpawnedNPCs;

    // Track spawn points that have NPCs
    UPROPERTY()
    TMap<APACS_NPCSpawnPoint*, APACS_NPCCharacter*> SpawnPointMapping;
};