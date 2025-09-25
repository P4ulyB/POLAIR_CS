#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/Configs/PACS_SelectionClassConfig.h"
#include "PACS_SelectionSystemSettings.generated.h"

/**
 * Global selection system settings for PACS characters
 * Extends UDeveloperSettings following Epic pattern from GameplayTagsSettings
 * Provides Project Settings → PACS → Selection System category
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="PACS Selection System"))
class POLAIR_CS_API UPACS_SelectionSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPACS_SelectionSystemSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Global selection configurations per class
	// Epic pattern: TArray<USTRUCT> in UDeveloperSettings (from EditorExperimentalSettings)
	UPROPERTY(Config, EditAnywhere, Category = "Selection Classes",
		meta = (DisplayName = "Class Selection Configurations",
				ToolTip = "Configure selection materials and parameters for each character class"))
	TArray<FPACS_SelectionClassConfig> ClassConfigurations;

	// Global selection system settings
	UPROPERTY(Config, EditAnywhere, Category = "Global Settings",
		meta = (DisplayName = "Selection Fade Time", ClampMin = 0.0f, ClampMax = 2.0f,
				ToolTip = "Time in seconds for selection highlight fade transitions"))
	float SelectionFadeTime = 0.3f;

	UPROPERTY(Config, EditAnywhere, Category = "Global Settings",
		meta = (DisplayName = "Enable Selection System",
				ToolTip = "Enable or disable the global selection system"))
	bool bEnableSelectionSystem = true;

	// Runtime access methods
	// Epic pattern: Static access to settings (from GameplayTagsSettings)
	UFUNCTION(BlueprintCallable, Category = "PACS Selection")
	static UPACS_SelectionSystemSettings* Get();

	// Note: Cannot be BlueprintCallable due to UE5 reflection limitation with USTRUCT pointers
	const FPACS_SelectionClassConfig* GetConfigForClass(TSoftClassPtr<AActor> ActorClass) const;

	// C++ only method - UE5 reflection cannot expose USTRUCT pointers to Blueprint
	const FPACS_SelectionClassConfig* GetConfigForClass(const UClass* ActorClass) const;

	// Authority validation
	UFUNCTION(BlueprintCallable, Category = "PACS Selection")
	bool HasValidConfiguration() const;

	// Get configuration count for debugging/validation
	UFUNCTION(BlueprintCallable, Category = "PACS Selection")
	int32 GetConfigurationCount() const { return ClassConfigurations.Num(); }

private:
	// Settings validation
	void ValidateConfigurations();

#if WITH_EDITOR
	// Editor-only validation and change handling
	void OnConfigurationChanged();
#endif
};