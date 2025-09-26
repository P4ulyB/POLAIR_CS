// Copyright POLAIR_CS Project. All Rights Reserved.

#include "Data/PACS_SpawnConfiguration.h"

UPACS_SpawnConfiguration::UPACS_SpawnConfiguration()
{
	// Initialize with default mappings
	InitializeDefaultMappings();
}

void UPACS_SpawnConfiguration::InitializeDefaultMappings()
{
	// Clear existing mappings
	CharacterTypeMappings.Empty();

	// Create default lightweight mappings
	FPACS_CharacterTypeMapping Civilian;
	Civilian.LegacyType = EPACSCharacterType::Civilian;
	Civilian.PoolType = EPACSCharacterType::LightweightCivilian;
	Civilian.bEnabled = true;
	CharacterTypeMappings.Add(Civilian);

	FPACS_CharacterTypeMapping Police;
	Police.LegacyType = EPACSCharacterType::Police;
	Police.PoolType = EPACSCharacterType::LightweightPolice;
	Police.bEnabled = true;
	CharacterTypeMappings.Add(Police);

	FPACS_CharacterTypeMapping Firefighter;
	Firefighter.LegacyType = EPACSCharacterType::Firefighter;
	Firefighter.PoolType = EPACSCharacterType::LightweightFirefighter;
	Firefighter.bEnabled = true;
	CharacterTypeMappings.Add(Firefighter);

	FPACS_CharacterTypeMapping Paramedic;
	Paramedic.LegacyType = EPACSCharacterType::Paramedic;
	Paramedic.PoolType = EPACSCharacterType::LightweightParamedic;
	Paramedic.bEnabled = true;
	CharacterTypeMappings.Add(Paramedic);
}

EPACSCharacterType UPACS_SpawnConfiguration::GetPoolTypeForLegacyType(EPACSCharacterType LegacyType) const
{
	// Find mapping for the legacy type
	for (const FPACS_CharacterTypeMapping& Mapping : CharacterTypeMappings)
	{
		if (Mapping.LegacyType == LegacyType && Mapping.bEnabled)
		{
			return Mapping.PoolType;
		}
	}

	// Fallback to lightweight civilian if no mapping found
	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnConfiguration: No mapping found for legacy type %s, defaulting to LightweightCivilian"),
		*UEnum::GetValueAsString(LegacyType));

	return EPACSCharacterType::LightweightCivilian;
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

	return true;
}

int32 UPACS_SpawnConfiguration::GetMaxNPCsForType(EPACSCharacterType CharacterType) const
{
	// For now, use global per-type limit
	// TODO: Add per-type specific limits in future iterations
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

	// Validate character type mappings
	if (CharacterTypeMappings.Num() == 0)
	{
		OutErrorMessage = TEXT("At least one character type mapping must be defined");
		return false;
	}

	// Check for duplicate legacy types
	TSet<EPACSCharacterType> SeenLegacyTypes;
	for (const FPACS_CharacterTypeMapping& Mapping : CharacterTypeMappings)
	{
		if (Mapping.bEnabled)
		{
			if (SeenLegacyTypes.Contains(Mapping.LegacyType))
			{
				OutErrorMessage = FString::Printf(TEXT("Duplicate mapping found for legacy type: %s"),
					*UEnum::GetValueAsString(Mapping.LegacyType));
				return false;
			}
			SeenLegacyTypes.Add(Mapping.LegacyType);
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

		// If character type mappings are cleared, reinitialize defaults
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_SpawnConfiguration, CharacterTypeMappings))
		{
			if (CharacterTypeMappings.Num() == 0)
			{
				InitializeDefaultMappings();
				UE_LOG(LogTemp, Log, TEXT("PACS_SpawnConfiguration: Reinitialized default character type mappings"));
			}
		}
	}
}
#endif