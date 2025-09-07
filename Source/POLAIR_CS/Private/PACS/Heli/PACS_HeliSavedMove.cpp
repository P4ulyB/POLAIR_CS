#include "PACS/Heli/PACS_HeliSavedMove.h"
#include "PACS/Heli/PACS_CandidateHelicopterCharacter.h"
#include "PACS/Heli/PACS_HeliMovementComponent.h"

void FSavedMove_HeliOrbit::SetMoveFor(ACharacter* C, float InDeltaTime, const FVector& NewAccel,
                                      FNetworkPredictionData_Client_Character& ClientData)
{
    FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
    if (const APACS_CandidateHelicopterCharacter* H = Cast<APACS_CandidateHelicopterCharacter>(C))
    {
        const UPACS_HeliMovementComponent* MC = CastChecked<UPACS_HeliMovementComponent>(H->GetCharacterMovement());
        SavedAngleRad     = MC->AngleRad;
        SavedCenterCm     = MC->CenterCm;
        SavedOrbitVersion = H->OrbitParamsVersion;
    }
}

void FSavedMove_HeliOrbit::PrepMoveFor(ACharacter* C)
{
    FSavedMove_Character::PrepMoveFor(C);
}