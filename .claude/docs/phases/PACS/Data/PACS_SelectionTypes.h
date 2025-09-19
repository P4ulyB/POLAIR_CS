#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "PACS_SelectionTypes.generated.h"

USTRUCT(BlueprintType)
struct FSelectionDecalParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Material")
    TSoftObjectPtr<UMaterialInterface> BaseMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Material")
    TSoftObjectPtr<UTexture2D> Texture;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Material")
    FLinearColor Colour = FLinearColor(0.2f, 0.7f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Material", meta=(ClampMin="0.0"))
    float Brightness = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Decal", meta=(ClampMin="1.0"))
    float ScaleXY = 256.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Decal", meta=(ClampMin="1.0"))
    float ThicknessZ = 64.f;
};

USTRUCT(BlueprintType)
struct FSelectionVisualSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    FSelectionDecalParams Hovered;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    FSelectionDecalParams SelectedOwner;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    FSelectionDecalParams Unavailable;
};
