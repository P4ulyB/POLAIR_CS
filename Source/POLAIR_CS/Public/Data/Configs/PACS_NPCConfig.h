#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/AssetManager.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (DisplayName = "Skeletal Mesh"))
	FPrimaryAssetId SkeletalMeshId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (DisplayName = "Animation Blueprint"))
	FPrimaryAssetId AnimBPId;

	void ToVisualConfig(struct FPACS_NPCVisualConfig& Out) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};