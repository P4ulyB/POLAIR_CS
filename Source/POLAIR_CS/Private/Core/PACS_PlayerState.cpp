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

AActor* APACS_PlayerState::GetSelectedActor() const
{
    // Return first selected actor for backward compatibility
    if (SelectedActors_ServerOnly.Num() > 0)
    {
        return SelectedActors_ServerOnly[0].Get();
    }
    return nullptr;
}

void APACS_PlayerState::SetSelectedActor(AActor* InActor)
{
    // Clear all selections and set single actor
    ClearSelectedActors();
    if (InActor)
    {
        SelectedActors_ServerOnly.Add(MakeWeakObjectPtr(InActor));
    }

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::SetSelectedActor - Player: %s, Actor: %s"),
        *GetPlayerName(),
        InActor ? *InActor->GetName() : TEXT("None"));
}

TArray<AActor*> APACS_PlayerState::GetSelectedActors() const
{
    TArray<AActor*> Result;
    for (const TWeakObjectPtr<AActor>& WeakActor : SelectedActors_ServerOnly)
    {
        if (AActor* Actor = WeakActor.Get())
        {
            Result.Add(Actor);
        }
    }
    return Result;
}

void APACS_PlayerState::AddSelectedActor(AActor* InActor)
{
    if (InActor)
    {
        SelectedActors_ServerOnly.Add(MakeWeakObjectPtr(InActor));
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::AddSelectedActor - Player: %s, Added: %s, Total: %d"),
            *GetPlayerName(), *InActor->GetName(), SelectedActors_ServerOnly.Num());
    }
}

void APACS_PlayerState::RemoveSelectedActor(AActor* InActor)
{
    if (InActor)
    {
        SelectedActors_ServerOnly.RemoveAll([InActor](const TWeakObjectPtr<AActor>& WeakActor)
        {
            return WeakActor.Get() == InActor;
        });
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::RemoveSelectedActor - Player: %s, Removed: %s, Remaining: %d"),
            *GetPlayerName(), *InActor->GetName(), SelectedActors_ServerOnly.Num());
    }
}

void APACS_PlayerState::ClearSelectedActors()
{
    int32 PreviousCount = SelectedActors_ServerOnly.Num();
    SelectedActors_ServerOnly.Empty();
    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::ClearSelectedActors - Player: %s, Cleared %d selections"),
        *GetPlayerName(), PreviousCount);
}

void APACS_PlayerState::SetSelectedActors(const TArray<AActor*>& InActors)
{
    ClearSelectedActors();
    for (AActor* Actor : InActors)
    {
        if (Actor)
        {
            SelectedActors_ServerOnly.Add(MakeWeakObjectPtr(Actor));
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::SetSelectedActors - Player: %s, Set %d actors"),
        *GetPlayerName(), SelectedActors_ServerOnly.Num());
}

void APACS_PlayerState::LogCurrentSelection() const
{
    FString SelectionList;
    for (const TWeakObjectPtr<AActor>& WeakActor : SelectedActors_ServerOnly)
    {
        if (AActor* Actor = WeakActor.Get())
        {
            if (!SelectionList.IsEmpty()) SelectionList += TEXT(", ");
            SelectionList += Actor->GetName();
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] PlayerState::LogCurrentSelection - Player: %s, Selected (%d): [%s]"),
        *GetPlayerName(), SelectedActors_ServerOnly.Num(),
        SelectionList.IsEmpty() ? TEXT("None") : *SelectionList);
}
#pragma endregion