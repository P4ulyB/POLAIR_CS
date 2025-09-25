#include "Data/Settings/PACS_SelectionSystemSettings.h"
#include "Data/Configs/PACS_SelectionClassConfig.h"
#include "Actors/NPC/PACS_NPCCharacter.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#define LOCTEXT_NAMESPACE "PACSSelectionSystemSettings"

UPACS_SelectionSystemSettings::UPACS_SelectionSystemSettings()
{
	// Initialize with sensible defaults
	SelectionFadeTime = 0.3f;
	bEnableSelectionSystem = true;
}

FName UPACS_SelectionSystemSettings::GetCategoryName() const
{
	// Epic pattern: Return category name for Project Settings organization
	return FName(TEXT("PACS"));
}

FName UPACS_SelectionSystemSettings::GetSectionName() const
{
	// Epic pattern: Return section name for subcategory
	return FName(TEXT("Selection System"));
}

#if WITH_EDITOR
FText UPACS_SelectionSystemSettings::GetSectionText() const
{
	return LOCTEXT("SelectionSystemSettingsName", "Selection System");
}

FText UPACS_SelectionSystemSettings::GetSectionDescription() const
{
	return LOCTEXT("SelectionSystemSettingsDescription", "Configure global selection materials and parameters for PACS characters");
}

void UPACS_SelectionSystemSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_SelectionSystemSettings, ClassConfigurations))
		{
			OnConfigurationChanged();
		}
	}

	// Validate configurations when changed
	ValidateConfigurations();
}

void UPACS_SelectionSystemSettings::OnConfigurationChanged()
{
	// Epic pattern: Notify about configuration changes in editor
	// This helps developers know when settings have been modified
	UE_LOG(LogTemp, Log, TEXT("PACS Selection System configuration updated with %d class configurations"), ClassConfigurations.Num());
}
#endif

UPACS_SelectionSystemSettings* UPACS_SelectionSystemSettings::Get()
{
	// Epic pattern: Static access to settings instance (from GameplayTagsSettings)
	return GetMutableDefault<UPACS_SelectionSystemSettings>();
}

const FPACS_SelectionClassConfig* UPACS_SelectionSystemSettings::GetConfigForClass(TSoftClassPtr<AActor> ActorClass) const
{
	if (!ActorClass.ToSoftObjectPath().IsValid())
	{
		return nullptr;
	}

	// Load the class synchronously for comparison
	// Epic pattern: LoadSynchronous for settings access (low frequency operation)
	if (UClass* LoadedClass = ActorClass.LoadSynchronous())
	{
		return GetConfigForClass(LoadedClass);
	}

	return nullptr;
}

const FPACS_SelectionClassConfig* UPACS_SelectionSystemSettings::GetConfigForClass(const UClass* ActorClass) const
{
	if (!ActorClass || !bEnableSelectionSystem)
	{
		return nullptr;
	}

	// Find matching configuration for this class
	// Epic pattern: Linear search for settings (typically small arrays)
	for (const FPACS_SelectionClassConfig& Config : ClassConfigurations)
	{
		if (Config.MatchesClass(ActorClass))
		{
			return &Config;
		}
	}

	return nullptr;
}

bool UPACS_SelectionSystemSettings::HasValidConfiguration() const
{
	if (!bEnableSelectionSystem || ClassConfigurations.Num() == 0)
	{
		return false;
	}

	// Check if at least one configuration is valid
	for (const FPACS_SelectionClassConfig& Config : ClassConfigurations)
	{
		if (Config.IsValid())
		{
			return true;
		}
	}

	return false;
}

void UPACS_SelectionSystemSettings::ValidateConfigurations()
{
	// Epic pattern: Validation method for settings integrity
	int32 ValidConfigs = 0;

	for (const FPACS_SelectionClassConfig& Config : ClassConfigurations)
	{
		if (Config.IsValid())
		{
			ValidConfigs++;
		}
	}

#if WITH_EDITOR
	// Log validation results in editor
	if (ClassConfigurations.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("PACS Selection System: %d/%d configurations are valid"),
			ValidConfigs, ClassConfigurations.Num());
	}
#endif
}

#undef LOCTEXT_NAMESPACE