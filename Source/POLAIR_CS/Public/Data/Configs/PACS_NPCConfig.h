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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals")
	TSoftClassPtr<UAnimInstance> AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Collision", meta = (ClampMin = "0", ClampMax = "100", DisplayName = "Collision Scale Steps"))
	uint8 CollisionScaleSteps = 0; // 0 = no offset, 1 = +10%, 10 = +100%

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Decal", meta = (ClampMin = "0", ClampMax = "200", DisplayName = "Decal Size Multiplier"))
	uint8 DecalSizeMultiplier = 100; // 100 = 1.0x, 0 = 0.0x, 200 = 2.0x

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Decal")
	TSoftObjectPtr<class UMaterialInterface> DecalMaterial;

	void ToVisualConfig(struct FPACS_NPCVisualConfig& Out) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};