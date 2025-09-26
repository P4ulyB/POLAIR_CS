// Copyright POLAIR_CS Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/PACS_CharacterPool.h"
#include "PACS_SpawnConfiguration.generated.h"

/**
 * Character pool configuration entry with blueprint reference
 * Provides data-driven character type configuration
 */
USTRUCT(BlueprintType)
struct FPACS_CharacterPoolEntry
{
	GENERATED_BODY()

	// Direct blueprint class reference - supports both heavyweight and lightweight NPCs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character",
		meta = (DisplayName = "Character Blueprint", AllowAbstract = "false"))
	TSoftClassPtr<APawn> CharacterBlueprint;

	// Pool type assignment for this blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool",
		meta = (DisplayName = "Pool Type"))
	EPACSCharacterType PoolType = EPACSCharacterType::LightweightCivilian;

	// Whether this pool entry is enabled globally
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool",
		meta = (DisplayName = "Enabled"))
	bool bEnabled = true;

	// Pool size configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool Size",
		meta = (ClampMin = "1", ClampMax = "100", DisplayName = "Initial Pool Size"))
	int32 InitialPoolSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool Size",
		meta = (ClampMin = "1", ClampMax = "500", DisplayName = "Maximum Pool Size"))
	int32 MaxPoolSize = 50;

	// Optional: Associated NPCConfig or lightweight config asset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character",
		meta = (DisplayName = "NPC Configuration Asset"))
	TSoftObjectPtr<UDataAsset> NPCConfigAsset;
};

/**
 * Spawn zone configuration for different areas of the level
 */
USTRUCT(BlueprintType)
struct FPACS_SpawnZoneConfig
{
	GENERATED_BODY()

	// Zone identifier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	FString ZoneName;

	// Maximum NPCs allowed in this zone
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (ClampMin = "0"))
	int32 MaxNPCs = 50;

	// Character types allowed in this zone
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	TArray<EPACSCharacterType> AllowedTypes;

	// Priority for spawning (higher = spawns first)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (ClampMin = "0"))
	int32 SpawnPriority = 1;
};

/**
 * Server-authoritative spawn configuration data asset
 * Defines character type mappings, spawn limits, and zone configurations
 */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SpawnConfiguration : public UDataAsset
{
	GENERATED_BODY()

public:
	UPACS_SpawnConfiguration();

	/**
	 * Character pool entries - data-driven blueprint configuration
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Pools", meta = (DisplayName = "Character Pool Entries"))
	TArray<FPACS_CharacterPoolEntry> CharacterPoolEntries;

	/**
	 * Global spawn limits
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Limits", meta = (ClampMin = "0"))
	int32 MaxTotalNPCs = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Limits", meta = (ClampMin = "0"))
	int32 MaxNPCsPerType = 100;

	/**
	 * Performance settings
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	bool bEnableAsyncSpawning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "1"))
	int32 MaxSpawnsPerTick = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "0.0"))
	float SpawnDelayBetweenBatches = 0.1f;

	/**
	 * Zone-based spawning configuration (future use)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zones", meta = (DisplayName = "Spawn Zones"))
	TArray<FPACS_SpawnZoneConfig> SpawnZones;

	/**
	 * Authority validation
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Authority")
	bool bRequireServerAuthority = true;

	/**
	 * Get the character blueprint for a given pool type
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Configuration")
	TSoftClassPtr<APawn> GetCharacterBlueprintForType(EPACSCharacterType PoolType) const;

	/**
	 * Get pool configuration settings for a given type
	 * Returns true if found, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Configuration", meta = (DisplayName = "Get Pool Settings"))
	bool GetPoolSettingsForType(EPACSCharacterType PoolType,
		UPARAM(ref) int32& OutInitialPoolSize,
		UPARAM(ref) int32& OutMaxPoolSize) const;

	/**
	 * Check if spawning is allowed for a character type
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Configuration")
	bool IsSpawningAllowed(EPACSCharacterType CharacterType, int32 CurrentCount) const;

	/**
	 * Get maximum allowed NPCs for a specific type
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Configuration")
	int32 GetMaxNPCsForType(EPACSCharacterType CharacterType) const;

	/**
	 * Validate configuration settings
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Configuration")
	bool ValidateConfiguration(FString& OutErrorMessage) const;

protected:
	/**
	 * Initialize default character type mappings
	 */
	void InitializeDefaultMappings();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};