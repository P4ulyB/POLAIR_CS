#include "Actors/Pawn/PACS_CandidateHelicopterCharacter.h"
#include "Components/PACS_HeliMovementComponent.h"
#include "Data/Configs/PACS_CandidateHelicopterData.h"
#include "Data/PACS_OrbitMessages.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Core/PACS_PlayerController.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

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

    // CCTV Camera System 1 Setup (Rotates with helicopter)
    MonitorPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CCTV_MonitorPlane"));
    MonitorPlane->SetupAttachment(HelicopterFrame);  // Consistent with VR hierarchy
    MonitorPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ExternalCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CCTV_ExternalCam"));
    ExternalCam->SetupAttachment(HelicopterFrame);  // Rotates with helicopter
    ExternalCam->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    ExternalCam->bCaptureEveryFrame = true;
    ExternalCam->bCaptureOnMovement = true;
    ExternalCam->FOVAngle = NormalFOV;

    // CCTV Camera System 2 Setup (Static world rotation)
    MonitorPlane2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CCTV_MonitorPlane2"));
    MonitorPlane2->SetupAttachment(HelicopterFrame);
    MonitorPlane2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Position second monitor slightly offset from first
    MonitorPlane2->SetRelativeLocation(FVector(0, 50, 0)); // Adjust as needed

    ExternalCam2 = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CCTV_ExternalCam2"));
    ExternalCam2->SetupAttachment(MonitorPlane2);  // Nested under MonitorPlane2 for logical hierarchy
    ExternalCam2->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    ExternalCam2->bCaptureEveryFrame = true;
    ExternalCam2->bCaptureOnMovement = true;
    ExternalCam2->FOVAngle = NormalFOV;
    ExternalCam2->SetRelativeRotation(FRotator(0, -90, 0));
    // Set absolute rotation to prevent inheriting parent rotation
    ExternalCam2->SetAbsolute(false, true, false); // Only rotation is absolute

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 180.f, 0);
}

void APACS_CandidateHelicopterCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Setup both CCTV systems
    SetupCCTV();
    SetupCCTV2();

    // Initialize static camera world rotation with configurable settings
    if (ExternalCam2)
    {
        // Set static world rotation: Pitch = -90 (facing ground), Yaw = configurable
        StaticCameraWorldRotation = FRotator(-90.0f, Camera2YAxisRotation, Camera2ZAxisRotation);
        ExternalCam2->SetWorldRotation(StaticCameraWorldRotation);
    }

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

    RegisterAsReceiverIfLocal();
}

void APACS_CandidateHelicopterCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();

    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
    }

    RegisterAsReceiverIfLocal();
}

void APACS_CandidateHelicopterCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateBankVisual(DeltaSeconds);
    UpdateStaticCameraPosition(DeltaSeconds);
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
    
    float TargetYaw = 0.f;
    if (bSnapYawToVehicleForward && HelicopterFrame)
    {
        // Get the helicopter mesh's local rotation yaw to align VR with the mesh's local orientation
        TargetYaw = HelicopterFrame->GetRelativeRotation().Yaw;
    }
    
    UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(
        TargetYaw,
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
    const float Target = -(CMC->SpeedCms / FMath::Max(Data->MaxSpeedCms,1.f)) * Data->MaxBankDeg; //<-------- Change bank direction
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

void APACS_CandidateHelicopterCharacter::UnPossessed()
{
    UnregisterAsReceiver();
    Super::UnPossessed();
}

void APACS_CandidateHelicopterCharacter::RegisterAsReceiverIfLocal()
{
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(Controller))
    {
        if (PACSPC->IsLocalController())
        {
            // Direct access to InputHandler component via getter
            if (UPACS_InputHandlerComponent* IH = PACSPC->GetInputHandler())
            {
                // Validate InputHandler is ready before registering
                if (IH->IsHealthy())
                {
                    IH->RegisterReceiver(this, GetInputPriority());
                    UE_LOG(LogPACSInput, Log, TEXT("Helicopter registered as input receiver"));
                }
                else
                {
                    UE_LOG(LogPACSInput, Warning, TEXT("Deferring helicopter registration - InputHandler not ready"));
                }
            }
            else
            {
                UE_LOG(LogPACSInput, Error, TEXT("InputHandler component not found on PlayerController"));
            }
        }
    }
}

void APACS_CandidateHelicopterCharacter::UnregisterAsReceiver()
{
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(Controller))
    {
        if (UPACS_InputHandlerComponent* IH = PACSPC->GetInputHandler())
        {
            IH->UnregisterReceiver(this);
            UE_LOG(LogPACSInput, Log, TEXT("Helicopter unregistered as input receiver"));
        }
    }
}

// ---- Input Receiver ----
EPACS_InputHandleResult APACS_CandidateHelicopterCharacter::HandleInputAction(FName ActionName, const FInputActionValue& Value)
{
    // Debug: Log ALL actions received by helicopter
    UE_LOG(LogPACSInput, Warning, TEXT("Helicopter received action: %s (Value: %s, Type: %s)"), 
        *ActionName.ToString(), *Value.ToString(), 
        Value.GetValueType() == EInputActionValueType::Axis1D ? TEXT("Axis1D") :
        Value.GetValueType() == EInputActionValueType::Boolean ? TEXT("Boolean") : TEXT("Other"));

    // The router already resolves UInputAction* -> FName via config
    if (ActionName == TEXT("VRSeat.Center"))
    {
        UE_LOG(LogPACSInput, Warning, TEXT("EXECUTING VRSeat.Center"));
        Seat_Center();
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("VRSeat.X"))
    {
        UE_LOG(LogPACSInput, Warning, TEXT("PROCESSING VRSeat.X"));
        float AxisValue = Value.Get<float>();
        UE_LOG(LogPACSInput, Warning, TEXT("VRSeat.X AxisValue: %f (abs: %f)"), AxisValue, FMath::Abs(AxisValue));
        
        // Test: Move even with zero values to check if keys are working at all
        if (HelicopterFrame)
        {
            FVector CurrentPos = HelicopterFrame->GetRelativeLocation();
            FVector NewPos = CurrentPos;
            // If axis value is zero, assume it's a button press and use fixed movement
            float MovementValue = (FMath::Abs(AxisValue) > 0.001f) ? AxisValue : 1.0f;
            NewPos.X += MovementValue * 4.0f; // Larger step for visibility
            HelicopterFrame->SetRelativeLocation(NewPos);
            UE_LOG(LogPACSInput, Error, TEXT("VRSeat.X MOVED: %f -> Frame X: %f to %f (using movement: %f)"), AxisValue, CurrentPos.X, NewPos.X, MovementValue);
        }
        else
        {
            UE_LOG(LogPACSInput, Error, TEXT("VRSeat.X NOT MOVED: HelicopterFrame=%s, AxisValue=%f"), 
                HelicopterFrame ? TEXT("Valid") : TEXT("NULL"), AxisValue);
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("VRSeat.Y"))
    {
        UE_LOG(LogPACSInput, Warning, TEXT("PROCESSING VRSeat.Y"));
        float AxisValue = Value.Get<float>();
        UE_LOG(LogPACSInput, Warning, TEXT("VRSeat.Y AxisValue: %f"), AxisValue);
        
        if (HelicopterFrame)
        {
            FVector CurrentPos = HelicopterFrame->GetRelativeLocation();
            FVector NewPos = CurrentPos;
            float MovementValue = (FMath::Abs(AxisValue) > 0.001f) ? AxisValue : 1.0f;
            NewPos.Y += MovementValue * 4.0f;
            HelicopterFrame->SetRelativeLocation(NewPos);
            UE_LOG(LogPACSInput, Error, TEXT("VRSeat.Y MOVED: %f -> Frame Y: %f to %f (using movement: %f)"), AxisValue, CurrentPos.Y, NewPos.Y, MovementValue);
        }
        else
        {
            UE_LOG(LogPACSInput, Error, TEXT("VRSeat.Y NOT MOVED: HelicopterFrame=%s, AxisValue=%f"), 
                HelicopterFrame ? TEXT("Valid") : TEXT("NULL"), AxisValue);
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("VRSeat.Z"))
    {
        UE_LOG(LogPACSInput, Warning, TEXT("PROCESSING VRSeat.Z"));
        float AxisValue = Value.Get<float>();
        UE_LOG(LogPACSInput, Warning, TEXT("VRSeat.Z AxisValue: %f"), AxisValue);
        
        if (HelicopterFrame)
        {
            FVector CurrentPos = HelicopterFrame->GetRelativeLocation();
            FVector NewPos = CurrentPos;
            float MovementValue = (FMath::Abs(AxisValue) > 0.001f) ? AxisValue : 1.0f;
            NewPos.Z += MovementValue * 4.0f;
            HelicopterFrame->SetRelativeLocation(NewPos);
            UE_LOG(LogPACSInput, Error, TEXT("VRSeat.Z MOVED: %f -> Frame Z: %f to %f (using movement: %f)"), AxisValue, CurrentPos.Z, NewPos.Z, MovementValue);
        }
        else
        {
            UE_LOG(LogPACSInput, Error, TEXT("VRSeat.Z NOT MOVED: HelicopterFrame=%s, AxisValue=%f"), 
                HelicopterFrame ? TEXT("Valid") : TEXT("NULL"), AxisValue);
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("Cam.ZoomToggle"))
    {
        const bool bPressed = Value.Get<bool>();
        UE_LOG(LogPACSInput, Warning, TEXT("CCTV: Cam.ZoomToggle received - Value: %s"), bPressed ? TEXT("true") : TEXT("false"));

        // Since we're only receiving 'false' (release) events, toggle on release instead
        if (!bPressed)
        {
            ToggleCamZoom();
            UE_LOG(LogPACSInput, Log, TEXT("CCTV Zoom toggled: %s"), bCCTVZoomed ? TEXT("Zoomed") : TEXT("Normal"));
        }
        // Always consume the action whether pressed or released
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("Cam2.ZoomToggle"))
    {
        const bool bPressed = Value.Get<bool>();
        UE_LOG(LogPACSInput, Warning, TEXT("CCTV2: Cam2.ZoomToggle received - Value: %s"), bPressed ? TEXT("true") : TEXT("false"));

        // Toggle on release to match behavior of Cam1
        if (!bPressed)
        {
            ToggleCam2Zoom();
            UE_LOG(LogPACSInput, Log, TEXT("CCTV2 Zoom toggled: %s"), bCCTV2Zoomed ? TEXT("Zoomed") : TEXT("Normal"));
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    
    // Log unhandled actions
    UE_LOG(LogPACSInput, Warning, TEXT("Helicopter did NOT handle action: %s"), *ActionName.ToString());
    return EPACS_InputHandleResult::NotHandled;
}

// ---- Seat helpers (local-only visual offsets you already support) ----
void APACS_CandidateHelicopterCharacter::Seat_Center()
{
    CenterSeatedPose(/*bSnapYawToVehicleForward*/ true);
}
void APACS_CandidateHelicopterCharacter::Seat_X(float Axis)
{
    NudgeSeatX(Axis * SeatNudgeStepCm);
}
void APACS_CandidateHelicopterCharacter::Seat_Y(float Axis)
{
    NudgeSeatY(Axis * SeatNudgeStepCm);
}
void APACS_CandidateHelicopterCharacter::Seat_Z(float Axis)
{
    NudgeSeatZ(Axis * SeatNudgeStepCm);
}

// ---- CCTV System ----
void APACS_CandidateHelicopterCharacter::SetupCCTV()
{
    // Create render target with proper settings
    CameraRT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_CCTV"));
    if (CameraRT)
    {
        CameraRT->InitAutoFormat(RT_Resolution, RT_Resolution);
        CameraRT->ClearColor = FLinearColor::Black;
        CameraRT->TargetGamma = 2.2f;
        CameraRT->bAutoGenerateMips = false;  // Performance optimisation
        
        UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Render target created (%dx%d)"), 
            RT_Resolution, RT_Resolution);
    }

    // Configure camera capture
    if (ExternalCam && CameraRT)
    {
        ExternalCam->TextureTarget = CameraRT;

        // Configure projection type from data asset
        if (Data && Data->bCamera1UseOrtho)
        {
            ExternalCam->ProjectionType = ECameraProjectionMode::Orthographic;
            if (Data->Camera1OrthoWidths.Num() > 0)
            {
                ExternalCam->OrthoWidth = Data->Camera1OrthoWidths[0];
                Camera1ZoomIndex = 0; // Reset zoom index
            }
            else
            {
                ExternalCam->OrthoWidth = 1000.0f; // Default fallback
            }
            UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Camera 1 set to Orthographic, OrthoWidth: %.1f"), ExternalCam->OrthoWidth);
        }
        else
        {
            ExternalCam->ProjectionType = ECameraProjectionMode::Perspective;
            ExternalCam->FOVAngle = NormalFOV;
            UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Camera 1 set to Perspective, FOV: %.1f"), ExternalCam->FOVAngle);
        }

        // Optimise for VR performance
        ExternalCam->bCaptureEveryFrame = true;
        ExternalCam->bCaptureOnMovement = false;  // Better performance

        UE_LOG(LogTemp, Log, TEXT("PACS CCTV: External camera configured"));
    }

    // Setup monitor material
    if (MonitorPlane && ScreenBaseMaterial)
    {
        ScreenMID = UMaterialInstanceDynamic::Create(ScreenBaseMaterial, this);
        if (ScreenMID && CameraRT)
        {
            ScreenMID->SetTextureParameterValue(TEXT("ScreenTex"), CameraRT);
            MonitorPlane->SetMaterial(0, ScreenMID);
            
            UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Monitor material configured"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS CCTV: Failed to create screen material"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS CCTV: Missing components - MonitorPlane: %s, ScreenBaseMaterial: %s"),
            MonitorPlane ? TEXT("Valid") : TEXT("NULL"),
            ScreenBaseMaterial ? TEXT("Valid") : TEXT("NULL"));
    }
}

void APACS_CandidateHelicopterCharacter::ToggleCamZoom()
{
    // Now calls the new cycle zoom function
    CycleCamera1Zoom();
}

void APACS_CandidateHelicopterCharacter::CycleCamera1Zoom()
{
    if (!ExternalCam || !Data)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS CCTV: ExternalCam or Data is null"));
        return;
    }

    // Handle orthographic projection
    if (Data->bCamera1UseOrtho && Data->Camera1OrthoWidths.Num() > 0)
    {
        // Cycle to next zoom level
        Camera1ZoomIndex = (Camera1ZoomIndex + 1) % Data->Camera1OrthoWidths.Num();

        // Apply new ortho width
        ExternalCam->OrthoWidth = Data->Camera1OrthoWidths[Camera1ZoomIndex];

        UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Camera 1 Zoom Level %d of %d, OrthoWidth: %.1f"),
            Camera1ZoomIndex + 1, Data->Camera1OrthoWidths.Num(), ExternalCam->OrthoWidth);
    }
    // Handle perspective projection (legacy)
    else
    {
        bCCTVZoomed = !bCCTVZoomed;
        ExternalCam->FOVAngle = bCCTVZoomed ? ZoomFOV : NormalFOV;

        UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Perspective Zoom %s (FOV: %.1f°)"),
            bCCTVZoomed ? TEXT("IN") : TEXT("OUT"), ExternalCam->FOVAngle);
    }
}

void APACS_CandidateHelicopterCharacter::SetupCCTV2()
{
    // Create render target for second camera
    CameraRT2 = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_CCTV2"));
    if (CameraRT2)
    {
        CameraRT2->InitAutoFormat(RT_Resolution, RT_Resolution);
        CameraRT2->ClearColor = FLinearColor::Black;
        CameraRT2->TargetGamma = 2.2f;
        CameraRT2->bAutoGenerateMips = false;  // Performance optimization

        UE_LOG(LogTemp, Log, TEXT("PACS CCTV2: Render target created (%dx%d)"),
            RT_Resolution, RT_Resolution);
    }

    // Configure second camera capture
    if (ExternalCam2 && CameraRT2)
    {
        ExternalCam2->TextureTarget = CameraRT2;

        // Configure projection type from data asset
        if (Data && Data->bCamera2UseOrtho)
        {
            ExternalCam2->ProjectionType = ECameraProjectionMode::Orthographic;
            if (Data->Camera2OrthoWidths.Num() > 0)
            {
                ExternalCam2->OrthoWidth = Data->Camera2OrthoWidths[0];
                Camera2ZoomIndex = 0; // Reset zoom index
            }
            else
            {
                ExternalCam2->OrthoWidth = 1000.0f; // Default fallback
            }
            UE_LOG(LogTemp, Log, TEXT("PACS CCTV2: Camera 2 set to Orthographic, OrthoWidth: %.1f"), ExternalCam2->OrthoWidth);
        }
        else
        {
            ExternalCam2->ProjectionType = ECameraProjectionMode::Perspective;
            ExternalCam2->FOVAngle = NormalFOV;
            UE_LOG(LogTemp, Log, TEXT("PACS CCTV2: Camera 2 set to Perspective, FOV: %.1f"), ExternalCam2->FOVAngle);
        }

        // Optimize for VR performance
        ExternalCam2->bCaptureEveryFrame = true;
        ExternalCam2->bCaptureOnMovement = false;  // Better performance

        // Position camera with configurable offset
        ExternalCam2->SetRelativeLocation(Camera2PositionOffset);

        // Set initial rotation (pitch -90 degrees to face ground, configurable yaw)
        FRotator CameraRotation(-90.0f, Camera2YAxisRotation, Camera2ZAxisRotation);
        ExternalCam2->SetRelativeRotation(CameraRotation);

        UE_LOG(LogTemp, Log, TEXT("PACS CCTV2: External camera 2 configured (static world rotation)"));
    }

    // Setup second monitor material
    if (MonitorPlane2 && ScreenBaseMaterial)
    {
        ScreenMID2 = UMaterialInstanceDynamic::Create(ScreenBaseMaterial, this);
        if (ScreenMID2 && CameraRT2)
        {
            ScreenMID2->SetTextureParameterValue(TEXT("ScreenTex"), CameraRT2);
            MonitorPlane2->SetMaterial(0, ScreenMID2);

            UE_LOG(LogTemp, Log, TEXT("PACS CCTV2: Monitor 2 material configured"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS CCTV2: Failed to create screen material 2"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS CCTV2: Missing components - MonitorPlane2: %s, ScreenBaseMaterial: %s"),
            MonitorPlane2 ? TEXT("Valid") : TEXT("NULL"),
            ScreenBaseMaterial ? TEXT("Valid") : TEXT("NULL"));
    }
}

void APACS_CandidateHelicopterCharacter::ToggleCam2Zoom()
{
    // Now calls the new cycle zoom function
    CycleCamera2Zoom();
}

void APACS_CandidateHelicopterCharacter::CycleCamera2Zoom()
{
    if (!ExternalCam2 || !Data)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS CCTV2: ExternalCam2 or Data is null"));
        return;
    }

    // Handle orthographic projection
    if (Data->bCamera2UseOrtho && Data->Camera2OrthoWidths.Num() > 0)
    {
        // Cycle to next zoom level
        Camera2ZoomIndex = (Camera2ZoomIndex + 1) % Data->Camera2OrthoWidths.Num();

        // Apply new ortho width
        ExternalCam2->OrthoWidth = Data->Camera2OrthoWidths[Camera2ZoomIndex];

        UE_LOG(LogTemp, Log, TEXT("PACS CCTV2: Camera 2 Zoom Level %d of %d, OrthoWidth: %.1f"),
            Camera2ZoomIndex + 1, Data->Camera2OrthoWidths.Num(), ExternalCam2->OrthoWidth);
    }
    // Handle perspective projection (legacy)
    else
    {
        bCCTV2Zoomed = !bCCTV2Zoomed;
        ExternalCam2->FOVAngle = bCCTV2Zoomed ? ZoomFOV : NormalFOV;

        UE_LOG(LogTemp, Log, TEXT("PACS CCTV2: Perspective Zoom %s (FOV: %.1f°)"),
            bCCTV2Zoomed ? TEXT("IN") : TEXT("OUT"), ExternalCam2->FOVAngle);
    }
}

void APACS_CandidateHelicopterCharacter::UpdateStaticCameraPosition(float DeltaSeconds)
{
    if (!ExternalCam2)
    {
        return;
    }

    // Update camera location to follow the helicopter using configurable offset
    FVector ActorLocation = GetActorLocation();

    // Set world location (follows helicopter position with configurable offset)
    ExternalCam2->SetWorldLocation(ActorLocation + Camera2PositionOffset);

    // Maintain static world rotation with configurable Y-axis rotation
    // Pitch = -90 (facing ground), Yaw = configurable, Roll = 0
    FRotator CameraRotation(-90.0f, Camera2YAxisRotation, Camera2ZAxisRotation);
    ExternalCam2->SetWorldRotation(CameraRotation);
}

void APACS_CandidateHelicopterCharacter::ApplyOrthoSettings(USceneCaptureComponent2D* Camera, bool bUseOrtho, float OrthoWidth)
{
    if (!Camera)
    {
        return;
    }

    if (bUseOrtho)
    {
        Camera->ProjectionType = ECameraProjectionMode::Orthographic;
        Camera->OrthoWidth = OrthoWidth;
        UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Applied Orthographic projection with width: %.1f"), OrthoWidth);
    }
    else
    {
        Camera->ProjectionType = ECameraProjectionMode::Perspective;
        Camera->FOVAngle = NormalFOV;
        UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Applied Perspective projection with FOV: %.1f"), Camera->FOVAngle);
    }
}