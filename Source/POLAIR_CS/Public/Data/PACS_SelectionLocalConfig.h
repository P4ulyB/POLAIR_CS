// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/PACS_SelectionTypes.h"
#include "PACS_SelectionLocalConfig.generated.h"

UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SelectionLocalConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) bool bOverrideGlobal = false;
	UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideGlobal")) FSelectionVisualSet Visuals;
};