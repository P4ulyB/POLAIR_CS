#include "PACS_PlayerState.h"
#include "Net/UnrealNetwork.h"

APACS_PlayerState::APACS_PlayerState()
{
    HMDState = EHMDState::Unknown;
}

void APACS_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate HMD state to all clients
    DOREPLIFETIME(APACS_PlayerState, HMDState);
}

void APACS_PlayerState::OnRep_HMDState()
{
    // Handle HMD state changes - update UI, notify systems, etc.
    // Called on clients when HMD state replicates
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: HMD state changed to %d"), static_cast<int32>(HMDState));
}

void APACS_PlayerState::SetSelectedNPC(APACS_NPCCharacter* InNPC)
{
    APACS_NPCCharacter* PreviousNPC = SelectedNPC_ServerOnly.Get();
    SelectedNPC_ServerOnly = MakeWeakObjectPtr(InNPC);

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::SetSelectedNPC - Player: %s, Previous: %s, New: %s"),
        *GetPlayerName(),
        PreviousNPC ? *PreviousNPC->GetName() : TEXT("None"),
        InNPC ? *InNPC->GetName() : TEXT("None"));
}

void APACS_PlayerState::LogCurrentSelection() const
{
    APACS_NPCCharacter* CurrentNPC = SelectedNPC_ServerOnly.Get();
    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::LogCurrentSelection - Player: %s, Selected: %s"),
        *GetPlayerName(),
        CurrentNPC ? *CurrentNPC->GetName() : TEXT("None"));
}