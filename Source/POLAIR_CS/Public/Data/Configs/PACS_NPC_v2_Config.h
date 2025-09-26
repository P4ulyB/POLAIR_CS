// Copyright Notice: Property of Particular Audience Limited

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PACS_NPC_v2_Config.generated.h"

/**
 * Simple configuration data asset for lightweight NPCs
 * Contains only essential properties - skeletal mesh and animations
 */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_NPC_v2_Config : public UDataAsset
{
    GENERATED_BODY()

public:
    // Visual
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    TSoftObjectPtr<class USkeletalMesh> SkeletalMesh;

    // Animation
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<class UAnimSequence> IdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<class UAnimSequence> MoveAnimation;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "50.0", ClampMax = "1000.0"))
    float WalkSpeed = 300.0f;

    // ========== Decal Configuration ==========
    // Decal material to use for selection states
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal",
        meta = (DisplayName = "Decal Material"))
    TSoftObjectPtr<class UMaterialInterface> DecalMaterial;

    // Available State - Default state when NPC is unselected
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Available State",
        meta = (ClampMin = "0.0", ClampMax = "40.0", DisplayName = "Available Brightness"))
    float AvailableBrightness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Available State",
        meta = (DisplayName = "Available Color"))
    FLinearColor AvailableColor = FLinearColor::Green;

    // Hovered State - LOCAL ONLY, never replicated
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Hovered State",
        meta = (ClampMin = "0.0", ClampMax = "40.0", DisplayName = "Hovered Brightness"))
    float HoveredBrightness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Hovered State",
        meta = (DisplayName = "Hovered Color"))
    FLinearColor HoveredColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);

    // Selected State - Shown only to the selecting client
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Selected State",
        meta = (ClampMin = "0.0", ClampMax = "40.0", DisplayName = "Selected Brightness"))
    float SelectedBrightness = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Selected State",
        meta = (DisplayName = "Selected Color"))
    FLinearColor SelectedColor = FLinearColor::Yellow;

    // Unavailable State - Shown to other clients when NPC is selected by someone else
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Unavailable State",
        meta = (ClampMin = "0.0", ClampMax = "40.0", DisplayName = "Unavailable Brightness"))
    float UnavailableBrightness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Unavailable State",
        meta = (DisplayName = "Unavailable Color"))
    FLinearColor UnavailableColor = FLinearColor::Red;
};