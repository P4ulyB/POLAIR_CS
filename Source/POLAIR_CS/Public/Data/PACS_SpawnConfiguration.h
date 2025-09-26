// Copyright POLAIR_CS Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/PACS_CharacterPool.h"
#include "PACS_SpawnConfiguration.generated.h"

/**
 * Configuration for character type spawning mapping
 * Replaces hard-coded type conversion logic with data-driven approach
 */
USTRUCT(BlueprintType)
struct FPACS_CharacterTypeMapping
{
	GENERATED_BODY()

	// The legacy spawn point character type
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EPACSCharacterType LegacyType = EPACSCharacterType::Civilian;

	// The pool character type to actually spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EPACSCharacterType PoolType = EPACSCharacterType::LightweightCivilian;

	// Whether this mapping is enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	bool bEnabled = true;
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
	 * Character type mappings - replaces hard-coded conversion logic
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Types", meta = (DisplayName = "Type Mappings"))
	TArray<FPACS_CharacterTypeMapping> CharacterTypeMappings;

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
	 * Get the pool character type for a given legacy type
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Configuration")
	EPACSCharacterType GetPoolTypeForLegacyType(EPACSCharacterType LegacyType) const;

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