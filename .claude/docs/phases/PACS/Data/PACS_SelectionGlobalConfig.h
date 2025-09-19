#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "Data/PACS_SelectionTypes.h"
#include "PACS_SelectionGlobalConfig.generated.h"

/** Global shared selection visual config */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SelectionGlobalConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    FSelectionVisualSet Visuals;
};
