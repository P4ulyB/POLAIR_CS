#include "Core/PACS_PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Core/PACS_PlayerController.h"

APACS_PlayerState::APACS_PlayerState()
{
    HMDState = EHMDState::Unknown;
}

#pragma region Lifecycle
void APACS_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replicate HMD state to all clients
    DOREPLIFETIME(APACS_PlayerState, HMDState);
}
#pragma endregion

#pragma region VR/HMD Management
void APACS_PlayerState::OnRep_HMDState()
{
    // Handle HMD state changes - update UI, notify systems, etc.
    // Called on clients when HMD state replicates
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: HMD state changed to %d"), static_cast<int32>(HMDState));

    // VR state change handled - could trigger other systems here
}
#pragma endregion

#pragma region Selection System
void APACS_PlayerState::SetSelectedActor(AActor* InActor)
{
    AActor* PreviousActor = SelectedActor_ServerOnly.Get();
    SelectedActor_ServerOnly = MakeWeakObjectPtr(InActor);

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::SetSelectedActor - Player: %s, Previous: %s, New: %s"),
        *GetPlayerName(),
        PreviousActor ? *PreviousActor->GetName() : TEXT("None"),
        InActor ? *InActor->GetName() : TEXT("None"));
}

void APACS_PlayerState::LogCurrentSelection() const
{
    AActor* CurrentActor = SelectedActor_ServerOnly.Get();
    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::LogCurrentSelection - Player: %s, Selected: %s"),
        *GetPlayerName(),
        CurrentActor ? *CurrentActor->GetName() : TEXT("None"));
}
#pragma endregion