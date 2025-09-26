// Copyright POLAIR_CS Project. All Rights Reserved.

#include "Data/PACS_SpawnConfiguration.h"

UPACS_SpawnConfiguration::UPACS_SpawnConfiguration()
{
	// Initialize with default entries if needed
	InitializeDefaultMappings();
}

void UPACS_SpawnConfiguration::InitializeDefaultMappings()
{
	// Clear existing entries
	CharacterPoolEntries.Empty();

	// Note: Default entries are left empty as blueprints should be configured in the editor
	// The data asset will be populated with actual blueprint references in the editor

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfiguration: Ready for blueprint configuration in editor"));
}

TSoftClassPtr<APawn> UPACS_SpawnConfiguration::GetCharacterBlueprintForType(EPACSCharacterType PoolType) const
{
	// Find the pool entry for this type
	for (const FPACS_CharacterPoolEntry& Entry : CharacterPoolEntries)
	{
		if (Entry.PoolType == PoolType && Entry.bEnabled)
		{
			return Entry.CharacterBlueprint;
		}
	}

	// Return empty if no blueprint found
	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnConfiguration: No blueprint found for pool type %s"),
		*UEnum::GetValueAsString(PoolType));

	return TSoftClassPtr<APawn>();
}

bool UPACS_SpawnConfiguration::GetPoolSettingsForType(EPACSCharacterType PoolType,
	int32& OutInitialPoolSize, int32& OutMaxPoolSize) const
{
	// Find the pool entry for this type
	for (const FPACS_CharacterPoolEntry& Entry : CharacterPoolEntries)
	{
		if (Entry.PoolType == PoolType && Entry.bEnabled)
		{
			OutInitialPoolSize = Entry.InitialPoolSize;
			OutMaxPoolSize = Entry.MaxPoolSize;
			return true;
		}
	}

	// Not found - set defaults
	OutInitialPoolSize = 10;
	OutMaxPoolSize = 50;
	return false;
}

bool UPACS_SpawnConfiguration::IsSpawningAllowed(EPACSCharacterType CharacterType, int32 CurrentCount) const
{
	// Check global limit
	if (CurrentCount >= MaxTotalNPCs)
	{
		return false;
	}

	// Check per-type limit
	if (CurrentCount >= GetMaxNPCsForType(CharacterType))
	{
		return false;
	}

	// Check if this character type has an enabled pool entry
	bool bHasEnabledEntry = false;
	for (const FPACS_CharacterPoolEntry& Entry : CharacterPoolEntries)
	{
		if (Entry.PoolType == CharacterType && Entry.bEnabled)
		{
			bHasEnabledEntry = true;
			break;
		}
	}

	return bHasEnabledEntry;
}

int32 UPACS_SpawnConfiguration::GetMaxNPCsForType(EPACSCharacterType CharacterType) const
{
	// Check if there's a specific pool entry with its own limit
	int32 InitialSize, MaxSize;
	if (GetPoolSettingsForType(CharacterType, InitialSize, MaxSize))
	{
		return MaxSize;
	}

	// Fall back to global per-type limit
	return MaxNPCsPerType;
}

bool UPACS_SpawnConfiguration::ValidateConfiguration(FString& OutErrorMessage) const
{
	OutErrorMessage.Empty();

	// Validate basic limits
	if (MaxTotalNPCs <= 0)
	{
		OutErrorMessage = TEXT("MaxTotalNPCs must be greater than 0");
		return false;
	}

	if (MaxNPCsPerType <= 0)
	{
		OutErrorMessage = TEXT("MaxNPCsPerType must be greater than 0");
		return false;
	}

	if (MaxSpawnsPerTick <= 0)
	{
		OutErrorMessage = TEXT("MaxSpawnsPerTick must be greater than 0");
		return false;
	}

	// Validate character pool entries
	if (CharacterPoolEntries.Num() == 0)
	{
		OutErrorMessage = TEXT("At least one character pool entry should be defined");
		// This is a warning, not an error - configuration can be done in editor
		UE_LOG(LogTemp, Warning, TEXT("%s"), *OutErrorMessage);
		return true; // Allow empty configuration to be filled in editor
	}

	// Check for duplicate pool types
	TSet<EPACSCharacterType> SeenPoolTypes;
	for (const FPACS_CharacterPoolEntry& Entry : CharacterPoolEntries)
	{
		if (Entry.bEnabled)
		{
			if (SeenPoolTypes.Contains(Entry.PoolType))
			{
				OutErrorMessage = FString::Printf(TEXT("Duplicate pool entry found for type: %s"),
					*UEnum::GetValueAsString(Entry.PoolType));
				return false;
			}
			SeenPoolTypes.Add(Entry.PoolType);

			// Validate blueprint reference
			if (Entry.CharacterBlueprint.IsNull())
			{
				OutErrorMessage = FString::Printf(TEXT("Pool entry for type %s has no blueprint assigned"),
					*UEnum::GetValueAsString(Entry.PoolType));
				return false;
			}

			// Validate pool sizes
			if (Entry.InitialPoolSize <= 0)
			{
				OutErrorMessage = FString::Printf(TEXT("Pool entry for type %s has invalid initial size"),
					*UEnum::GetValueAsString(Entry.PoolType));
				return false;
			}

			if (Entry.MaxPoolSize < Entry.InitialPoolSize)
			{
				OutErrorMessage = FString::Printf(TEXT("Pool entry for type %s has max size less than initial size"),
					*UEnum::GetValueAsString(Entry.PoolType));
				return false;
			}
		}
	}

	// Validate spawn zones
	TSet<FString> SeenZoneNames;
	for (const FPACS_SpawnZoneConfig& Zone : SpawnZones)
	{
		if (Zone.ZoneName.IsEmpty())
		{
			OutErrorMessage = TEXT("Spawn zone name cannot be empty");
			return false;
		}

		if (SeenZoneNames.Contains(Zone.ZoneName))
		{
			OutErrorMessage = FString::Printf(TEXT("Duplicate spawn zone name: %s"), *Zone.ZoneName);
			return false;
		}
		SeenZoneNames.Add(Zone.ZoneName);

		if (Zone.MaxNPCs <= 0)
		{
			OutErrorMessage = FString::Printf(TEXT("Spawn zone %s must have MaxNPCs greater than 0"), *Zone.ZoneName);
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
void UPACS_SpawnConfiguration::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();

		// Auto-validate configuration when properties change
		FString ErrorMessage;
		if (!ValidateConfiguration(ErrorMessage))
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnConfiguration validation failed: %s"), *ErrorMessage);
		}

		// Log pool entry changes for debugging
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_SpawnConfiguration, CharacterPoolEntries))
		{
			UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfiguration: Character pool entries updated (%d entries)"),
				CharacterPoolEntries.Num());
		}
	}
}
#endif