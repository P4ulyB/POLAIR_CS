// Copyright POLAIR_CS Project. All Rights Reserved.

#include "Systems/PACS_CharacterPool.h"
#include "Pawns/NPC/PACS_NPCCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StreamableManager.h"
#include "UObject/SoftObjectPtr.h"
#include "AIController.h"

void UPACS_CharacterPool::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Initialize pool configurations with default values
    for (uint8 i = 0; i < (uint8)EPACSCharacterType::MAX; ++i)
    {
        EPACSCharacterType CharType = (EPACSCharacterType)i;
        FPACSCharacterPoolConfig Config;
        Config.InitialPoolSize = 10;
        Config.MaxPoolSize = 50;

        FString ClassPath;

        // Set default character class paths based on type
        // Note: /Game/ maps to the Content folder in Unreal
        switch (CharType)
        {
            case EPACSCharacterType::Civilian:
                ClassPath = TEXT("/Game/Blueprints/NPCs/BP_NPC_Civilian.BP_NPC_Civilian_C");
                Config.CharacterClass = TSoftClassPtr<APACS_NPCCharacter>(FSoftObjectPath(ClassPath));
                break;
            case EPACSCharacterType::Police:
                ClassPath = TEXT("/Game/Blueprints/NPCs/BP_NPC_Police.BP_NPC_Police_C");
                Config.CharacterClass = TSoftClassPtr<APACS_NPCCharacter>(FSoftObjectPath(ClassPath));
                break;
            case EPACSCharacterType::Firefighter:
                ClassPath = TEXT("/Game/Blueprints/NPCs/BP_NPC_FireFighter.BP_NPC_FireFighter_C");
                Config.CharacterClass = TSoftClassPtr<APACS_NPCCharacter>(FSoftObjectPath(ClassPath));
                break;
            case EPACSCharacterType::Paramedic:
                ClassPath = TEXT("/Game/Blueprints/NPCs/BP_NPC_Paramedic.BP_NPC_Paramedic_C");
                Config.CharacterClass = TSoftClassPtr<APACS_NPCCharacter>(FSoftObjectPath(ClassPath));
                break;
            default:
                break;
        }

        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Initialize - Setting up %s with path: %s"),
            *UEnum::GetValueAsString(CharType), *ClassPath);

        PoolConfigurations.Add(CharType, Config);
        CharacterPools.Add(CharType, TArray<FPACSPooledCharacter>());
    }

    // Pre-allocate pool arrays to avoid reallocation
    for (auto& Pool : CharacterPools)
    {
        Pool.Value.Reserve(PoolConfigurations[Pool.Key].MaxPoolSize);
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Initialized with %d character types"), PoolConfigurations.Num());
}

void UPACS_CharacterPool::Deinitialize()
{
    // Clean up all pooled characters
    for (auto& PoolPair : CharacterPools)
    {
        for (FPACSPooledCharacter& PooledChar : PoolPair.Value)
        {
            if (PooledChar.Character && IsValid(PooledChar.Character))
            {
                PooledChar.Character->Destroy();
            }
        }
    }

    CharacterPools.Empty();
    SharedMaterialInstances.Empty();
    LoadedMeshes.Empty();
    LoadedMaterials.Empty();

    Super::Deinitialize();
}

void UPACS_CharacterPool::PreloadCharacterAssets()
{
    if (bAssetsPreloaded)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: Assets already preloaded"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] PreloadCharacterAssets - Starting asset loading"));

    const float StartTime = FPlatformTime::Seconds();

    // Collect all assets to load
    TArray<FSoftObjectPath> AssetsToLoad;

    for (const auto& ConfigPair : PoolConfigurations)
    {
        EPACSCharacterType CharType = ConfigPair.Key;
        const FPACSCharacterPoolConfig& Config = ConfigPair.Value;

        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Checking config for type: %s"), *UEnum::GetValueAsString(CharType));

        // Add character class - check both IsValid and path directly
        FSoftObjectPath ClassPath = Config.CharacterClass.ToSoftObjectPath();
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Character class path for %s: %s"),
            *UEnum::GetValueAsString(CharType), *ClassPath.ToString());
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] IsValid: %s, IsNull: %s"),
            Config.CharacterClass.IsValid() ? TEXT("True") : TEXT("False"),
            Config.CharacterClass.IsNull() ? TEXT("True") : TEXT("False"));

        // Use the path directly if it's not empty, regardless of IsValid()
        if (!ClassPath.IsNull() && !ClassPath.ToString().IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Adding class path: %s"), *ClassPath.ToString());
            AssetsToLoad.Add(ClassPath);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[POOL DEBUG] Empty or null character class path for type: %s"), *UEnum::GetValueAsString(CharType));
        }

        // Add mesh variants
        for (const TSoftObjectPtr<USkeletalMesh>& MeshPtr : Config.MeshVariants)
        {
            if (MeshPtr.IsValid())
            {
                AssetsToLoad.Add(MeshPtr.ToSoftObjectPath());
            }
        }

        // Add material variants
        for (const TSoftObjectPtr<UMaterialInterface>& MatPtr : Config.MaterialVariants)
        {
            if (MatPtr.IsValid())
            {
                AssetsToLoad.Add(MatPtr.ToSoftObjectPath());
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Total assets to load: %d"), AssetsToLoad.Num());

    // Synchronous load to eliminate async bottleneck
    TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestSyncLoad(AssetsToLoad);

    if (Handle.IsValid() && Handle->HasLoadCompleted())
    {
        // Cache loaded assets
        for (auto& ConfigPair : PoolConfigurations)
        {
            EPACSCharacterType CharType = ConfigPair.Key;
            FPACSCharacterPoolConfig& Config = ConfigPair.Value;

            // Cache meshes
            TArray<USkeletalMesh*>& MeshArray = LoadedMeshes.FindOrAdd(CharType);
            for (const TSoftObjectPtr<USkeletalMesh>& MeshPtr : Config.MeshVariants)
            {
                if (USkeletalMesh* Mesh = MeshPtr.Get())
                {
                    MeshArray.Add(Mesh);
                }
            }

            // Cache materials
            TArray<UMaterialInterface*>& MatArray = LoadedMaterials.FindOrAdd(CharType);
            for (const TSoftObjectPtr<UMaterialInterface>& MatPtr : Config.MaterialVariants)
            {
                if (UMaterialInterface* Mat = MatPtr.Get())
                {
                    MatArray.Add(Mat);
                }
            }
        }

        // Create shared material instances
        CreateSharedMaterialInstances();

        bAssetsPreloaded = true;
        LastPreloadTime = FPlatformTime::Seconds() - StartTime;

        UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Preloaded %d assets in %.2fms (eliminated 972ms async bottleneck)"),
            AssetsToLoad.Num(), LastPreloadTime * 1000.0f);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: Failed to preload character assets"));
    }
}

void UPACS_CharacterPool::CreateSharedMaterialInstances()
{
    // Create shared material instances to reduce memory footprint
    for (const auto& MatPair : LoadedMaterials)
    {
        EPACSCharacterType CharType = MatPair.Key;
        const TArray<UMaterialInterface*>& Materials = MatPair.Value;

        for (int32 i = 0; i < Materials.Num(); ++i)
        {
            if (UMaterialInterface* BaseMat = Materials[i])
            {
                FName InstanceName = FName(*FString::Printf(TEXT("SharedMat_%s_%d"),
                    *UEnum::GetValueAsString(CharType), i));

                if (!SharedMaterialInstances.Contains(InstanceName))
                {
                    if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this))
                    {
                        SharedMaterialInstances.Add(InstanceName, MID);
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Created %d shared material instances"), SharedMaterialInstances.Num());
}

APACS_NPCCharacter* UPACS_CharacterPool::AcquireCharacter(EPACSCharacterType CharacterType, UWorld* WorldContext)
{
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: AcquireCharacter called with null WorldContext"));
        return nullptr;
    }

    // Ensure assets are preloaded
    if (!bAssetsPreloaded)
    {
        PreloadCharacterAssets();
    }

    // Find available character in pool
    if (TArray<FPACSPooledCharacter>* Pool = CharacterPools.Find(CharacterType))
    {
        for (FPACSPooledCharacter& PooledChar : *Pool)
        {
            if (!PooledChar.bInUse && IsValid(PooledChar.Character))
            {
                PooledChar.bInUse = true;
                ResetCharacterState(PooledChar.Character);
                TotalCharactersReused++;

                UE_LOG(LogTemp, Verbose, TEXT("PACS_CharacterPool: Reused character from pool (Type: %s)"),
                    *UEnum::GetValueAsString(CharacterType));

                return PooledChar.Character;
            }
        }

        // No available character, spawn new one if under max
        const FPACSCharacterPoolConfig* Config = PoolConfigurations.Find(CharacterType);
        if (Config && Pool->Num() < Config->MaxPoolSize)
        {
            if (APACS_NPCCharacter* NewChar = SpawnPooledCharacter(CharacterType, WorldContext))
            {
                FPACSPooledCharacter PooledChar;
                PooledChar.Character = NewChar;
                PooledChar.bInUse = true;
                PooledChar.CharacterType = CharacterType;
                Pool->Add(PooledChar);

                TotalCharactersCreated++;

                UE_LOG(LogTemp, Verbose, TEXT("PACS_CharacterPool: Created new character for pool (Type: %s, Pool Size: %d)"),
                    *UEnum::GetValueAsString(CharacterType), Pool->Num());

                return NewChar;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: Max pool size reached for type %s"),
                *UEnum::GetValueAsString(CharacterType));
        }
    }

    return nullptr;
}

void UPACS_CharacterPool::ReleaseCharacter(APACS_NPCCharacter* Character)
{
    if (!Character)
    {
        return;
    }

    // Find character in pools and mark as available
    for (auto& PoolPair : CharacterPools)
    {
        for (FPACSPooledCharacter& PooledChar : PoolPair.Value)
        {
            if (PooledChar.Character == Character)
            {
                PooledChar.bInUse = false;

                // Hide character and move to storage location
                Character->SetActorHiddenInGame(true);
                Character->SetActorEnableCollision(false);
                Character->SetActorLocation(FVector(0, 0, -10000)); // Move below world

                UE_LOG(LogTemp, Verbose, TEXT("PACS_CharacterPool: Released character back to pool"));
                return;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: Attempted to release character not in pool"));
}

void UPACS_CharacterPool::GetPoolStatistics(int32& OutTotalPooled, int32& OutInUse, int32& OutAvailable) const
{
    OutTotalPooled = 0;
    OutInUse = 0;
    OutAvailable = 0;

    for (const auto& PoolPair : CharacterPools)
    {
        for (const FPACSPooledCharacter& PooledChar : PoolPair.Value)
        {
            OutTotalPooled++;
            if (PooledChar.bInUse)
            {
                OutInUse++;
            }
            else
            {
                OutAvailable++;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool Stats - Total: %d, InUse: %d, Available: %d, Reuse Rate: %.1f%%"),
        OutTotalPooled, OutInUse, OutAvailable,
        TotalCharactersCreated > 0 ? (float)TotalCharactersReused / (float)(TotalCharactersCreated + TotalCharactersReused) * 100.0f : 0.0f);
}

void UPACS_CharacterPool::WarmUpPool(EPACSCharacterType CharacterType, int32 Count)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: No valid world for warm up"));
        return;
    }

    // Ensure assets are preloaded
    if (!bAssetsPreloaded)
    {
        PreloadCharacterAssets();
    }

    TArray<FPACSPooledCharacter>* Pool = CharacterPools.Find(CharacterType);
    const FPACSCharacterPoolConfig* Config = PoolConfigurations.Find(CharacterType);

    if (!Pool || !Config)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: Invalid character type for warm up"));
        return;
    }

    const int32 CurrentSize = Pool->Num();
    const int32 TargetSize = FMath::Min(CurrentSize + Count, Config->MaxPoolSize);
    const int32 ToCreate = TargetSize - CurrentSize;

    if (ToCreate <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: Pool already at capacity for type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return;
    }

    const float StartTime = FPlatformTime::Seconds();

    for (int32 i = 0; i < ToCreate; ++i)
    {
        if (APACS_NPCCharacter* NewChar = SpawnPooledCharacter(CharacterType, World))
        {
            FPACSPooledCharacter PooledChar;
            PooledChar.Character = NewChar;
            PooledChar.bInUse = false;
            PooledChar.CharacterType = CharacterType;
            Pool->Add(PooledChar);

            // Immediately hide for pool storage
            NewChar->SetActorHiddenInGame(true);
            NewChar->SetActorEnableCollision(false);
            NewChar->SetActorLocation(FVector(0, 0, -10000));

            TotalCharactersCreated++;
        }
    }

    const float ElapsedTime = FPlatformTime::Seconds() - StartTime;
    UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Warmed up pool with %d characters in %.2fms (Type: %s)"),
        ToCreate, ElapsedTime * 1000.0f, *UEnum::GetValueAsString(CharacterType));
}

APACS_NPCCharacter* UPACS_CharacterPool::SpawnPooledCharacter(EPACSCharacterType CharacterType, UWorld* WorldContext)
{
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Error, TEXT("[POOL DEBUG] SpawnPooledCharacter - No WorldContext"));
        return nullptr;
    }

    UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] SpawnPooledCharacter - Type: %s"), *UEnum::GetValueAsString(CharacterType));

    const FPACSCharacterPoolConfig* Config = PoolConfigurations.Find(CharacterType);
    if (!Config || !Config->CharacterClass.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: No valid class for character type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return nullptr;
    }

    // Load class if needed
    UClass* CharClass = Config->CharacterClass.Get();
    if (!CharClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Loading character class synchronously..."));
        CharClass = Config->CharacterClass.LoadSynchronous();
    }

    if (!CharClass)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: Failed to load character class"));
        return nullptr;
    }

    UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Spawning character of class: %s"), *CharClass->GetName());

    // Spawn character
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APACS_NPCCharacter* NewChar = WorldContext->SpawnActor<APACS_NPCCharacter>(CharClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

    if (NewChar)
    {
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Successfully spawned character: %s"), *NewChar->GetName());

        // Debug: Check AI controller setup immediately after spawn
        AController* Controller = NewChar->GetController();
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Post-spawn controller: %s"), Controller ? *Controller->GetName() : TEXT("NULL"));
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Post-spawn AIControllerClass: %s"), NewChar->AIControllerClass ? *NewChar->AIControllerClass->GetName() : TEXT("NULL"));
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Post-spawn AutoPossessAI: %d"), (int32)NewChar->AutoPossessAI);

        // Configure with shared assets
        ConfigureCharacterAssets(NewChar, CharacterType);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[POOL DEBUG] FAILED to spawn character"));
    }

    return NewChar;
}

void UPACS_CharacterPool::ResetCharacterState(APACS_NPCCharacter* Character)
{
    if (!Character)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] ResetCharacterState for: %s"), *Character->GetName());

    // Debug: Check controller state before reset
    AController* Controller = Character->GetController();
    UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Pre-reset controller: %s"), Controller ? *Controller->GetName() : TEXT("NULL"));

    // Reset visibility and collision
    Character->SetActorHiddenInGame(false);
    Character->SetActorEnableCollision(true);

    // Reset transform
    Character->SetActorLocation(FVector::ZeroVector);
    Character->SetActorRotation(FRotator::ZeroRotator);

    // Clear selection state
    Character->CurrentSelector = nullptr;

    // CRITICAL FIX: Ensure AI controller is possessed when character is reused
    Controller = Character->GetController();
    if (!Controller && Character->AIControllerClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] No controller found - forcing AI possession"));

        // Force spawn and possess AI controller
        UWorld* World = Character->GetWorld();
        if (World)
        {
            // Spawn AI controller manually
            AAIController* AIController = World->SpawnActor<AAIController>(Character->AIControllerClass);
            if (AIController)
            {
                // Possess the character
                AIController->Possess(Character);
                UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Successfully created and possessed AI controller: %s"), *AIController->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[POOL DEBUG] Failed to spawn AI controller"));
            }
        }
    }

    // Debug: Check if controller is valid after reset and fix
    Controller = Character->GetController();
    UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Post-reset controller: %s"), Controller ? *Controller->GetName() : TEXT("NULL"));
    if (Controller)
    {
        UE_LOG(LogTemp, Warning, TEXT("[POOL DEBUG] Controller pawn: %s"), Controller->GetPawn() ? *Controller->GetPawn()->GetName() : TEXT("NULL"));
    }
}

void UPACS_CharacterPool::ConfigureCharacterAssets(APACS_NPCCharacter* Character, EPACSCharacterType CharacterType)
{
    if (!Character)
    {
        return;
    }

    // Get cached assets for this character type
    const TArray<USkeletalMesh*>* Meshes = LoadedMeshes.Find(CharacterType);

    // Select random mesh variant if available
    if (Meshes && Meshes->Num() > 0)
    {
        int32 MeshIndex = FMath::RandRange(0, Meshes->Num() - 1);
        USkeletalMesh* SelectedMesh = (*Meshes)[MeshIndex];

        if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
        {
            MeshComp->SetSkeletalMesh(SelectedMesh);
        }
    }

    // Apply shared material instances
    TArray<UMaterialInstanceDynamic*> CharMaterials;
    for (const auto& MatPair : SharedMaterialInstances)
    {
        if (MatPair.Key.ToString().Contains(UEnum::GetValueAsString(CharacterType)))
        {
            CharMaterials.Add(MatPair.Value);
        }
    }

    if (CharMaterials.Num() > 0 && Character->GetMesh())
    {
        for (int32 i = 0; i < CharMaterials.Num() && i < Character->GetMesh()->GetNumMaterials(); ++i)
        {
            Character->GetMesh()->SetMaterial(i, CharMaterials[i]);
        }
    }
}