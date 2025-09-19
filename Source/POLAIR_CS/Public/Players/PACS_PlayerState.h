#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PACS_PlayerState.generated.h"

UENUM(BlueprintType)
enum class EHMDState : uint8
{
    Unknown = 0,    // HMD state not yet detected
    NoHMD = 1,      // No HMD connected/enabled
    HasHMD = 2      // HMD connected and enabled
};

UCLASS()
class POLAIR_CS_API APACS_PlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    APACS_PlayerState();

    // HMD detection state - replicated to all clients for system-wide access
    UPROPERTY(ReplicatedUsing=OnRep_HMDState, BlueprintReadOnly, Category = "PACS")
    EHMDState HMDState = EHMDState::Unknown;

    // Is this player an assessor (not VR candidate)?
    UPROPERTY(ReplicatedUsing=OnRep_IsAssessor, BlueprintReadOnly, Category = "PACS")
    bool bIsAssessor = false;

    // RepNotify for HMD state changes - made public for testing
    UFUNCTION()
    void OnRep_HMDState();

    // RepNotify for assessor state changes
    UFUNCTION()
    void OnRep_IsAssessor();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
