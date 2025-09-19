// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/PACS_SelectionTypes.h"
#include "PACS_SelectionGlobalConfig.generated.h"

UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SelectionGlobalConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) FSelectionVisualSet Visuals;
};