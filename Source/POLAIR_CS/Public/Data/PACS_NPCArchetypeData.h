#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PACS_NPCArchetypeData.generated.h"

/** What the NPC is: appearance & basic movement knobs */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_NPCArchetypeData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
    TSoftClassPtr<UAnimInstance> AnimBPClass;

    /** Slot index -> material override */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
    TMap<int32, TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.0"))
    float WalkSpeed = 150.f;
};