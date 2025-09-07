#pragma once
#include "GameFramework/CharacterMovementComponent.h"
#include "PACS_HeliMovementComponent.generated.h"

UENUM()
enum class EPACS_HeliMoveMode : uint8 { CMOVE_HeliOrbit = 0 };

UCLASS()
class UPACS_HeliMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    FVector CenterCm = FVector::ZeroVector;
    float   AltitudeCm = 0.f, RadiusCm = 1.f, SpeedCms = 0.f;
    float   AngleRad = 0.f;
    float   ServerNowS = 0.f;
    float   LastPlaneZ = TNumericLimits<float>::Lowest();

    UPROPERTY(EditDefaultsOnly) TObjectPtr<class UPACS_CandidateHelicopterData> Data;

    virtual void OnRegister() override;
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

    void TickClock_Server();
    void TickClock_Client();
    void TickClock_Proxy();

    void Eval_Server();
    void Eval_Client();
    void Eval_Proxy();

    void UpdateAngle_Server();
    void UpdateAngle_Client();
    void UpdateAngle_Proxy();

    void ApplyAltitudePlane();
    void StepKinematics(float Dt);

private:
    mutable FNetworkPredictionData_Client* ClientPredictionData = nullptr;
};