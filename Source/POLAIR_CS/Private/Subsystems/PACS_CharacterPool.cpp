// Copyright POLAIR_CS Project. All Rights Reserved.

#include "Subsystems/PACS_CharacterPool.h"
#include "Actors/NPC/PACS_NPCCharacter.h"
#include "Actors/NPC/PACS_NPC_Humanoid.h"
#include "Data/Configs/PACS_NPCConfig.h"
#include "Data/Configs/PACS_NPC_v2_Config.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Blueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
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
            // Lightweight NPCs - using actual Blueprint names
            case EPACSCharacterType::LightweightCivilian:
                ClassPath = TEXT("/Game/Blueprints/NPCs/Lightweight/BP_NPC_LightweightCivilian.BP_NPC_LightweightCivilian_C");
                Config.LightweightCharacterClass = TSoftClassPtr<APACS_NPC_Humanoid>(FSoftObjectPath(ClassPath));
                break;
            case EPACSCharacterType::LightweightPolice:
                ClassPath = TEXT("/Game/Blueprints/NPCs/Lightweight/BP_NPC_LightweightPolice.BP_NPC_LightweightPolice_C");
                Config.LightweightCharacterClass = TSoftClassPtr<APACS_NPC_Humanoid>(FSoftObjectPath(ClassPath));
                break;
            case EPACSCharacterType::LightweightFirefighter:
                ClassPath = TEXT("/Game/Blueprints/NPCs/Lightweight/BP_NPC_LightweightFireFighter.BP_NPC_LightweightFireFighter_C");
                Config.LightweightCharacterClass = TSoftClassPtr<APACS_NPC_Humanoid>(FSoftObjectPath(ClassPath));
                break;
            case EPACSCharacterType::LightweightParamedic:
                ClassPath = TEXT("/Game/Blueprints/NPCs/Lightweight/BP_NPC_LightweightParamedic.BP_NPC_LightweightParamedic_C");
                Config.LightweightCharacterClass = TSoftClassPtr<APACS_NPC_Humanoid>(FSoftObjectPath(ClassPath));
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
        return;
    }

    const float StartTime = FPlatformTime::Seconds();

    // Collect ALL assets to load - including from NPCConfig assets
    TArray<FSoftObjectPath> AssetsToLoad;
    TMap<EPACSCharacterType, TArray<FSoftObjectPath>> AssetPathsByType;

    // First, load the character blueprint classes to get NPCConfig references
    for (const auto& ConfigPair : PoolConfigurations)
    {
        EPACSCharacterType CharType = ConfigPair.Key;
        const FPACSCharacterPoolConfig& Config = ConfigPair.Value;

        FSoftObjectPath ClassPath = Config.CharacterClass.ToSoftObjectPath();
        if (!ClassPath.IsNull() && !ClassPath.ToString().IsEmpty())
        {
            AssetsToLoad.Add(ClassPath);
        }
    }

    // Do an initial sync load of the character classes
    TSharedPtr<FStreamableHandle> ClassHandle = StreamableManager.RequestSyncLoad(AssetsToLoad);

    if (!ClassHandle.IsValid() || !ClassHandle->HasLoadCompleted())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: Failed to load character classes"));
        return;
    }

    // Now extract NPCConfig assets from the loaded blueprints and collect ALL required assets
    AssetsToLoad.Empty();

    for (const auto& ConfigPair : PoolConfigurations)
    {
        EPACSCharacterType CharType = ConfigPair.Key;
        const FPACSCharacterPoolConfig& Config = ConfigPair.Value;

        // Get the loaded character class
        if (UClass* CharClass = Config.CharacterClass.Get())
        {
            // Check the CDO for NPCConfig assets
            if (APACS_NPCCharacter* CDO = Cast<APACS_NPCCharacter>(CharClass->GetDefaultObject()))
            {
                if (UPACS_NPCConfig* NPCConfig = CDO->GetNPCConfigAsset())
                {
                    // Extract asset paths from NPCConfig
                    TArray<FSoftObjectPath>& TypeAssets = AssetPathsByType.FindOrAdd(CharType);

                    if (!NPCConfig->SkeletalMesh.IsNull())
                    {
                        TypeAssets.Add(NPCConfig->SkeletalMesh.ToSoftObjectPath());
                        AssetsToLoad.Add(NPCConfig->SkeletalMesh.ToSoftObjectPath());
                    }

                    if (!NPCConfig->AnimClass.IsNull())
                    {
                        TypeAssets.Add(NPCConfig->AnimClass.ToSoftObjectPath());
                        AssetsToLoad.Add(NPCConfig->AnimClass.ToSoftObjectPath());
                    }

                    if (!NPCConfig->DecalMaterial.IsNull())
                    {
                        TypeAssets.Add(NPCConfig->DecalMaterial.ToSoftObjectPath());
                        AssetsToLoad.Add(NPCConfig->DecalMaterial.ToSoftObjectPath());
                    }

                    // Store the NPCConfig for later use
                    NPCConfigurations.Add(CharType, NPCConfig);
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Loading %d unique assets for all character types"), AssetsToLoad.Num());

    // Synchronous load ALL assets at once to eliminate async bottleneck
    TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestSyncLoad(AssetsToLoad);

    if (Handle.IsValid() && Handle->HasLoadCompleted())
    {
        // Cache loaded assets by type for quick access
        for (const auto& AssetPair : AssetPathsByType)
        {
            EPACSCharacterType CharType = AssetPair.Key;

            if (UPACS_NPCConfig* NPCConfig = NPCConfigurations.FindRef(CharType))
            {
                // Cache the loaded mesh
                if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(NPCConfig->SkeletalMesh.Get()))
                {
                    TArray<USkeletalMesh*>& MeshArray = LoadedMeshes.FindOrAdd(CharType);
                    MeshArray.Add(Mesh);
                }

                // Cache the loaded material
                if (UMaterialInterface* DecalMat = Cast<UMaterialInterface>(NPCConfig->DecalMaterial.Get()))
                {
                    TArray<UMaterialInterface*>& MatArray = LoadedMaterials.FindOrAdd(CharType);
                    MatArray.Add(DecalMat);
                }
            }
        }

        // Create shared material instances
        CreateSharedMaterialInstances();

        bAssetsPreloaded = true;
        LastPreloadTime = FPlatformTime::Seconds() - StartTime;

        UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Successfully preloaded all assets in %.2fms"), LastPreloadTime * 1000.0f);

        // Log performance metrics
        int32 TotalMeshes = 0;
        int32 TotalMaterials = 0;
        for (const auto& MeshPair : LoadedMeshes)
        {
            TotalMeshes += MeshPair.Value.Num();
        }
        for (const auto& MatPair : LoadedMaterials)
        {
            TotalMaterials += MatPair.Value.Num();
        }

        UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Cached %d meshes, %d materials, %d shared material instances"),
            TotalMeshes, TotalMaterials, SharedMaterialInstances.Num());
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
        return nullptr;
    }

    const FPACSCharacterPoolConfig* Config = PoolConfigurations.Find(CharacterType);
    if (!Config || !Config->CharacterClass.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: No valid class for character type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return nullptr;
    }

    // Assets should already be loaded by PreloadCharacterAssets
    UClass* CharClass = Config->CharacterClass.Get();
    if (!CharClass)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: Character class not loaded for type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return nullptr;
    }

    // Spawn character
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APACS_NPCCharacter* NewChar = WorldContext->SpawnActor<APACS_NPCCharacter>(CharClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

    if (NewChar)
    {
        // CRITICAL: Mark character as pooled to prevent async loading
        NewChar->SetIsPooledCharacter(true);

        // Configure with pre-loaded shared assets immediately
        ConfigureCharacterAssets(NewChar, CharacterType);

        // FIXED: Do not automatically possess AI controllers
        // AI controllers will be possessed only when movement is needed
        // This prevents automatic/autonomous movement behavior
    }

    return NewChar;
}

void UPACS_CharacterPool::ResetCharacterState(APACS_NPCCharacter* Character)
{
    if (!Character)
    {
        return;
    }

    // Reset visibility and collision
    Character->SetActorHiddenInGame(false);
    Character->SetActorEnableCollision(true);

    // Reset transform
    Character->SetActorLocation(FVector::ZeroVector);
    Character->SetActorRotation(FRotator::ZeroRotator);

    // Clear selection state
    Character->CurrentSelector = nullptr;

    // FIXED: Do not automatically possess AI controllers when character is reused
    // Clear any existing AI controller to prevent autonomous movement
    AController* CurrentController = Character->GetController();
    if (CurrentController)
    {
        CurrentController->UnPossess();
    }
}

void UPACS_CharacterPool::ConfigureCharacterAssets(APACS_NPCCharacter* Character, EPACSCharacterType CharacterType)
{
    if (!Character)
    {
        return;
    }

    // Get the NPCConfig for this character type
    UPACS_NPCConfig* NPCConfig = NPCConfigurations.FindRef(CharacterType);
    if (!NPCConfig)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: No NPCConfig found for character type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return;
    }

    // Apply pre-loaded mesh directly (no async loading)
    if (USkeletalMesh* Mesh = NPCConfig->SkeletalMesh.Get())
    {
        if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
        {
            MeshComp->SetSkeletalMesh(Mesh, true);

            // Apply mesh transforms from config
            MeshComp->SetRelativeLocation(NPCConfig->MeshLocation);
            MeshComp->SetRelativeRotation(NPCConfig->MeshRotation);
            MeshComp->SetRelativeScale3D(NPCConfig->MeshScale);

            // Set animation optimizations
            MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
            MeshComp->bEnableUpdateRateOptimizations = true;
        }
    }

    // Apply pre-loaded animation class directly (no async loading)
    if (UClass* AnimClass = NPCConfig->AnimClass.Get())
    {
        if (AnimClass && Character->GetMesh())
        {
            Character->GetMesh()->SetAnimInstanceClass(AnimClass);
        }
    }

    // Apply shared material instance for this character type
    FName MaterialKey = FName(*FString::Printf(TEXT("SharedMat_%s_0"), *UEnum::GetValueAsString(CharacterType)));
    if (UMaterialInstanceDynamic** SharedMatPtr = SharedMaterialInstances.Find(MaterialKey))
    {
        UMaterialInstanceDynamic* SharedMat = *SharedMatPtr;

        // Apply to decal component
        if (UDecalComponent* DecalComp = Character->GetCollisionDecal())
        {
            DecalComp->SetDecalMaterial(SharedMat);
        }

        // Cache the shared material on the character for visual state updates
        Character->SetCachedDecalMaterial(SharedMat);
    }

    // Apply collision settings from config
    if (UBoxComponent* CollisionBox = Character->GetCollisionBox())
    {
        // Get bounds from the skeletal mesh using UE5.5 API
        if (USkeletalMesh* Mesh = Character->GetMesh()->GetSkeletalMeshAsset())
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            const FVector& BoxExtent = Bounds.BoxExtent;
            const float MaxDimension = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);

            // Calculate scale factor from CollisionScaleSteps
            const float ScaleFactor = 1.0f + (0.1f * static_cast<float>(NPCConfig->CollisionScaleSteps));
            const float UniformExtent = MaxDimension * ScaleFactor;

            CollisionBox->SetBoxExtent(FVector(UniformExtent, UniformExtent, UniformExtent), true);
            CollisionBox->SetRelativeLocation(Bounds.Origin);

            // Apply same dimensions to decal
            if (UDecalComponent* DecalComp = Character->GetCollisionDecal())
            {
                DecalComp->DecalSize = FVector(UniformExtent, UniformExtent, UniformExtent);
            }
        }
    }

    // Mark character as fully configured to skip ApplyVisuals_Client
    Character->SetVisualsApplied(true);
}

// Lightweight character methods implementation

APACS_NPC_Humanoid* UPACS_CharacterPool::AcquireLightweightCharacter(EPACSCharacterType CharacterType, UWorld* WorldContext)
{
    // Only handle lightweight types
    if (CharacterType < EPACSCharacterType::LightweightCivilian)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: Non-lightweight type passed to AcquireLightweightCharacter"));
        return nullptr;
    }

    TArray<FPACSPooledCharacter>* Pool = CharacterPools.Find(CharacterType);
    if (!Pool)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: No pool found for lightweight type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return nullptr;
    }

    // Look for available character in pool
    for (FPACSPooledCharacter& PooledChar : *Pool)
    {
        if (!PooledChar.bInUse && PooledChar.LightweightCharacter && IsValid(PooledChar.LightweightCharacter))
        {
            PooledChar.bInUse = true;
            ResetLightweightCharacterState(PooledChar.LightweightCharacter);
            TotalCharactersReused++;

            UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Reused lightweight character from pool (Type: %s)"),
                *UEnum::GetValueAsString(CharacterType));

            return PooledChar.LightweightCharacter;
        }
    }

    // No available character, spawn new one if under max
    FPACSCharacterPoolConfig* Config = PoolConfigurations.Find(CharacterType);
    if (!Config || Pool->Num() >= Config->MaxPoolSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: Pool at max capacity for lightweight type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return nullptr;
    }

    // Spawn new lightweight character
    APACS_NPC_Humanoid* NewChar = SpawnLightweightCharacter(CharacterType, WorldContext);
    if (!NewChar)
    {
        return nullptr;
    }

    // Add to pool
    FPACSPooledCharacter PooledChar;
    PooledChar.LightweightCharacter = NewChar;
    PooledChar.bInUse = true;
    PooledChar.CharacterType = CharacterType;
    Pool->Add(PooledChar);

    TotalCharactersCreated++;

    UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Created new lightweight character (Type: %s, Total: %d)"),
        *UEnum::GetValueAsString(CharacterType), Pool->Num());

    return NewChar;
}

void UPACS_CharacterPool::ReleaseLightweightCharacter(APACS_NPC_Humanoid* Character)
{
    if (!Character || !IsValid(Character))
    {
        return;
    }

    // Find character in all pools
    for (auto& PoolPair : CharacterPools)
    {
        for (FPACSPooledCharacter& PooledChar : PoolPair.Value)
        {
            if (PooledChar.LightweightCharacter == Character)
            {
                PooledChar.bInUse = false;

                // Hide and move to storage location
                Character->SetActorHiddenInGame(true);
                Character->SetActorEnableCollision(false);
                Character->SetActorLocation(FVector(0, 0, -10000));

                UE_LOG(LogTemp, Log, TEXT("PACS_CharacterPool: Released lightweight character back to pool (Type: %s)"),
                    *UEnum::GetValueAsString(PooledChar.CharacterType));

                return;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: Lightweight character not found in pool, destroying"));
    Character->Destroy();
}

APACS_NPC_Humanoid* UPACS_CharacterPool::SpawnLightweightCharacter(EPACSCharacterType CharacterType, UWorld* WorldContext)
{
    if (!WorldContext)
    {
        return nullptr;
    }

    FPACSCharacterPoolConfig* Config = PoolConfigurations.Find(CharacterType);
    if (!Config)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: No configuration for lightweight type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return nullptr;
    }

    // Load lightweight character class
    UClass* CharClass = nullptr;
    if (!Config->LightweightCharacterClass.IsNull())
    {
        CharClass = Config->LightweightCharacterClass.LoadSynchronous();
    }

    // If no Blueprint class configured, use the C++ class directly
    if (!CharClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: No Blueprint class for %s, using base C++ class"),
            *UEnum::GetValueAsString(CharacterType));
        CharClass = APACS_NPC_Humanoid::StaticClass();
    }

    if (!CharClass)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: Failed to get any class for lightweight type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return nullptr;
    }

    // Spawn lightweight character
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APACS_NPC_Humanoid* NewChar = WorldContext->SpawnActor<APACS_NPC_Humanoid>(CharClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

    if (!NewChar)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_CharacterPool: Failed to spawn lightweight character for type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return nullptr;
    }

    // Configure with assets
    ConfigureLightweightCharacterAssets(NewChar, CharacterType);

    return NewChar;
}

void UPACS_CharacterPool::ResetLightweightCharacterState(APACS_NPC_Humanoid* Character)
{
    if (!Character || !IsValid(Character))
    {
        return;
    }

    // Reset basic state
    Character->SetActorHiddenInGame(false);
    Character->SetActorEnableCollision(true);

    // Clear selection state through interface
    if (IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(Character))
    {
        Selectable->SetCurrentSelector(nullptr);
        Selectable->SetLocalHover(false);
    }
}

void UPACS_CharacterPool::ConfigureLightweightCharacterAssets(APACS_NPC_Humanoid* Character, EPACSCharacterType CharacterType)
{
    if (!Character || !IsValid(Character))
    {
        return;
    }

    // Get lightweight config
    UPACS_NPC_v2_Config** ConfigPtr = LightweightNPCConfigurations.Find(CharacterType);
    if (!ConfigPtr || !*ConfigPtr)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_CharacterPool: No lightweight config for type %s"),
            *UEnum::GetValueAsString(CharacterType));
        return;
    }

    // Configuration would be applied through the character's NPCConfig property
    // This is typically set in the Blueprint class defaults
}

// Helper methods for FPACSPooledCharacter
APawn* FPACSPooledCharacter::GetPawn() const
{
    if (Character && IsValid(Character))
    {
        return Character;
    }
    if (LightweightCharacter && IsValid(LightweightCharacter))
    {
        return LightweightCharacter;
    }
    return nullptr;
}

IPACS_SelectableCharacterInterface* FPACSPooledCharacter::GetSelectableInterface() const
{
    if (APawn* Pawn = GetPawn())
    {
        return Cast<IPACS_SelectableCharacterInterface>(Pawn);
    }
    return nullptr;
}