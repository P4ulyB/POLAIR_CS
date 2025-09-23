#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

// Forward declaration with proper include for weak pointer usage
#include "Pawns/NPC/PACS_NPCCharacter.h"

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

    // RepNotify for HMD state changes - made public for testing
    UFUNCTION()
    void OnRep_HMDState();

    // Selection System - Server-only tracking (not replicated)
    UFUNCTION(BlueprintCallable, Category = "Selection")
    APACS_NPCCharacter* GetSelectedNPC() const { return SelectedNPC_ServerOnly.Get(); }

    void SetSelectedNPC(APACS_NPCCharacter* InNPC);

    // Debug helper to log current selection state
    UFUNCTION(BlueprintCallable, Category = "Selection|Debug")
    void LogCurrentSelection() const;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    // Server-only: currently selected NPC for this player (weak ptr for safety)
    TWeakObjectPtr<APACS_NPCCharacter> SelectedNPC_ServerOnly;
};
