// Copyright POLAIR_CS Project. All Rights Reserved.

#include "Subsystems/PACS_NPCSpawnManager.h"
#include "Subsystems/PACS_CharacterPool.h"
#include "Actors/PACS_NPCSpawnPoint.h"
#include "Actors/NPC/PACS_NPCCharacter.h"
#include "Actors/NPC/PACS_NPC_Humanoid.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"

void UPACS_NPCSpawnManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: Initialized"));
}

void UPACS_NPCSpawnManager::Deinitialize()
{
    // Clean up all spawned NPCs before shutdown
    DespawnAllNPCs();

    Super::Deinitialize();
}

void UPACS_NPCSpawnManager::SpawnAllNPCs()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_NPCSpawnManager: No valid world"));
        return;
    }

    // Only spawn on server
    if (!World->GetAuthGameMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: Not on server, skipping spawn"));
        return;
    }

    // Get character pool
    if (!CharacterPool)
    {
        CharacterPool = World->GetGameInstance() ?
            World->GetGameInstance()->GetSubsystem<UPACS_CharacterPool>() : nullptr;
    }

    if (!CharacterPool)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_NPCSpawnManager: Character pool not available"));
        return;
    }

    // CRITICAL: Preload all character assets before spawning
    // This eliminates the WaitForTasks bottleneck
    CharacterPool->PreloadCharacterAssets();

    // Find all spawn points in the level
    TArray<APACS_NPCSpawnPoint*> SpawnPoints = GetAllSpawnPoints();

    int32 SuccessCount = 0;
    int32 FailCount = 0;

    // Spawn at each point
    for (APACS_NPCSpawnPoint* SpawnPoint : SpawnPoints)
    {
        if (SpawnNPCAtPoint(SpawnPoint))
        {
            SuccessCount++;
        }
        else
        {
            FailCount++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: Spawned %d NPCs successfully, %d failed"),
        SuccessCount, FailCount);
}

void UPACS_NPCSpawnManager::DespawnAllNPCs()
{
    if (!CharacterPool)
    {
        return;
    }

    // Return all spawned heavyweight NPCs to pool
    for (APACS_NPCCharacter* NPC : SpawnedNPCs)
    {
        if (IsValid(NPC))
        {
            CharacterPool->ReleaseCharacter(NPC);
        }
    }

    // Return all spawned lightweight NPCs to pool
    for (APACS_NPC_Humanoid* LightweightNPC : SpawnedLightweightNPCs)
    {
        if (IsValid(LightweightNPC))
        {
            CharacterPool->ReleaseLightweightCharacter(LightweightNPC);
        }
    }

    // Clear tracking arrays
    SpawnedNPCs.Empty();
    SpawnedLightweightNPCs.Empty();
    SpawnPointMapping.Empty();
    LightweightSpawnPointMapping.Empty();

    // Clear spawn point references
    for (TActorIterator<APACS_NPCSpawnPoint> It(GetWorld()); It; ++It)
    {
        if (APACS_NPCSpawnPoint* SpawnPoint = *It)
        {
            SpawnPoint->SetSpawnedCharacter(nullptr);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: All NPCs returned to pool"));
}

TArray<APACS_NPCSpawnPoint*> UPACS_NPCSpawnManager::GetAllSpawnPoints() const
{
    TArray<APACS_NPCSpawnPoint*> SpawnPoints;

    // Find all spawn points in the world
    for (TActorIterator<APACS_NPCSpawnPoint> It(GetWorld()); It; ++It)
    {
        if (APACS_NPCSpawnPoint* SpawnPoint = *It)
        {
            if (SpawnPoint->bEnabled)
            {
                SpawnPoints.Add(SpawnPoint);
            }
        }
    }

    return SpawnPoints;
}

bool UPACS_NPCSpawnManager::SpawnNPCAtPoint(APACS_NPCSpawnPoint* SpawnPoint)
{
    if (!SpawnPoint || !SpawnPoint->bEnabled)
    {
        return false;
    }

    // Check if this point already has an NPC
    if (SpawnPoint->GetSpawnedCharacter())
    {
        return false;
    }

    if (!CharacterPool)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_NPCSpawnManager: No character pool available"));
        return false;
    }

    // Convert to lightweight type for better performance
    // Map old types to new lightweight types
    EPACSCharacterType PoolCharType;
    switch (static_cast<EPACSCharacterType>(SpawnPoint->CharacterType))
    {
        case EPACSCharacterType::Civilian:
            PoolCharType = EPACSCharacterType::LightweightCivilian;
            break;
        case EPACSCharacterType::Police:
            PoolCharType = EPACSCharacterType::LightweightPolice;
            break;
        case EPACSCharacterType::Firefighter:
            PoolCharType = EPACSCharacterType::LightweightFirefighter;
            break;
        case EPACSCharacterType::Paramedic:
            PoolCharType = EPACSCharacterType::LightweightParamedic;
            break;
        default:
            PoolCharType = EPACSCharacterType::LightweightCivilian;
            break;
    }

    // Acquire lightweight character from pool
    APACS_NPC_Humanoid* LightweightNPC = CharacterPool->AcquireLightweightCharacter(
        PoolCharType,
        GetWorld()
    );

    if (!LightweightNPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: Failed to acquire lightweight character from pool for type %s"),
            *UEnum::GetValueAsString(PoolCharType));
        return false;
    }

    // Position the lightweight NPC at spawn point
    FVector SpawnLocation = SpawnPoint->GetActorLocation();
    FRotator SpawnRotation = SpawnPoint->SpawnRotation.IsNearlyZero() ?
        SpawnPoint->GetActorRotation() : SpawnPoint->SpawnRotation;

    LightweightNPC->SetActorLocation(SpawnLocation);
    LightweightNPC->SetActorRotation(SpawnRotation);

    // Track the spawned lightweight NPC
    SpawnedLightweightNPCs.Add(LightweightNPC);
    LightweightSpawnPointMapping.Add(SpawnPoint, LightweightNPC);

    // Note: SpawnPoint expects APACS_NPCCharacter, but we're using lightweight now
    // This will need to be updated to use the interface instead
    // For now, we'll skip setting it on the spawn point
    // SpawnPoint->SetSpawnedCharacter(NPC);

    return true;
}