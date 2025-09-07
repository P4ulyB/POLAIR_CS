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

    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
    {
        if (!NewMove.IsValid()) return false;
        
        const FSavedMove_HeliOrbit* H = static_cast<const FSavedMove_HeliOrbit*>(NewMove.Get());
        if (!H) return false;
        
        const bool SameVersion = (SavedOrbitVersion == H->SavedOrbitVersion);
        const bool SameCenter  = SavedCenterCm.Equals(H->SavedCenterCm, 1.f);
        return SameVersion && SameCenter && FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);
    }
};

struct FNetworkPredictionData_Client_HeliOrbit : public FNetworkPredictionData_Client_Character
{
    FNetworkPredictionData_Client_HeliOrbit(const UCharacterMovementComponent& ClientMovement)
    : FNetworkPredictionData_Client_Character(ClientMovement) {}

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return MakeShareable(new FSavedMove_HeliOrbit());
    }
};