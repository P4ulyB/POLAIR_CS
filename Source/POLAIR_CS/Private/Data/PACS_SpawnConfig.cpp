#include "Data/PACS_SpawnConfig.h"
#include "Core/PACS_GameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UPACS_SpawnConfig::UPACS_SpawnConfig()
{
	// Default constructor
}

bool UPACS_SpawnConfig::GetConfigForTag(FGameplayTag SpawnTag, FSpawnClassConfig& OutConfig) const
{
	// Debug logging
	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::GetConfigForTag - Looking for tag: %s"), *SpawnTag.ToString());
	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::GetConfigForTag - SpawnConfigs count: %d"), SpawnConfigs.Num());
	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::GetConfigForTag - TagToIndexMap count: %d"), TagToIndexMap.Num());

	// Ensure lookup map is built
	if (TagToIndexMap.Num() == 0 || TagToIndexMap.Num() != SpawnConfigs.Num())
	{
		UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::GetConfigForTag - Rebuilding lookup map"));
		RebuildLookupMap();
		UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::GetConfigForTag - After rebuild, TagToIndexMap count: %d"), TagToIndexMap.Num());
	}

	// Log all available tags in the map
	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::GetConfigForTag - Available tags in map:"));
	for (const auto& TagPair : TagToIndexMap)
	{
		UE_LOG(LogTemp, Log, TEXT("  - %s -> Index %d"), *TagPair.Key.ToString(), TagPair.Value);
	}

	// Fast lookup
	const int32* IndexPtr = TagToIndexMap.Find(SpawnTag);
	if (IndexPtr && SpawnConfigs.IsValidIndex(*IndexPtr))
	{
		OutConfig = SpawnConfigs[*IndexPtr];
		UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::GetConfigForTag - Found exact match for %s at index %d"),
			*SpawnTag.ToString(), *IndexPtr);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnConfig::GetConfigForTag - No exact match for %s, checking parent tags"),
		*SpawnTag.ToString());

	// Check parent tags if exact match not found
	FGameplayTag CurrentTag = SpawnTag;
	while (CurrentTag.IsValid())
	{
		const int32* ParentIndexPtr = TagToIndexMap.Find(CurrentTag);
		if (ParentIndexPtr && SpawnConfigs.IsValidIndex(*ParentIndexPtr))
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnConfig: Using parent tag %s for requested tag %s"),
				*CurrentTag.ToString(), *SpawnTag.ToString());
			OutConfig = SpawnConfigs[*ParentIndexPtr];
			return true;
		}

		// Move to parent tag
		CurrentTag = CurrentTag.RequestDirectParent();
	}

	UE_LOG(LogTemp, Error, TEXT("PACS_SpawnConfig::GetConfigForTag - Failed to find config for tag %s"),
		*SpawnTag.ToString());
	return false;
}

TArray<FGameplayTag> UPACS_SpawnConfig::GetAllSpawnTags() const
{
	TArray<FGameplayTag> Tags;
	Tags.Reserve(SpawnConfigs.Num());

	for (const FSpawnClassConfig& Config : SpawnConfigs)
	{
		if (Config.SpawnTag.IsValid())
		{
			Tags.Add(Config.SpawnTag);
		}
	}

	return Tags;
}

#if WITH_EDITOR
void UPACS_SpawnConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Rebuild lookup map when configs change
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPACS_SpawnConfig, SpawnConfigs))
	{
		TagToIndexMap.Empty();
		RebuildLookupMap();
	}
}
#endif

#if WITH_EDITOR
EDataValidationResult UPACS_SpawnConfig::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = Super::IsDataValid(ValidationErrors);

	// Check for duplicate tags
	TSet<FGameplayTag> SeenTags;
	for (const FSpawnClassConfig& Config : SpawnConfigs)
	{
		if (!Config.SpawnTag.IsValid())
		{
			ValidationErrors.Add(FText::FromString("Invalid spawn tag found in configuration"));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		if (SeenTags.Contains(Config.SpawnTag))
		{
			ValidationErrors.Add(FText::Format(
				FText::FromString("Duplicate spawn tag: {0}"),
				FText::FromString(Config.SpawnTag.ToString())
			));
			Result = EDataValidationResult::Invalid;
		}
		SeenTags.Add(Config.SpawnTag);

		// Validate actor class
		if (Config.ActorClass.IsNull())
		{
			ValidationErrors.Add(FText::Format(
				FText::FromString("No actor class specified for tag: {0}"),
				FText::FromString(Config.SpawnTag.ToString())
			));
			Result = EDataValidationResult::Invalid;
		}

		// Validate pool settings
		if (Config.PoolSettings.InitialSize > Config.PoolSettings.MaxSize)
		{
			ValidationErrors.Add(FText::Format(
				FText::FromString("Initial pool size exceeds max size for tag: {0}"),
				FText::FromString(Config.SpawnTag.ToString())
			));
			Result = EDataValidationResult::Invalid;
		}

		// Warn about character pooling
		if (!Config.ActorClass.IsNull())
		{
			FString ClassPath = Config.ActorClass.ToString();
			if (ClassPath.Contains("Character") || ClassPath.Contains("Pawn"))
			{
				ValidationErrors.Add(FText::Format(
					FText::FromString("Warning: Pooling Character/Pawn classes requires comprehensive state reset for tag: {0}"),
					FText::FromString(Config.SpawnTag.ToString())
				));
				// This is a warning, not an error
				if (Result == EDataValidationResult::Valid)
				{
					Result = EDataValidationResult::NotValidated;
				}
			}
		}

		// Check VR settings consistency
		if (Config.VRSettings.bEnableVROptimizations && Config.VRSettings.VRCullDistance > 10000.0f)
		{
			ValidationErrors.Add(FText::Format(
				FText::FromString("Warning: VR cull distance may be too large for optimal Quest 3 performance for tag: {0}"),
				FText::FromString(Config.SpawnTag.ToString())
			));
		}
	}

	// Check global pool size
	int32 TotalMaxSize = 0;
	for (const FSpawnClassConfig& Config : SpawnConfigs)
	{
		TotalMaxSize += Config.PoolSettings.MaxSize;
	}

	if (TotalMaxSize > GlobalMaxPoolSize)
	{
		ValidationErrors.Add(FText::Format(
			FText::FromString("Total max pool size ({0}) exceeds global max ({1})"),
			FText::AsNumber(TotalMaxSize),
			FText::AsNumber(GlobalMaxPoolSize)
		));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

void UPACS_SpawnConfig::RebuildLookupMap() const
{
	TagToIndexMap.Empty();

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::RebuildLookupMap - Starting rebuild with %d configs"), SpawnConfigs.Num());

	for (int32 i = 0; i < SpawnConfigs.Num(); ++i)
	{
		if (SpawnConfigs[i].SpawnTag.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::RebuildLookupMap - Adding tag %s at index %d"),
				*SpawnConfigs[i].SpawnTag.ToString(), i);
			TagToIndexMap.Add(SpawnConfigs[i].SpawnTag, i);

			// Also log the actor class for this config
			FString ClassName = SpawnConfigs[i].ActorClass.IsNull() ? TEXT("NULL") : SpawnConfigs[i].ActorClass.ToString();
			UE_LOG(LogTemp, Log, TEXT("  - Actor Class: %s"), *ClassName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnConfig::RebuildLookupMap - Invalid tag at index %d"), i);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfig::RebuildLookupMap - Completed. Map now has %d entries"), TagToIndexMap.Num());
}