#include "Players/PACS_PlayerState.h"
#include "Net/UnrealNetwork.h"

APACS_PlayerState::APACS_PlayerState()
{
    HMDState = EHMDState::Unknown;
}

void APACS_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APACS_PlayerState, HMDState);
    DOREPLIFETIME(APACS_PlayerState, bIsAssessor);
}

void APACS_PlayerState::OnRep_HMDState()
{
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: HMD state changed to %d"), static_cast<int32>(HMDState));
}

void APACS_PlayerState::OnRep_IsAssessor()
{
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: Assessor state changed to %s"), bIsAssessor ? TEXT("true") : TEXT("false"));
}
