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
};