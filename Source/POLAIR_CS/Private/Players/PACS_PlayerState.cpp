#include "Players/PACS_PlayerState.h"
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

    // Replicate assessor state to all clients
    DOREPLIFETIME(APACS_PlayerState, bIsAssessor);
}

void APACS_PlayerState::OnRep_HMDState()
{
    // Handle HMD state changes - update UI, notify systems, etc.
    // Called on clients when HMD state replicates
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: HMD state changed to %d"), static_cast<int32>(HMDState));
}

void APACS_PlayerState::OnRep_IsAssessor()
{
    // Handle assessor state changes - update UI, notify systems, etc.
    // Called on clients when assessor state replicates
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: Assessor state changed to %s"), bIsAssessor ? TEXT("true") : TEXT("false"));
}