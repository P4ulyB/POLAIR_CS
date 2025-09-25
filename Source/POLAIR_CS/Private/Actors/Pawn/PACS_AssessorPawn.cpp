#include "Actors/Pawn/PACS_AssessorPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"

#include "Core/PACS_PlayerController.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Data/Configs/AssessorPawnConfig.h"

APACS_AssessorPawn::APACS_AssessorPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    AxisBasis = CreateDefaultSubobject<USceneComponent>(TEXT("AxisBasis"));
    AxisBasis->SetupAttachment(RootComponent);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(AxisBasis);
    SpringArm->bDoCollisionTest = false;  // invisible pawn; avoid camera popping
    SpringArm->TargetArmLength = 1500.f;  // overridden in ApplyConfigDefaults()

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);

    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = Config ? Config->bEnableCameraLag : true;

    // Replicate the pawn for dedicated server, but not movement (client-only navigation)
    bReplicates = true;
    bOnlyRelevantToOwner = true;
    bNetUseOwnerRelevancy = true;
    SetReplicateMovement(false);
}

void APACS_AssessorPawn::BeginPlay()
{
    Super::BeginPlay();
    ApplyConfigDefaults(); // ensure values in standalone/PIE even if possession order changes
}

void APACS_AssessorPawn::PossessedBy(AController* NewController)
{
    UE_LOG(LogTemp, Warning, TEXT("APACS_AssessorPawn::PossessedBy called with controller: %s"),
        NewController ? *NewController->GetClass()->GetName() : TEXT("NULL"));

    Super::PossessedBy(NewController);
#if !UE_SERVER
    // Try to cast to APACS_PlayerController (handles Blueprint inheritance)
    if (APACS_PlayerController* PC = Cast<APACS_PlayerController>(NewController))
    {
        UE_LOG(LogTemp, Log, TEXT("AssessorPawn: Registered with PACS on client (IsLocal=%d)"), PC->IsLocalController());
        RegisterWithInputHandler(PC);
    }
    else if (APlayerController* GenericPC = Cast<APlayerController>(NewController))
    {
        UE_LOG(LogTemp, Error, TEXT("APACS_AssessorPawn: Controller is PlayerController but not APACS_PlayerController! Class: %s"),
            *GenericPC->GetClass()->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APACS_AssessorPawn: Controller is not a PlayerController"));
    }
    ApplyConfigDefaults(); // safe: guarded by bConfigApplied
#endif
}

void APACS_AssessorPawn::UnPossessed()
{
#if !UE_SERVER
    if (APACS_PlayerController* PC = Cast<APACS_PlayerController>(GetController()))
    {
        UnregisterFromInputHandler(PC);
    }
#endif
    Super::UnPossessed();
}

bool APACS_AssessorPawn::EnsureConfigReady()
{
    if (Config)
    {
        return true;
    }

    // Try fallback soft reference (editor/PIE safe; sync load is fine for a small DA)
    if (FallbackConfig.IsValid())
    {
        Config = FallbackConfig.Get();
        return Config != nullptr;
    }

    if (FallbackConfig.ToSoftObjectPath().IsValid())
    {
        UAssessorPawnConfig* Loaded = Cast<UAssessorPawnConfig>(FallbackConfig.LoadSynchronous());
        if (Loaded)
        {
            Config = Loaded;
            return true;
        }
    }

#if WITH_EDITOR
    UE_LOG(LogTemp, Warning, TEXT("APACS_AssessorPawn: Config is null and no FallbackConfig set. Using hardcoded defaults."));
#endif
    return false;
}

void APACS_AssessorPawn::ApplyConfigDefaults()
{
    if (bConfigApplied)
    {
        return;
    }

    EnsureConfigReady(); // may or may not populate Config

    const float Tilt  = (Config ? Config->CameraTiltDegrees    : 30.f);
    const float Arm   = (Config ? Config->StartingArmLength    : 1500.f);
    const bool  bLag  = (Config ? Config->bEnableCameraLag     : true);
    const float LagSp = (Config ? Config->CameraLagSpeed       : 10.f);
    const float LagMx = (Config ? Config->CameraLagMaxDistance : 250.f);

#if WITH_EDITOR
    if (!Config)
    {
        UE_LOG(LogTemp, Warning, TEXT("APACS_AssessorPawn: ApplyConfigDefaults using hardcoded defaults (no Data Asset)."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("APACS_AssessorPawn: Using Config '%s' (Tilt=%.2f, Arm=%.1f, MoveSpeed=%.1f)"),
            *Config->GetName(), Tilt, Arm, (Config ? Config->MoveSpeed : -1.f));
    }
#endif

    // Tilt the rig downward (negative pitch)
    const FRotator BasisRot(-Tilt, 0.f, 0.f);
    SpringArm->SetRelativeRotation(BasisRot);

    // Spring Arm length & lag
    SpringArm->TargetArmLength = Arm;
    SpringArm->bEnableCameraLag = bLag;
    SpringArm->CameraLagSpeed = LagSp;
    SpringArm->CameraLagMaxDistance = LagMx;

    TargetArmLength = Arm;

    // Initialize cumulative rotation state
    CumulativeYaw = 0.0f;
    TargetCumulativeYaw = 0.0f;
    bIsRotating = false;

    bConfigApplied = true;
}

void APACS_AssessorPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Debug: Log input values if non-zero (disabled for production)
    // if (InputForward != 0.f || InputRight != 0.f)
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("APACS_AssessorPawn Tick: InputForward=%f, InputRight=%f"), InputForward, InputRight);
    // }

    // Planar movement in AxisBasis' local frame
    const FVector Fwd   = AxisBasis->GetForwardVector();
    const FVector Right = AxisBasis->GetRightVector();
    const float   Speed = (Config ? Config->MoveSpeed : 2400.f);

    // Debug: Log movement calculation (disabled for production)
    // if (InputForward != 0.f || InputRight != 0.f)
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("APACS_AssessorPawn: Fwd=%s, Right=%s, Speed=%f"),
    //         *Fwd.ToString(), *Right.ToString(), Speed);
    // }

    FVector Delta = (Fwd * InputForward + Right * InputRight) * Speed * DeltaSeconds;
    Delta.Z = 0.f;

    // Debug: Log final delta if non-zero (disabled for production)
    // if (!Delta.IsNearlyZero())
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("APACS_AssessorPawn: Moving by Delta=%s"), *Delta.ToString());
    // }

    AddActorWorldOffset(Delta, false);

    // Drive ArmLength toward Target (instant for now)
    SpringArm->TargetArmLength = TargetArmLength;

    // Update rotation interpolation
#if !UE_SERVER
    UpdateRotation(DeltaSeconds);
#endif

    // Consume per-frame inputs
    InputForward = 0.f;
    InputRight   = 0.f;
}

void APACS_AssessorPawn::StepZoom(float AxisValue)
{
    if (FMath::IsNearlyZero(AxisValue)) return;

    const float Step = (Config ? Config->ZoomStep      : 200.f);
    const float MinL = (Config ? Config->MinArmLength  : 400.f);
    const float MaxL = (Config ? Config->MaxArmLength  : 4000.f);

    TargetArmLength = FMath::Clamp(TargetArmLength + AxisValue * Step, MinL, MaxL);
}

// ===== IPACS_InputReceiver =====
EPACS_InputHandleResult APACS_AssessorPawn::HandleInputAction(FName ActionName, const FInputActionValue& Value)
{
#if !UE_SERVER
    
    if (!IsLocallyControlled())
    {
        return EPACS_InputHandleResult::NotHandled;
    }

    if (ActionName == TEXT("Assessor.MoveForward"))
    {
        InputForward += Value.Get<float>();
        return EPACS_InputHandleResult::HandledConsume;
    }
    if (ActionName == TEXT("Assessor.MoveRight"))
    {
        InputRight += Value.Get<float>();
        return EPACS_InputHandleResult::HandledConsume;
    }
    if (ActionName == TEXT("Assessor.Zoom"))
    {
        StepZoom(Value.Get<float>());
        return EPACS_InputHandleResult::HandledConsume;
    }
    if (ActionName == TEXT("Assessor.RotateLeft"))
    {
        AddRotationInput(1.0f);
        return EPACS_InputHandleResult::HandledConsume;
    }
    if (ActionName == TEXT("Assessor.RotateRight"))
    {
        AddRotationInput(-1.0f);
        return EPACS_InputHandleResult::HandledConsume;
    }
#endif

    return EPACS_InputHandleResult::NotHandled;
}

// ===== Narrow API (controller may call these directly later) =====
void APACS_AssessorPawn::AddPlanarInput(const FVector2D& Axis01)
{
    InputForward += Axis01.Y;
    InputRight   += Axis01.X;
}

void APACS_AssessorPawn::AddZoomSteps(float Steps)
{
    StepZoom(Steps);
}

void APACS_AssessorPawn::AddRotationInput(float Direction)
{
#if !UE_SERVER
    if (!Config || !Config->bRotationEnabled)
    {
        return;
    }

    if (FMath::IsNearlyZero(Direction))
    {
        return;
    }

    const float RotationStep = Config->RotationDegreesPerStep;
    const float RotationDirection = FMath::Sign(Direction);

    // Add to cumulative target (no blocking, no normalization)
    TargetCumulativeYaw += (RotationDirection * RotationStep);

    UE_LOG(LogTemp, Log, TEXT("APACS_AssessorPawn: Adding rotation %.1f degrees, target cumulative: %.1f"),
        RotationDirection * RotationStep, TargetCumulativeYaw);
#endif
}

void APACS_AssessorPawn::RegisterWithInputHandler(APACS_PlayerController* PC)
{
    if (!PC) return;

    if (UPACS_InputHandlerComponent* IH = PC->GetInputHandler())
    {
        // Register immediately even if the handler isn't "healthy" yet.
        IH->RegisterReceiver(this, GetInputPriority());
        UE_LOG(LogTemp, Warning, TEXT("AssessorPawn: registered with PACS InputHandler (may init later)"));

        // Lightweight safety: if the handler isn't ready, retry once on next tick.
        if (!IH->IsHealthy())
        {
            if (UWorld* W = GetWorld())
            {
                FTimerHandle Th;
                W->GetTimerManager().SetTimer(
                    Th,
                    FTimerDelegate::CreateWeakLambda(this, [this, PC]()
                        {
                            if (UPACS_InputHandlerComponent* IH2 = PC->GetInputHandler())
                            {
                                IH2->RegisterReceiver(this, GetInputPriority());
                                UE_LOG(LogTemp, Warning, TEXT("AssessorPawn: re-registered after handler init"));
                            }
                        }),
                    0.1f, false
                );
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AssessorPawn: PlayerController has no InputHandler"));
    }
}

void APACS_AssessorPawn::UnregisterFromInputHandler(APACS_PlayerController* PC)
{
    if (!PC) return;

    // Use proper getter method for InputHandler component
    if (UPACS_InputHandlerComponent* IH = PC->GetInputHandler())
    {
        IH->UnregisterReceiver(this);
        UE_LOG(LogTemp, Log, TEXT("APACS_AssessorPawn: Unregistered as input receiver"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APACS_AssessorPawn: GetInputHandler returned null during unregister"));
    }
}

void APACS_AssessorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    // DO NOT call Super::SetupPlayerInputComponent() to avoid SpectatorPawn's default input
    // Our PACS input system handles all input through the PlayerController's InputHandler

#if WITH_EDITOR
    UE_LOG(LogTemp, Log, TEXT("APACS_AssessorPawn: SetupPlayerInputComponent called - PACS input system will handle all input"));
#endif
}

void APACS_AssessorPawn::EnableInput(APlayerController* PlayerController)
{
    // Override to prevent SpectatorPawn from enabling its default input
    // PACS input system manages input enablement through the InputHandler

#if WITH_EDITOR
    UE_LOG(LogTemp, Log, TEXT("APACS_AssessorPawn: EnableInput called - delegating to PACS input system"));
#endif
}

void APACS_AssessorPawn::DisableInput(APlayerController* PlayerController)
{
    // Override to prevent SpectatorPawn from disabling input improperly
    // PACS input system manages input disablement through the InputHandler

#if WITH_EDITOR
    UE_LOG(LogTemp, Log, TEXT("APACS_AssessorPawn: DisableInput called - delegating to PACS input system"));
#endif
}

void APACS_AssessorPawn::OnRep_Controller()
{
    Super::OnRep_Controller();

#if !UE_SERVER
    // This runs on the owning client when the Controller pointer replicates.
    if (IsLocallyControlled())
    {
        if (APACS_PlayerController* PC = Cast<APACS_PlayerController>(GetController()))
        {
            RegisterWithInputHandler(PC);
            UE_LOG(LogTemp, Log, TEXT("AssessorPawn OnRep_Controller: registered with PACS (IsLocal=%d)"), PC->IsLocalController());

            // Ensure view target and config are applied on the client too
            PC->SetViewTarget(this);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AssessorPawn OnRep_Controller: Controller is not APACS_PlayerController (%s)"),
                *GetNameSafe(GetController()));
        }

        ApplyConfigDefaults(); // guarded by your bConfigApplied
    }
#endif
}

void APACS_AssessorPawn::UpdateRotation(float DeltaTime)
{
    if (!Config)
    {
        return;
    }

    const float InterpSpeed = Config->RotationInterpSpeed;

    // Smooth interpolation toward target cumulative rotation
    CumulativeYaw = FMath::FInterpTo(CumulativeYaw, TargetCumulativeYaw, DeltaTime, InterpSpeed);

    // Update bIsRotating state for external systems
    bIsRotating = !FMath::IsNearlyEqual(CumulativeYaw, TargetCumulativeYaw, 0.1f);

    // Apply normalized rotation to component (only normalize for component rotation)
    const FRotator NewRotation(0.0f, NormalizeYaw(CumulativeYaw), 0.0f);
    AxisBasis->SetWorldRotation(NewRotation);

    // Debug logging
    if (bIsRotating)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("APACS_AssessorPawn: Rotating - Cumulative: %.1f, Target: %.1f, Applied: %.1f"),
            CumulativeYaw, TargetCumulativeYaw, NormalizeYaw(CumulativeYaw));
    }
}

float APACS_AssessorPawn::NormalizeYaw(float Yaw)
{
    // Normalize yaw to [-180, 180] range
    while (Yaw > 180.0f)
    {
        Yaw -= 360.0f;
    }
    while (Yaw < -180.0f)
    {
        Yaw += 360.0f;
    }
    return Yaw;
}
