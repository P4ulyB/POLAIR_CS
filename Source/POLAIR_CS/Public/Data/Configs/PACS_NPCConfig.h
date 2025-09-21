#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Data/PACS_NPCVisualConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PACS_NPCConfig.generated.h"

UCLASS(BlueprintType, meta = (DisplayName = "NPC Configuration"))
class POLAIR_CS_API UPACS_NPCConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|AnimBP")
	TSoftClassPtr<UAnimInstance> AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh", meta = (ClampMin = "0", ClampMax = "100", DisplayName = "Collision Scale Steps"))
	uint8 CollisionScaleSteps = 0; // 0 = no offset, 1 = +10%, 10 = +100%

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh", meta = (DisplayName = "Mesh Location"))
	FVector MeshLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh", meta = (DisplayName = "Mesh Rotation"))
	FRotator MeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh", meta = (DisplayName = "Mesh Scale"))
	FVector MeshScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Decal|Materials")
	TSoftObjectPtr<class UMaterialInterface> DecalMaterial;

	// Available State (default startup state)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Selection|Available State",
		meta = (DisplayName = "Brightness", ClampMin = 0.0f, ClampMax = 40.0f,
				ToolTip = "Brightness when NPC is available for interaction"))
	float AvailableBrightness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Selection|Available State",
		meta = (DisplayName = "Colour",
				ToolTip = "Color when NPC is available for interaction"))
	FLinearColor AvailableColour = FLinearColor::Green;

	// Selected State
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Selection|Selected State",
		meta = (DisplayName = "Brightness", ClampMin = 0.0f, ClampMax = 40.0f,
				ToolTip = "Brightness when NPC is currently selected"))
	float SelectedBrightness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Selection|Selected State",
		meta = (DisplayName = "Colour",
				ToolTip = "Color when NPC is currently selected"))
	FLinearColor SelectedColour = FLinearColor::Yellow;

	// Hovered State
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Selection|Hovered State",
		meta = (DisplayName = "Brightness", ClampMin = 0.0f, ClampMax = 40.0f,
				ToolTip = "Brightness when NPC is being hovered over"))
	float HoveredBrightness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Selection|Hovered State",
		meta = (DisplayName = "Colour",
				ToolTip = "Color when NPC is being hovered over"))
	FLinearColor HoveredColour = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);

	// Unavailable State
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Selection|Unavailable State",
		meta = (DisplayName = "Brightness", ClampMin = 0.0f, ClampMax = 40.0f,
				ToolTip = "Brightness when NPC is unavailable for interaction"))
	float UnavailableBrightness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Selection|Unavailable State",
		meta = (DisplayName = "Colour",
				ToolTip = "Color when NPC is unavailable for interaction"))
	FLinearColor UnavailableColour = FLinearColor::Red;

	void ToVisualConfig(struct FPACS_NPCVisualConfig& Out) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};