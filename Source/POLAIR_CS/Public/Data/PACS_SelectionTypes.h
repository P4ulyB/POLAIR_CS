// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PACS_SelectionTypes.generated.h"

class UMaterialInterface;
class USoundBase;

USTRUCT(BlueprintType)
struct FSelectionDecalParams
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UTexture2D>        Texture;
	UPROPERTY(EditAnywhere) FLinearColor                      Colour = FLinearColor(0.2f,0.7f,1.f);
	UPROPERTY(EditAnywhere, meta=(ClampMin=0)) float          Brightness = 10.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin=1)) float          ScaleXY = 256.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin=1)) float          ThicknessZ = 64.f;
};

USTRUCT(BlueprintType)
struct FSelectionVisualSet
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FSelectionDecalParams Hovered;
	UPROPERTY(EditAnywhere) FSelectionDecalParams SelectedOwner;
	UPROPERTY(EditAnywhere) FSelectionDecalParams Unavailable;
};