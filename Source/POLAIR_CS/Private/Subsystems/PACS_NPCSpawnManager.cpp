// Copyright POLAIR_CS Project. All Rights Reserved.

#include "Subsystems/PACS_NPCSpawnManager.h"
#include "Subsystems/PACS_CharacterPool.h"
#include "Actors/PACS_NPCSpawnPoint.h"
#include "Actors/NPC/PACS_NPCCharacter.h"
#include "Actors/NPC/PACS_NPC_Humanoid.h"
#include "Data/PACS_SpawnConfiguration.h"
#include "Core/PACSGameMode.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"

void UPACS_NPCSpawnManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Load spawn configuration from GameMode (server authority)
    LoadSpawnConfiguration();

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

    // Server authority check
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

    // Load configuration if not already loaded
    if (!SpawnConfiguration)
    {
        LoadSpawnConfiguration();
    }

    // Cache spawn points for performance
    CacheSpawnPoints();

    // CRITICAL: Preload all character assets before spawning
    // This eliminates the WaitForTasks bottleneck
    CharacterPool->PreloadCharacterAssets();

    // Get spawn points using cached system
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

    UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: Spawned %d NPCs successfully, %d failed (Total active: %d)"),
        SuccessCount, FailCount, SpawnedCharacters.Num());
}

void UPACS_NPCSpawnManager::DespawnAllNPCs()
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: DespawnAllNPCs called on client - ignoring"));
        return;
    }

    if (!CharacterPool)
    {
        return;
    }

    // Return all spawned characters to pool using unified interface
    for (auto& CharacterInterface : SpawnedCharacters)
    {
        if (CharacterInterface.GetInterface())
        {
            // Try to cast to heavyweight first
            if (APawn* Pawn = Cast<APawn>(CharacterInterface.GetObject()))
            {
                if (APACS_NPCCharacter* HeavyweightNPC = Cast<APACS_NPCCharacter>(Pawn))
                {
                    CharacterPool->ReleaseCharacter(HeavyweightNPC);
                }
                else if (APACS_NPC_Humanoid* LightweightNPC = Cast<APACS_NPC_Humanoid>(Pawn))
                {
                    CharacterPool->ReleaseLightweightCharacter(LightweightNPC);
                }
            }
        }
    }

    // Clear unified tracking systems
    SpawnedCharacters.Empty();
    SpawnPointMapping.Empty();

    // Clear spawn point references using cached points if available
    if (bSpawnPointsCached)
    {
        for (APACS_NPCSpawnPoint* SpawnPoint : CachedSpawnPoints)
        {
            if (IsValid(SpawnPoint))
            {
                SpawnPoint->SetSpawnedCharacter(TScriptInterface<IPACS_SelectableCharacterInterface>());
            }
        }
    }
    else
    {
        // Fallback to world iteration
        for (TActorIterator<APACS_NPCSpawnPoint> It(GetWorld()); It; ++It)
        {
            if (APACS_NPCSpawnPoint* SpawnPoint = *It)
            {
                SpawnPoint->SetSpawnedCharacter(TScriptInterface<IPACS_SelectableCharacterInterface>());
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: All %d NPCs returned to pool"), SpawnedCharacters.Num());
}

TArray<APACS_NPCSpawnPoint*> UPACS_NPCSpawnManager::GetAllSpawnPoints() const
{
    // Use cached spawn points if available (performance optimization)
    if (bSpawnPointsCached && CachedSpawnPoints.Num() > 0)
    {
        TArray<APACS_NPCSpawnPoint*> EnabledSpawnPoints;
        for (APACS_NPCSpawnPoint* SpawnPoint : CachedSpawnPoints)
        {
            if (IsValid(SpawnPoint) && SpawnPoint->bEnabled)
            {
                EnabledSpawnPoints.Add(SpawnPoint);
            }
        }
        return EnabledSpawnPoints;
    }

    // Fallback to world iteration if cache not available
    TArray<APACS_NPCSpawnPoint*> SpawnPoints;
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
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: SpawnNPCAtPoint called on client - ignoring"));
        return false;
    }

    if (!SpawnPoint || !SpawnPoint->bEnabled)
    {
        return false;
    }

    // Check if this point already has an NPC
    if (SpawnPoint->GetSpawnedCharacter().GetInterface())
    {
        return false;
    }

    if (!CharacterPool)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_NPCSpawnManager: No character pool available"));
        return false;
    }

    // Get character type using configuration
    EPACSCharacterType PoolCharType = GetCharacterTypeForSpawnPoint(SpawnPoint);

    // Check spawn limits if configuration is available
    if (SpawnConfiguration)
    {
        if (!SpawnConfiguration->IsSpawningAllowed(PoolCharType, SpawnedCharacters.Num()))
        {
            UE_LOG(LogTemp, Verbose, TEXT("PACS_NPCSpawnManager: Spawn limit reached for type %s"),
                *UEnum::GetValueAsString(PoolCharType));
            return false;
        }
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

    // Position the NPC at spawn point
    FVector SpawnLocation = SpawnPoint->GetActorLocation();
    FRotator SpawnRotation = SpawnPoint->SpawnRotation.IsNearlyZero() ?
        SpawnPoint->GetActorRotation() : SpawnPoint->SpawnRotation;

    LightweightNPC->SetActorLocation(SpawnLocation);
    LightweightNPC->SetActorRotation(SpawnRotation);

    // Create interface reference
    TScriptInterface<IPACS_SelectableCharacterInterface> CharacterInterface;
    CharacterInterface.SetObject(LightweightNPC);
    CharacterInterface.SetInterface(Cast<IPACS_SelectableCharacterInterface>(LightweightNPC));

    // Track using unified interface system
    SpawnedCharacters.Add(CharacterInterface);
    SpawnPointMapping.Add(SpawnPoint, CharacterInterface);

    // Set reference on spawn point
    SpawnPoint->SetSpawnedCharacter(CharacterInterface);

    UE_LOG(LogTemp, Verbose, TEXT("PACS_NPCSpawnManager: Spawned %s at %s"),
        *UEnum::GetValueAsString(PoolCharType), *SpawnPoint->GetName());

    return true;
}

void UPACS_NPCSpawnManager::LoadSpawnConfiguration()
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode())
    {
        return;
    }

    // Get configuration from GameMode
    if (APACSGameMode* GameMode = Cast<APACSGameMode>(World->GetAuthGameMode()))
    {
        SpawnConfiguration = GameMode->GetSpawnConfiguration();
        if (SpawnConfiguration)
        {
            UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: Loaded spawn configuration from GameMode"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: No spawn configuration available in GameMode"));
        }
    }
}

void UPACS_NPCSpawnManager::CacheSpawnPoints()
{
    if (bSpawnPointsCached)
    {
        return;
    }

    CachedSpawnPoints.Empty();

    // Cache all spawn points for performance
    for (TActorIterator<APACS_NPCSpawnPoint> It(GetWorld()); It; ++It)
    {
        if (APACS_NPCSpawnPoint* SpawnPoint = *It)
        {
            CachedSpawnPoints.Add(SpawnPoint);
        }
    }

    bSpawnPointsCached = true;
    UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: Cached %d spawn points"), CachedSpawnPoints.Num());
}

EPACSCharacterType UPACS_NPCSpawnManager::GetCharacterTypeForSpawnPoint(APACS_NPCSpawnPoint* SpawnPoint) const
{
    if (!SpawnPoint)
    {
        return EPACSCharacterType::LightweightCivilian;
    }

    // For now, directly use the spawn point's character type
    // The data asset configuration will determine which blueprint to use
    return SpawnPoint->CharacterType;
}

void UPACS_NPCSpawnManager::SpawnAllNPCsAsync()
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: SpawnAllNPCsAsync called on client - ignoring"));
        return;
    }

    if (bAsyncSpawningActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_NPCSpawnManager: Async spawning already in progress"));
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
        UE_LOG(LogTemp, Error, TEXT("PACS_NPCSpawnManager: Character pool not available for async spawning"));
        return;
    }

    // Load configuration if not already loaded
    if (!SpawnConfiguration)
    {
        LoadSpawnConfiguration();
    }

    // Cache spawn points for performance
    CacheSpawnPoints();

    // Preload character assets
    CharacterPool->PreloadCharacterAssets();

    // Reset spawn index and start async spawning
    CurrentSpawnIndex = 0;
    bAsyncSpawningActive = true;

    float SpawnDelay = SpawnConfiguration ? SpawnConfiguration->SpawnDelayBetweenBatches : 0.1f;

    World->GetTimerManager().SetTimer(
        AsyncSpawnTimerHandle,
        this,
        &UPACS_NPCSpawnManager::SpawnNextBatch,
        SpawnDelay,
        true
    );

    UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: Started async spawning with %d spawn points"), CachedSpawnPoints.Num());
}

void UPACS_NPCSpawnManager::SpawnNextBatch()
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode() || !bAsyncSpawningActive)
    {
        return;
    }

    TArray<APACS_NPCSpawnPoint*> SpawnPoints = GetAllSpawnPoints();
    if (CurrentSpawnIndex >= SpawnPoints.Num())
    {
        // All spawn points processed, stop async spawning
        bAsyncSpawningActive = false;
        GetWorld()->GetTimerManager().ClearTimer(AsyncSpawnTimerHandle);

        UE_LOG(LogTemp, Log, TEXT("PACS_NPCSpawnManager: Async spawning completed. Total spawned: %d"), SpawnedCharacters.Num());
        return;
    }

    int32 MaxSpawnsThisBatch = SpawnConfiguration ? SpawnConfiguration->MaxSpawnsPerTick : 5;
    int32 SpawnsThisBatch = 0;
    int32 SuccessCount = 0;

    // Spawn batch of NPCs
    while (CurrentSpawnIndex < SpawnPoints.Num() && SpawnsThisBatch < MaxSpawnsThisBatch)
    {
        if (SpawnNPCAtPoint(SpawnPoints[CurrentSpawnIndex]))
        {
            SuccessCount++;
        }

        CurrentSpawnIndex++;
        SpawnsThisBatch++;
    }

    UE_LOG(LogTemp, Verbose, TEXT("PACS_NPCSpawnManager: Batch spawn - %d/%d successful (Progress: %d/%d)"),
        SuccessCount, SpawnsThisBatch, CurrentSpawnIndex, SpawnPoints.Num());
}