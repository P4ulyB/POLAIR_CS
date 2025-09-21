#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/PACS_NPCVisualConfig.h"
#include "PACS_SelectionClassConfig.generated.h"

class APACS_NPCCharacter;

/**
 * Configuration for a single character class selection system
 * Follows Epic pattern from EditorExperimentalSettings for struct arrays in UDeveloperSettings
 */
USTRUCT(BlueprintType)
struct POLAIR_CS_API FPACS_SelectionClassConfig
{
	GENERATED_BODY()

public:
	FPACS_SelectionClassConfig();

	// Target character class for this configuration
	// Epic pattern: Use specific derived type for UI filtering (from ChildActorComponent)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class",
		meta = (DisplayName = "Target Character Class", AllowAbstract = false))
	TSoftClassPtr<APACS_NPCCharacter> TargetClass;

	// Selection material instance (soft reference for async loading)
	// Epic pattern: TSoftObjectPtr for material references to prevent auto-loading
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material",
		meta = (DisplayName = "Selection Material Instance"))
	TSoftObjectPtr<UMaterialInterface> SelectionMaterial;

	// Material scalar parameters (matching user requirements)
	// Epic naming convention: Semantic names, not abbreviated (from MaterialExpressionParameter)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Parameters",
		meta = (DisplayName = "Brightness", ClampMin = 0.0f, ClampMax = 2.0f))
	float Brightness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Parameters",
		meta = (DisplayName = "Texture", ClampMin = 0.0f, ClampMax = 10.0f))
	float Texture = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Parameters",
		meta = (DisplayName = "Colour", ClampMin = 0.0f, ClampMax = 10.0f))
	float Colour = 0.0f;

	// Validation and utility methods
	bool IsValid() const;
	bool MatchesClass(const UClass* TestClass) const;

	// Convert to visual config for replication (integration with existing system)
	void ApplyToVisualConfig(FPACS_NPCVisualConfig& VisualConfig) const;

	// Equality operators for array operations
	bool operator==(const FPACS_SelectionClassConfig& Other) const;
	bool operator!=(const FPACS_SelectionClassConfig& Other) const;
};