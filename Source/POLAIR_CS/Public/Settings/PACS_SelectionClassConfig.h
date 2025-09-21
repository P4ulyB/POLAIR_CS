#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/Texture2D.h"
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

	// Material parameters

	// Epic pattern: FLinearColor for color parameters (used by material instances)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Parameters",
		meta = (DisplayName = "Colour"))
	FLinearColor Colour = FLinearColor::White;

	// Epic pattern: Scalar for brightness intensity
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Parameters",
		meta = (DisplayName = "Brightness", ClampMin = 0.0f, ClampMax = 40.0f))
	float Brightness = 1.0f;

	// Validation and utility methods
	bool IsValid() const;
	bool MatchesClass(const UClass* TestClass) const;

	// Convert to visual config for replication (integration with existing system)
	void ApplyToVisualConfig(FPACS_NPCVisualConfig& VisualConfig) const;

	// Equality operators for array operations
	bool operator==(const FPACS_SelectionClassConfig& Other) const;
	bool operator!=(const FPACS_SelectionClassConfig& Other) const;
};