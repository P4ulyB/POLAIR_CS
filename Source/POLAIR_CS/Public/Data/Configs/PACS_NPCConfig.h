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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|AnimBP")
	TSoftClassPtr<UAnimInstance> AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh", meta = (ClampMin = "0", ClampMax = "100", DisplayName = "Collision Scale Steps"))
	uint8 CollisionScaleSteps = 0; // 0 = no offset, 1 = +10%, 10 = +100%

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh", meta = (DisplayName = "Mesh Location"))
	FVector MeshLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh", meta = (DisplayName = "Mesh Rotation"))
	FRotator MeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals|Mesh", meta = (DisplayName = "Mesh Scale"))
	FVector MeshScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Decal|Materials")
	TSoftObjectPtr<class UMaterialInterface> DecalMaterial;

	void ToVisualConfig(struct FPACS_NPCVisualConfig& Out) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};