#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PACS_SpawnConfig.generated.h"

/**
 * Pool configuration settings for a spawn type
 */
USTRUCT(BlueprintType)
struct FPoolSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0, ClampMax = 100))
	int32 InitialSize = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1, ClampMax = 500))
	int32 MaxSize = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPrewarmOnStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1, ClampMax = 10))
	int32 ExpandSize = 5;
};

/**
 * Replication policy for spawned actors
 */
USTRUCT(BlueprintType)
struct FReplicationPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bReplicated = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAlwaysRelevant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bReplicated"))
	float NetCullDistanceSquared = 225000000.0f; // 15000 units squared

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bReplicated"))
	TEnumAsByte<ENetDormancy> NetDormancy = DORM_DormantAll;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bReplicated"))
	float NetUpdateFrequency = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bReplicated"))
	float MinNetUpdateFrequency = 2.0f;
};

/**
 * Spawn failure reasons for client feedback
 */
UENUM(BlueprintType)
enum class ESpawnFailureReason : uint8
{
	None = 0,
	PoolExhausted,
	InvalidLocation,
	PlayerLimitReached,
	GlobalLimitReached,
	NotAuthorized,
	SystemNotReady
};

/**
 * Configuration for a single spawnable class type
 */
USTRUCT(BlueprintType)
struct FSpawnClassConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "PACS.Spawn"))
	FGameplayTag SpawnTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MetaClass = "/Script/Engine.Actor"))
	TSoftClassPtr<AActor> ActorClass;

	// Selection profile for visual configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual",
		meta = (ToolTip = "Selection profile defining NPC mesh, materials and selection visuals"))
	TSoftObjectPtr<class UPACS_SelectionProfileAsset> SelectionProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool")
	FPoolSettings PoolSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replication")
	FReplicationPolicy ReplicationPolicy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bSpawnOnServer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bAutoActivate = true;

	// UI Display Information
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI",
		meta = (ToolTip = "Display name shown on spawn button"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI",
		meta = (ToolTip = "Icon texture for spawn button"))
	TSoftObjectPtr<class UTexture2D> ButtonIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI",
		meta = (ToolTip = "Whether this spawn type should appear in the UI"))
	bool bVisibleInUI = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI",
		meta = (ToolTip = "Tooltip description for spawn button"))
	FText TooltipDescription;

	// Spawn Limits
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Max spawns per player (0 = unlimited)"))
	int32 PlayerSpawnLimit = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits",
		meta = (ClampMin = "0", ClampMax = "1000", ToolTip = "Max global spawns of this type (0 = unlimited)"))
	int32 GlobalSpawnLimit = 100;
};

/**
 * Primary configuration data asset for spawn system
 * Maps spawn tags to actor classes with pool and replication settings
 */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SpawnConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPACS_SpawnConfig();

	// Get configuration for a specific spawn tag
	UFUNCTION(BlueprintCallable, Category = "Spawn Config")
	bool GetConfigForTag(FGameplayTag SpawnTag, FSpawnClassConfig& OutConfig) const;

	// Get all configured spawn tags
	UFUNCTION(BlueprintCallable, Category = "Spawn Config")
	TArray<FGameplayTag> GetAllSpawnTags() const;

	// Get all spawn configurations
	const TArray<FSpawnClassConfig>& GetSpawnConfigs() const { return SpawnConfigs; }

	// Validation
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Classes")
	TArray<FSpawnClassConfig> SpawnConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Global Settings")
	bool bEnablePooling = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Global Settings", meta = (EditCondition = "bEnablePooling"))
	int32 GlobalMaxPoolSize = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance")
	int32 ActorsPerFrameWarmup = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance")
	float WarmupDelaySeconds = 0.1f;

private:
	// Internal lookup map (built from array for fast access)
	UPROPERTY(Transient)
	mutable TMap<FGameplayTag, int32> TagToIndexMap;

	void RebuildLookupMap() const;
};