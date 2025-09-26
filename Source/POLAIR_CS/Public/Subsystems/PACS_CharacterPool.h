// Copyright POLAIR_CS Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/StreamableManager.h"
#include "PACS_CharacterPool.generated.h"

class APACS_NPCCharacter;
class APACS_NPC_Humanoid;
class UMaterialInstanceDynamic;
class UPACS_NPCConfig;
class UPACS_NPC_v2_Config;

/**
 * Character type identifier for pooling system
 */
UENUM(BlueprintType)
enum class EPACSCharacterType : uint8
{
    Civilian = 0,
    Police,
    Firefighter,
    Paramedic,
    // Lightweight NPCs
    LightweightCivilian,
    LightweightPolice,
    LightweightFirefighter,
    LightweightParamedic,
    MAX UMETA(Hidden)
};

/**
 * Pooled character instance with state tracking
 * Now supports both heavyweight and lightweight NPCs
 */
USTRUCT()
struct FPACSPooledCharacter
{
    GENERATED_BODY()

    // Either heavyweight or lightweight NPC (only one will be valid)
    UPROPERTY()
    APACS_NPCCharacter* Character = nullptr;

    UPROPERTY()
    APACS_NPC_Humanoid* LightweightCharacter = nullptr;

    UPROPERTY()
    bool bInUse = false;

    UPROPERTY()
    EPACSCharacterType CharacterType = EPACSCharacterType::Civilian;

    // Helper to get as interface
    class APawn* GetPawn() const;
    class IPACS_SelectableCharacterInterface* GetSelectableInterface() const;

    FPACSPooledCharacter() = default;
};

/**
 * Character pool configuration per type
 */
USTRUCT()
struct FPACSCharacterPoolConfig
{
    GENERATED_BODY()

    UPROPERTY()
    int32 InitialPoolSize = 10;

    UPROPERTY()
    int32 MaxPoolSize = 50;

    // Support both heavyweight and lightweight classes
    UPROPERTY()
    TSoftClassPtr<APACS_NPCCharacter> CharacterClass;

    UPROPERTY()
    TSoftClassPtr<APACS_NPC_Humanoid> LightweightCharacterClass;

    UPROPERTY()
    TArray<TSoftObjectPtr<USkeletalMesh>> MeshVariants;

    UPROPERTY()
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialVariants;

    FPACSCharacterPoolConfig() = default;
};

/**
 * Character Asset Pooling System
 * Eliminates async loading bottlenecks by pre-loading and pooling character assets
 * Reduces memory footprint through shared material instances
 */
UCLASS()
class POLAIR_CS_API UPACS_CharacterPool : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // UGameInstanceSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    /**
     * Configure pool from spawn configuration data asset
     * Must be called before PreloadCharacterAssets
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|CharacterPool")
    void ConfigureFromDataAsset(class UPACS_SpawnConfiguration* SpawnConfig);

    /**
     * Pre-load all character assets at level start
     * Eliminates 972ms WaitForTasks bottleneck identified in performance analysis
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|CharacterPool")
    void PreloadCharacterAssets();

    /**
     * Acquire a character from the pool
     * @param CharacterType Type of character to acquire
     * @param WorldContext World to spawn in if pool is empty
     * @return Pooled character instance or nullptr if unavailable
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|CharacterPool", meta = (CallInEditor = "true"))
    APACS_NPCCharacter* AcquireCharacter(EPACSCharacterType CharacterType, UWorld* WorldContext);

    /**
     * Acquire a lightweight character from the pool
     * @param CharacterType Type of character to acquire (should be Lightweight*)
     * @param WorldContext World to spawn in if pool is empty
     * @return Pooled lightweight character or nullptr if unavailable
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|CharacterPool")
    APACS_NPC_Humanoid* AcquireLightweightCharacter(EPACSCharacterType CharacterType, UWorld* WorldContext);

    /**
     * Release a character back to the pool
     * @param Character Character to release
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|CharacterPool")
    void ReleaseCharacter(APACS_NPCCharacter* Character);

    /**
     * Release a lightweight character back to the pool
     * @param Character Lightweight character to release
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|CharacterPool")
    void ReleaseLightweightCharacter(APACS_NPC_Humanoid* Character);

    /**
     * Get current pool statistics for monitoring
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|CharacterPool")
    void GetPoolStatistics(int32& OutTotalPooled, int32& OutInUse, int32& OutAvailable) const;

    /**
     * Warm up the pool by pre-creating characters
     * @param CharacterType Type to warm up
     * @param Count Number to pre-create
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|CharacterPool")
    void WarmUpPool(EPACSCharacterType CharacterType, int32 Count);

protected:
    /**
     * Create shared material instances for all characters
     * Reduces memory from 1.15MB to <1MB per character
     */
    void CreateSharedMaterialInstances();

    /**
     * Spawn a new character for the pool
     */
    APACS_NPCCharacter* SpawnPooledCharacter(EPACSCharacterType CharacterType, UWorld* WorldContext);

    /**
     * Spawn a new lightweight character for the pool
     */
    APACS_NPC_Humanoid* SpawnLightweightCharacter(EPACSCharacterType CharacterType, UWorld* WorldContext);

    /**
     * Reset character state for reuse
     */
    void ResetCharacterState(APACS_NPCCharacter* Character);
    void ResetLightweightCharacterState(APACS_NPC_Humanoid* Character);

    /**
     * Configure character with shared assets
     */
    void ConfigureCharacterAssets(APACS_NPCCharacter* Character, EPACSCharacterType CharacterType);
    void ConfigureLightweightCharacterAssets(APACS_NPC_Humanoid* Character, EPACSCharacterType CharacterType);

private:
    // Character pools organised by type - using non-UPROPERTY for complex containers
    TMap<EPACSCharacterType, TArray<FPACSPooledCharacter>> CharacterPools;

    // Pool configuration per character type
    TMap<EPACSCharacterType, FPACSCharacterPoolConfig> PoolConfigurations;

    // NPCConfig assets per character type
    TMap<EPACSCharacterType, UPACS_NPCConfig*> NPCConfigurations;

    // Lightweight NPCConfig assets per character type
    TMap<EPACSCharacterType, UPACS_NPC_v2_Config*> LightweightNPCConfigurations;

    // Shared material instances to reduce memory footprint
    UPROPERTY()
    TMap<FName, UMaterialInstanceDynamic*> SharedMaterialInstances;

    // Pre-loaded mesh assets - using non-UPROPERTY for complex containers
    TMap<EPACSCharacterType, TArray<USkeletalMesh*>> LoadedMeshes;

    // Pre-loaded material assets - using non-UPROPERTY for complex containers
    TMap<EPACSCharacterType, TArray<UMaterialInterface*>> LoadedMaterials;

    // Streamable manager for asset loading
    FStreamableManager StreamableManager;

    // Track loading state
    bool bAssetsPreloaded = false;

    // Performance metrics
    float LastPreloadTime = 0.0f;
    int32 TotalCharactersCreated = 0;
    int32 TotalCharactersReused = 0;
};