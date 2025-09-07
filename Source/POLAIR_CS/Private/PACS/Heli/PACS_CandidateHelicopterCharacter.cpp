#include "PACS/Heli/PACS_CandidateHelicopterCharacter.h"
#include "PACS/Heli/PACS_HeliMovementComponent.h"
#include "PACS/Heli/PACS_CandidateHelicopterData.h"
#include "PACS/Heli/PACS_OrbitMessages.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

APACS_CandidateHelicopterCharacter::APACS_CandidateHelicopterCharacter(const FObjectInitializer& OI)
: Super(OI.SetDefaultSubobjectClass<UPACS_HeliMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    HelicopterFrame = CreateDefaultSubobject<UStaticMeshComponent>("HelicopterFrame");
    HelicopterFrame->SetupAttachment(RootComponent);

    CockpitRoot   = CreateDefaultSubobject<USceneComponent>("CockpitRoot");
    CockpitRoot->SetupAttachment(RootComponent);

    SeatOriginRef = CreateDefaultSubobject<USceneComponent>("SeatOriginRef");
    SeatOriginRef->SetupAttachment(CockpitRoot);

    SeatOffsetRoot = CreateDefaultSubobject<USceneComponent>("SeatOffsetRoot");
    SeatOffsetRoot->SetupAttachment(SeatOriginRef);

    VRCamera = CreateDefaultSubobject<UCameraComponent>("VRCamera");
    VRCamera->SetupAttachment(SeatOffsetRoot);
    VRCamera->bLockToHmd = true;

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 180.f, 0);
}

void APACS_CandidateHelicopterCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        // Ensure custom movement
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
        CMC->bConstrainToPlane = true;

        // Prefer-existing: do NOT stomp a valid CMC->Data with nullptr.
        if (!CMC->Data && Data)      { CMC->Data = Data; }
        else if (CMC->Data && !Data) { Data = CMC->Data; }

        // Optional diagnostic (remove after validation)
        // ensureMsgf(CMC->Data != nullptr, TEXT("PACS: CMC->Data is NULL. Set the data asset on either the Character or the CMC in BP."));
    }

    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        UE_LOG(LogTemp, Log, TEXT("PACS Spawn: Mode=%d Custom=%d Data=%s"),
            int32(CMC->MovementMode),
            int32(CMC->CustomMovementMode),
            CMC->Data ? TEXT("OK") : TEXT("NULL"));
    }
}

void APACS_CandidateHelicopterCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
    }
}

void APACS_CandidateHelicopterCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();

    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
    }
}

void APACS_CandidateHelicopterCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateBankVisual(DeltaSeconds);
}

void APACS_CandidateHelicopterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APACS_CandidateHelicopterCharacter, OrbitTargets);
    DOREPLIFETIME(APACS_CandidateHelicopterCharacter, OrbitAnchors);
    DOREPLIFETIME(APACS_CandidateHelicopterCharacter, SelectedBy);
    DOREPLIFETIME(APACS_CandidateHelicopterCharacter, OrbitParamsVersion);
}

static float NowS(const UWorld* W)
{
    if (const AGameStateBase* GS = W->GetGameState()) return GS->GetServerWorldTimeSeconds();
    return W->GetTimeSeconds();
}

// ----- VR Seat -----
void APACS_CandidateHelicopterCharacter::CenterSeatedPose(bool bSnapYawToVehicleForward)
{
    ZeroSeatChain();
    UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(
        bSnapYawToVehicleForward ? GetActorRotation().Yaw : 0.f,
        EOrientPositionSelector::OrientationAndPosition);
    ApplySeatOffset();
}
void APACS_CandidateHelicopterCharacter::ZeroSeatChain()
{
    SeatOriginRef->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    SeatOffsetRoot->SetRelativeLocationAndRotation(SeatLocalOffsetCm, FRotator::ZeroRotator);
}
void APACS_CandidateHelicopterCharacter::ApplySeatOffset()
{
    if (const UPACS_CandidateHelicopterData* D = Data)
    {
        SeatLocalOffsetCm.X = FMath::Clamp(SeatLocalOffsetCm.X, -D->SeatLocalClamp.X, D->SeatLocalClamp.X);
        SeatLocalOffsetCm.Y = FMath::Clamp(SeatLocalOffsetCm.Y, -D->SeatLocalClamp.Y, D->SeatLocalClamp.Y);
        SeatLocalOffsetCm.Z = FMath::Clamp(SeatLocalOffsetCm.Z, -D->SeatLocalClamp.Z, D->SeatLocalClamp.Z);
    }
    SeatOffsetRoot->SetRelativeLocation(SeatLocalOffsetCm);
}
void APACS_CandidateHelicopterCharacter::NudgeSeatX(float S){ SeatLocalOffsetCm.X += S; ApplySeatOffset(); }
void APACS_CandidateHelicopterCharacter::NudgeSeatY(float S){ SeatLocalOffsetCm.Y += S; ApplySeatOffset(); }
void APACS_CandidateHelicopterCharacter::NudgeSeatZ(float S){ SeatLocalOffsetCm.Z += S; ApplySeatOffset(); }

// ----- Banking (visual only) -----
void APACS_CandidateHelicopterCharacter::UpdateBankVisual(float Dt)
{
    const auto* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement());
    if (!CMC || !Data || !HelicopterFrame) return;
    const float Target = +(CMC->SpeedCms / FMath::Max(Data->MaxSpeedCms,1.f)) * Data->MaxBankDeg;
    CurrentBankDeg = FMath::FInterpTo(CurrentBankDeg, Target, Dt, BankInterpSpeed);
    HelicopterFrame->SetRelativeRotation(FRotator(0, 0, CurrentBankDeg));
}

// ----- Param Validation -----
bool APACS_CandidateHelicopterCharacter::ValidateOrbitCenter(const FVector& Proposed) const
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(OrbitCenter), false, this);
    const FCollisionShape Probe = FCollisionShape::MakeSphere(50.f);
    return !GetWorld()->OverlapAnyTestByChannel(Proposed, FQuat::Identity, ECC_WorldStatic, Probe, Q);
}

// ----- Reliable batched edits -----
void APACS_CandidateHelicopterCharacter::Server_ApplyOrbitParams_Implementation(const FPACS_OrbitEdit& E)
{
    if (!HasAuthority() || (SelectedBy == nullptr)) return;
    if (E.TransactionId <= LastAppliedTxnId) return;
    LastAppliedTxnId = E.TransactionId;

    if (E.bHasCenter && !ValidateOrbitCenter(E.NewCenterCm)) return;
    if (E.bHasCenter) OrbitTargets.CenterCm = E.NewCenterCm;
    if (E.bHasAlt)    OrbitTargets.AltitudeCm = FMath::Clamp(E.NewAltCm, 100.f, 1'000'000.f);
    if (E.bHasRadius) OrbitTargets.RadiusCm   = FMath::Clamp(E.NewRadiusCm, 100.f, 1'000'000.f);
    if (E.bHasSpeed)  OrbitTargets.SpeedCms   = FMath::Clamp(E.NewSpeedCms, 0.f, Data ? Data->MaxSpeedCms : 6000.f);

    OrbitTargets.CenterDurS = E.DurCenterS > 0 ? E.DurCenterS : (Data ? Data->CenterDurS : 0.f);
    OrbitTargets.AltDurS    = E.DurAltS    > 0 ? E.DurAltS    : (Data ? Data->AltDurS    : 0.f);
    OrbitTargets.RadiusDurS = E.DurRadiusS > 0 ? E.DurRadiusS : (Data ? Data->RadiusDurS : 0.f);
    OrbitTargets.SpeedDurS  = E.DurSpeedS  > 0 ? E.DurSpeedS  : (Data ? Data->SpeedDurS  : 0.f);

    const float S = NowS(GetWorld());
    if (E.bHasCenter) OrbitAnchors.CenterStartS = S;
    if (E.bHasAlt)    OrbitAnchors.AltStartS    = S;
    if (E.bHasRadius) OrbitAnchors.RadiusStartS = S;
    if (E.bHasSpeed)  OrbitAnchors.SpeedStartS  = S;

    if (E.AnchorPolicy == EPACS_AnchorPolicy::PreserveAngleOnce)
    {
        const auto* CMC = CastChecked<UPACS_HeliMovementComponent>(GetCharacterMovement());
        OrbitAnchors.AngleAtStart = FMath::UnwindRadians(CMC->AngleRad);
        OrbitAnchors.OrbitStartS  = S;
    }
    else
    {
        OrbitAnchors.AngleAtStart = 0.f;
        OrbitAnchors.OrbitStartS  = S;
    }

    ++OrbitParamsVersion;
    ForceNetUpdate();
}

void APACS_CandidateHelicopterCharacter::Server_RequestSelect_Implementation(APlayerState* Requestor)
{
    if (!HasAuthority()) return;
    if (SelectedBy == nullptr) SelectedBy = Requestor;
}

void APACS_CandidateHelicopterCharacter::Server_ReleaseSelect_Implementation(APlayerState* Requestor)
{
    if (!HasAuthority()) return;
    if (SelectedBy == Requestor) SelectedBy = nullptr;
}

void APACS_CandidateHelicopterCharacter::ApplyOffsetsThenSeed(const FPACS_OrbitOffsets* Off)
{
    const UPACS_CandidateHelicopterData* EffData = Data;
    if (!EffData)
    {
        if (const UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
        {
            EffData = CMC->Data;
        }
    }

    const float S = NowS(GetWorld());

    float Alt = EffData ? EffData->DefaultAltitudeCm : 20000.f;
    float Rad = EffData ? EffData->DefaultRadiusCm   : 15000.f;
    float Spd = EffData ? EffData->DefaultSpeedCms   : 2222.22f;
    const float MaxSpd = EffData ? EffData->MaxSpeedCms : 6000.f;

    if (Off)
    {
        if (Off->bHasAltOffset)    Alt += Off->AltitudeDeltaCm;
        if (Off->bHasRadiusOffset) Rad += Off->RadiusDeltaCm;
        if (Off->bHasSpeedOffset)  Spd += Off->SpeedDeltaCms;
    }

    OrbitTargets.CenterCm   = FVector(GetActorLocation().X, GetActorLocation().Y, 0.f);
    OrbitTargets.AltitudeCm = FMath::Max(Alt, 100.f);
    OrbitTargets.RadiusCm   = FMath::Max(Rad, 100.f);
    OrbitTargets.SpeedCms   = FMath::Clamp(Spd, 0.f, MaxSpd);
    OrbitTargets.CenterDurS = OrbitTargets.AltDurS = OrbitTargets.RadiusDurS = OrbitTargets.SpeedDurS = 0.f;

    UE_LOG(LogTemp, Log, TEXT("PACS Seed: Alt=%.0f Rad=%.0f Spd=%.0f (MaxSpd=%.0f)"),
        OrbitTargets.AltitudeCm, OrbitTargets.RadiusCm, OrbitTargets.SpeedCms,
        EffData ? EffData->MaxSpeedCms : -1.f);

    OrbitAnchors.CenterStartS = OrbitAnchors.AltStartS = OrbitAnchors.RadiusStartS =
    OrbitAnchors.SpeedStartS  = OrbitAnchors.OrbitStartS = S;
    OrbitAnchors.AngleAtStart = 0.f;

    const FVector StartPos = FVector(OrbitTargets.CenterCm.X,
                                     OrbitTargets.CenterCm.Y + OrbitTargets.RadiusCm,
                                     OrbitTargets.AltitudeCm);
    SetActorLocation(StartPos, /*bSweep=*/false);

    if (auto* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        CMC->CenterCm   = OrbitTargets.CenterCm;
        CMC->AltitudeCm = OrbitTargets.AltitudeCm;
        CMC->RadiusCm   = OrbitTargets.RadiusCm;
        CMC->SpeedCms   = OrbitTargets.SpeedCms;
        CMC->AngleRad   = 0.f;
    }

    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        // Ensure next tick uses PhysCustom
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
    }
}

void APACS_CandidateHelicopterCharacter::OnRep_OrbitTargets()
{
    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        // Snap CMC working state to replicated targets as soon as they arrive
        CMC->CenterCm   = OrbitTargets.CenterCm;
        CMC->AltitudeCm = OrbitTargets.AltitudeCm;
        CMC->RadiusCm   = OrbitTargets.RadiusCm;
        CMC->SpeedCms   = OrbitTargets.SpeedCms;

        // Ensure custom mode on clients
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
        CMC->bConstrainToPlane = true;
    }
}
void APACS_CandidateHelicopterCharacter::OnRep_OrbitAnchors(){}
void APACS_CandidateHelicopterCharacter::OnRep_SelectedBy(){}