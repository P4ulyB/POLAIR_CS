#pragma once
#include "GameFramework/CharacterMovementComponent.h"

struct FSavedMove_HeliOrbit : public FSavedMove_Character
{
    float   SavedAngleRad = 0.f;
    FVector SavedCenterCm = FVector::ZeroVector;
    uint8   SavedOrbitVersion = 0;

    virtual void Clear() override
    {
        FSavedMove_Character::Clear();
        SavedAngleRad = 0.f; SavedCenterCm = FVector::ZeroVector; SavedOrbitVersion = 0;
    }

    virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
                            FNetworkPredictionData_Client_Character& ClientData) override;

    virtual void PrepMoveFor(ACharacter* C) override;

    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* C, float MaxDelta) const override
    {
        const FSavedMove_HeliOrbit* H = static_cast<const FSavedMove_HeliOrbit*>(NewMove.Get());
        const bool SameVersion = (SavedOrbitVersion == H->SavedOrbitVersion);
        const bool SameCentre  = SavedCenterCm.Equals(H->SavedCenterCm, 1.f);
        return SameVersion && SameCentre && FSavedMove_Character::CanCombineWith(NewMove, C, MaxDelta);
    }
};

struct FNetworkPredictionData_Client_HeliOrbit : public FNetworkPredictionData_Client_Character
{
    FNetworkPredictionData_Client_HeliOrbit(const UCharacterMovementComponent& ClientMovement)
    : FNetworkPredictionData_Client_Character(ClientMovement) {}

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FSavedMove_HeliOrbit());
    }
};