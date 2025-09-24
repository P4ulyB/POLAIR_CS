// Copyright POLAIR_CS Project. All Rights Reserved.

#include "Systems/PACS_NPCSpawnManager.h"
#include "Systems/PACS_CharacterPool.h"
#include "Actors/PACS_NPCSpawnPoint.h"
#include "Pawns/NPC/PACS_NPCCharacter.h"
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

    // Return all spawned NPCs to pool
    for (APACS_NPCCharacter* NPC : SpawnedNPCs)
    {
        if (IsValid(NPC))
        {
            CharacterPool->ReleaseCharacter(NPC);
        }
    }

    // Clear tracking arrays
    SpawnedNPCs.Empty();
    SpawnPointMapping.Empty();

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
        UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] SpawnNPCAtPoint - Invalid or disabled spawn point"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] SpawnNPCAtPoint - Location: %s, Type: %d"),
        *SpawnPoint->GetActorLocation().ToString(), (int32)SpawnPoint->CharacterType);

    // Check if this point already has an NPC
    if (SpawnPoint->GetSpawnedCharacter())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: Spawn point already has NPC"));
        return false;
    }

    if (!CharacterPool)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_NPCSpawnManager: No character pool available"));
        return false;
    }

    // Acquire character from pool (convert enum types)
    EPACSCharacterType PoolCharType = static_cast<EPACSCharacterType>(SpawnPoint->CharacterType);
    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Requesting character from pool - Type: %s"), *UEnum::GetValueAsString(PoolCharType));

    APACS_NPCCharacter* NPC = CharacterPool->AcquireCharacter(
        PoolCharType,
        GetWorld()
    );

    if (!NPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: Failed to acquire character from pool"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Successfully acquired NPC: %s"), *NPC->GetName());

    // Debug: Check controller state immediately after acquiring from pool
    AController* Controller = NPC->GetController();
    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Pre-positioning controller: %s"), Controller ? *Controller->GetName() : TEXT("NULL"));
    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Pre-positioning AIControllerClass: %s"), NPC->AIControllerClass ? *NPC->AIControllerClass->GetName() : TEXT("NULL"));
    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Pre-positioning AutoPossessAI: %d"), (int32)NPC->AutoPossessAI);

    // Position the NPC at spawn point
    FVector SpawnLocation = SpawnPoint->GetActorLocation();
    FRotator SpawnRotation = SpawnPoint->SpawnRotation.IsNearlyZero() ?
        SpawnPoint->GetActorRotation() : SpawnPoint->SpawnRotation;

    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Setting NPC position to: %s"), *SpawnLocation.ToString());
    NPC->SetActorLocation(SpawnLocation);
    NPC->SetActorRotation(SpawnRotation);

    // Debug: Check controller state after positioning
    Controller = NPC->GetController();
    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Post-positioning controller: %s"), Controller ? *Controller->GetName() : TEXT("NULL"));
    if (Controller)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Controller pawn: %s"), Controller->GetPawn() ? *Controller->GetPawn()->GetName() : TEXT("NULL"));
        UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Controller class: %s"), *Controller->GetClass()->GetName());
    }

    // Track the spawned NPC
    SpawnedNPCs.Add(NPC);
    SpawnPointMapping.Add(SpawnPoint, NPC);
    SpawnPoint->SetSpawnedCharacter(NPC);

    UE_LOG(LogTemp, Warning, TEXT("[SPAWN DEBUG] Successfully spawned and positioned %s at %s"),
        *UEnum::GetValueAsString(SpawnPoint->CharacterType),
        *SpawnLocation.ToString());

    return true;
}