#include "PACS/Heli/PACS_HeliMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "PACS/Heli/PACS_CandidateHelicopterCharacter.h"
#include "PACS/Heli/PACS_CandidateHelicopterData.h"
#include "PACS/Heli/PACS_HeliSavedMove.h"
#include "Curves/CurveFloat.h"

static float Eval01(float StartS, float DurS, UCurveFloat* Curve, float NowS)
{
    if (DurS <= 0.f) return 1.f;
    const float T = FMath::Clamp((NowS - StartS)/DurS, 0.f, 1.f);
    return Curve ? Curve->GetFloatValue(T) : T;
}

void UPACS_HeliMovementComponent::OnRegister()
{
    Super::OnRegister();
    SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
    bConstrainToPlane = true;
}

void UPACS_HeliMovementComponent::OnMovementModeChanged(EMovementMode Prev, uint8 PrevCustom)
{
    Super::OnMovementModeChanged(Prev, PrevCustom);
    if (MovementMode == MOVE_Custom && CustomMovementMode == (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit)
    {
        LastPlaneZ = TNumericLimits<float>::Lowest();
    }
}

FNetworkPredictionData_Client* UPACS_HeliMovementComponent::GetPredictionData_Client() const
{
    check(PawnOwner);
    if (!ClientPredictionData)
    {
        UPACS_HeliMovementComponent* Mutable = const_cast<UPACS_HeliMovementComponent*>(this);
        Mutable->ClientPredictionData = new FNetworkPredictionData_Client_HeliOrbit(*this);
        
        // Cast to base class to access MaxSmoothNetUpdateDist and NoSmoothNetUpdateDist
        if (FNetworkPredictionData_Client_Character* CharPredictionData = 
            static_cast<FNetworkPredictionData_Client_Character*>(Mutable->ClientPredictionData))
        {
            CharPredictionData->MaxSmoothNetUpdateDist = 92.f;
            CharPredictionData->NoSmoothNetUpdateDist  = 140.f;
        }
    }
    return ClientPredictionData;
}

void UPACS_HeliMovementComponent::PhysCustom(float Dt, int32 Iter)
{
    const ENetRole Role = CharacterOwner ? CharacterOwner->GetLocalRole() : ROLE_None;

    if (Role == ROLE_Authority)
    {
        TickClock_Server();   Eval_Server();   ApplyAltitudePlane();   UpdateAngle_Server();   StepKinematics(Dt);
        return;
    }
    if (Role == ROLE_AutonomousProxy)
    {
        TickClock_Client();   Eval_Client();   ApplyAltitudePlane();   UpdateAngle_Client();   StepKinematics(Dt);
        return;
    }

    TickClock_Proxy();        Eval_Proxy();    ApplyAltitudePlane();   UpdateAngle_Proxy();    StepKinematics(Dt);
}

void UPACS_HeliMovementComponent::TickClock_Server()
{
    if (const AGameStateBase* GS = GetWorld()->GetGameState())
        ServerNowS = GS->GetServerWorldTimeSeconds();
    else
        ServerNowS = GetWorld()->GetTimeSeconds();
}

void UPACS_HeliMovementComponent::TickClock_Client() { TickClock_Server(); }
void UPACS_HeliMovementComponent::TickClock_Proxy()  { TickClock_Server(); }

void UPACS_HeliMovementComponent::Eval_Server()
{
    const APACS_CandidateHelicopterCharacter* C = Cast<APACS_CandidateHelicopterCharacter>(CharacterOwner);
    if (!C || !Data) return;

    const float aC = Eval01(C->OrbitAnchors.CenterStartS, (C->OrbitTargets.CenterDurS>0?C->OrbitTargets.CenterDurS:Data->CenterDurS), Data->CenterInterp, ServerNowS);
    const float aA = Eval01(C->OrbitAnchors.AltStartS,    (C->OrbitTargets.AltDurS>0?C->OrbitTargets.AltDurS:Data->AltDurS),          Data->AltInterp,    ServerNowS);
    const float aR = Eval01(C->OrbitAnchors.RadiusStartS, (C->OrbitTargets.RadiusDurS>0?C->OrbitTargets.RadiusDurS:Data->RadiusDurS), Data->RadiusInterp, ServerNowS);
    const float aS = Eval01(C->OrbitAnchors.SpeedStartS,  (C->OrbitTargets.SpeedDurS>0?C->OrbitTargets.SpeedDurS:Data->SpeedDurS),    Data->SpeedInterp,  ServerNowS);

    const FVector DesiredC = FMath::Lerp(CenterCm, C->OrbitTargets.CenterCm, aC);
    const float   MaxStep  = (Data->MaxCenterDriftCms) * FApp::GetDeltaTime();
    CenterCm += (DesiredC - CenterCm).GetClampedToMaxSize(MaxStep);

    AltitudeCm = FMath::Lerp(AltitudeCm, C->OrbitTargets.AltitudeCm, aA);
    RadiusCm   = FMath::Max(FMath::Lerp(RadiusCm,   C->OrbitTargets.RadiusCm,   aR), 1.f);
    SpeedCms   = FMath::Clamp(FMath::Lerp(SpeedCms, C->OrbitTargets.SpeedCms,   aS), 0.f, Data->MaxSpeedCms);
}

void UPACS_HeliMovementComponent::Eval_Client() { Eval_Server(); }
void UPACS_HeliMovementComponent::Eval_Proxy()  { Eval_Server(); }

void UPACS_HeliMovementComponent::UpdateAngle_Server()
{
    const APACS_CandidateHelicopterCharacter* C = Cast<APACS_CandidateHelicopterCharacter>(CharacterOwner);
    if (!C || SpeedCms <= KINDA_SMALL_NUMBER || RadiusCm <= 1.f) return;
    const float Omega = SpeedCms / RadiusCm;
    AngleRad = FMath::UnwindRadians(C->OrbitAnchors.AngleAtStart + Omega * (ServerNowS - C->OrbitAnchors.OrbitStartS));
}

void UPACS_HeliMovementComponent::UpdateAngle_Client() { UpdateAngle_Server(); }
void UPACS_HeliMovementComponent::UpdateAngle_Proxy()  { UpdateAngle_Server(); }

void UPACS_HeliMovementComponent::ApplyAltitudePlane()
{
    if (!FMath::IsNearlyEqual(LastPlaneZ, AltitudeCm, 0.1f))
    {
        SetPlaneConstraintEnabled(true);
        PlaneConstraintNormal = FVector::UpVector;
        PlaneConstraintOrigin = FVector(0,0,AltitudeCm);
        LastPlaneZ = AltitudeCm;
    }
}

void UPACS_HeliMovementComponent::StepKinematics(float Dt)
{
    const float s = FMath::Sin(AngleRad), c = FMath::Cos(AngleRad);
    const FVector Tangent(-s, c, 0.f);
    Velocity = Tangent * SpeedCms;
    const FRotator Yaw = Tangent.ToOrientationRotator();

    FHitResult Hit;
    SafeMoveUpdatedComponent(Velocity * Dt, Yaw, /*bSweep=*/true, Hit);
    if (Hit.IsValidBlockingHit())
    {
        SlideAlongSurface(Velocity * Dt, 1.f - Hit.Time, Hit.Normal, Hit, true);
    }
}