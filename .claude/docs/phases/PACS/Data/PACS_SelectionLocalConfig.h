#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "Data/PACS_SelectionTypes.h"
#include "PACS_SelectionLocalConfig.generated.h"

/** Optional per-actor override for selection visuals */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SelectionLocalConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    bool bOverrideGlobal = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection", meta=(EditCondition="bOverrideGlobal"))
    FSelectionVisualSet Visuals;
};
