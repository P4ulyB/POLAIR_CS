#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/Texture2D.h"
#include "Data/PACS_NPCVisualConfig.h"
#include "PACS_SelectionClassConfig.generated.h"

class APACS_NPCCharacter;
class AActor;

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
	// Now supports any Actor that implements IPACS_SelectableCharacterInterface
	// This includes both APACS_NPCCharacter and APACS_NPC_Humanoid
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class",
		meta = (DisplayName = "Target Character Class", AllowAbstract = false))
	TSoftClassPtr<AActor> TargetClass;

	// Selection material instance (soft reference for async loading)
	// Epic pattern: TSoftObjectPtr for material references to prevent auto-loading
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material",
		meta = (DisplayName = "Selection Material Instance"))
	TSoftObjectPtr<UMaterialInterface> SelectionMaterial;

	// Available State (default startup state)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Available State",
		meta = (DisplayName = "Available Brightness", ClampMin = 0.0f, ClampMax = 40.0f,
				ToolTip = "Brightness when NPC is available for interaction"))
	float AvailableBrightness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Available State",
		meta = (DisplayName = "Available Colour",
				ToolTip = "Color when NPC is available for interaction"))
	FLinearColor AvailableColour = FLinearColor::Green;

	// Selected State
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selected State",
		meta = (DisplayName = "Selected Brightness", ClampMin = 0.0f, ClampMax = 40.0f,
				ToolTip = "Brightness when NPC is currently selected"))
	float SelectedBrightness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selected State",
		meta = (DisplayName = "Selected Colour",
				ToolTip = "Color when NPC is currently selected"))
	FLinearColor SelectedColour = FLinearColor::Yellow;

	// Hovered State
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hovered State",
		meta = (DisplayName = "Hovered Brightness", ClampMin = 0.0f, ClampMax = 40.0f,
				ToolTip = "Brightness when NPC is being hovered over"))
	float HoveredBrightness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hovered State",
		meta = (DisplayName = "Hovered Colour",
				ToolTip = "Color when NPC is being hovered over"))
	FLinearColor HoveredColour = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);

	// Unavailable State
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unavailable State",
		meta = (DisplayName = "Unavailable Brightness", ClampMin = 0.0f, ClampMax = 40.0f,
				ToolTip = "Brightness when NPC is unavailable for interaction"))
	float UnavailableBrightness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unavailable State",
		meta = (DisplayName = "Unavailable Colour",
				ToolTip = "Color when NPC is unavailable for interaction"))
	FLinearColor UnavailableColour = FLinearColor::Red;

	// Validation and utility methods
	bool IsValid() const;
	bool MatchesClass(const UClass* TestClass) const;

	// Convert to visual config for replication (integration with existing system)
	void ApplyToVisualConfig(FPACS_NPCVisualConfig& VisualConfig) const;

	// Equality operators for array operations
	bool operator==(const FPACS_SelectionClassConfig& Other) const;
	bool operator!=(const FPACS_SelectionClassConfig& Other) const;
};