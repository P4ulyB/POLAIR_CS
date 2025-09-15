#include "PACS_PlayerController.h"
#include "PACS_PlayerState.h"
#include "PACSGameMode.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "PACS/Heli/PACS_CandidateHelicopterCharacter.h"

#if !UE_SERVER
#include "EnhancedInputComponent.h"
#include "PACS_InputTypes.h"
#include "InputActionValue.h"
#endif

APACS_PlayerController::APACS_PlayerController()
{
    InputHandler = CreateDefaultSubobject<UPACS_InputHandlerComponent>(TEXT("InputHandler"));
    PrimaryActorTick.bCanEverTick = true;

}

void APACS_PlayerController::BeginPlay()
{
    Super::BeginPlay();
    ValidateInputSystem();
    
    // TEST: Register PlayerController as input receiver for debugging
    if (InputHandler && IsLocalController())
    {
        InputHandler->RegisterReceiver(this, PACS_InputPriority::UI); // higher
        UE_LOG(LogPACSInput, Log, TEXT("PC registered as UI receiver"));
    }

    // Set up VR delegates for local controllers only
    if (IsLocalController())
    {
        OnPutOnHandle   = FCoreDelegates::VRHeadsetPutOnHead.AddUObject(this, &ThisClass::HandleHMDPutOn);
        OnRemovedHandle = FCoreDelegates::VRHeadsetRemovedFromHead.AddUObject(this, &ThisClass::HandleHMDRemoved);
        OnRecenterHandle= FCoreDelegates::VRHeadsetRecenter.AddUObject(this, &ThisClass::HandleHMDRecenter);
    }
}

void APACS_PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean up VR delegates
    FCoreDelegates::VRHeadsetPutOnHead.Remove(OnPutOnHandle);
    FCoreDelegates::VRHeadsetRemovedFromHead.Remove(OnRemovedHandle);
    FCoreDelegates::VRHeadsetRecenter.Remove(OnRecenterHandle);

    Super::EndPlay(EndPlayReason);
}

void APACS_PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    
    // Epic's pattern: ALWAYS bind here, regardless of network role
    // No deferral to OnPossess() needed
#if !UE_SERVER
    if (InputComponent && IsLocalController())
    {
        BindInputActions(); // This should happen here for ALL scenarios
    }
#endif
}

void APACS_PlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    // Epic's pattern: OnPossess() is for pawn-specific setup
    // InputHandler initialization happens here, binding already done in SetupInputComponent()
#if !UE_SERVER
    if (InputHandler && IsLocalController())
    {
        // Trigger InputHandler to update contexts now that we have a pawn
        InputHandler->OnSubsystemAvailable();
    }
#endif
}

void APACS_PlayerController::OnUnPossess()
{
#if !UE_SERVER
    if (InputHandler)
    {
        InputHandler->OnSubsystemUnavailable();
    }
#endif
    
    Super::OnUnPossess();
}

void APACS_PlayerController::ValidateInputSystem()
{
#if !UE_SERVER
    if (!InputHandler)
    {
        UE_LOG(LogPACSInput, Error, 
            TEXT("InputHandler component missing! Input will not work."));
        return;
    }
    
    if (!InputHandler->IsHealthy())
    {
        UE_LOG(LogPACSInput, Warning, 
            TEXT("InputHandler not healthy - check configuration"));
    }
#endif
}

void APACS_PlayerController::BindInputActions()
{
#if !UE_SERVER
    if (!InputHandler)
    {
        UE_LOG(LogPACSInput, Warning, 
            TEXT("Cannot bind input actions - InputHandler is null"));
        return;
    }

    // Skip binding if handler isn't initialized yet - it will call us back when ready
    if (!InputHandler->IsHealthy())
    {
        UE_LOG(LogPACSInput, Log, 
            TEXT("Deferring input binding - InputHandler not ready yet (IsHealthy=%s)"), 
            InputHandler->IsHealthy() ? TEXT("true") : TEXT("false"));
        return;
    }

    if (!InputHandler->InputConfig)
    {
        UE_LOG(LogPACSInput, Warning, 
            TEXT("Cannot bind input actions - InputConfig not set (check Blueprint configuration)"));
        return;
    }

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC)
    {
        UE_LOG(LogPACSInput, Error, TEXT("Enhanced Input Component not found!"));
        return;
    }

    // Clear any existing bindings first
    EIC->ClearActionBindings();
    UE_LOG(LogPACSInput, Log, TEXT("Cleared existing action bindings"));

    int32 BindingCount = 0;
    for (const FPACS_InputActionMapping& Mapping : InputHandler->InputConfig->ActionMappings)
    {
        if (!Mapping.InputAction)
        {
            UE_LOG(LogPACSInput, Warning, TEXT("Null InputAction for %s"), 
                *Mapping.ActionIdentifier.ToString());
            continue;
        }

        if (Mapping.bBindStarted)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Started, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogPACSInput, VeryVerbose, TEXT("  Bound %s for Started"), 
                *Mapping.ActionIdentifier.ToString());
        }
        
        if (Mapping.bBindTriggered)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Triggered, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogPACSInput, VeryVerbose, TEXT("  Bound %s for Triggered"), 
                *Mapping.ActionIdentifier.ToString());
        }
        
        if (Mapping.bBindCompleted)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Completed, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogPACSInput, VeryVerbose, TEXT("  Bound %s for Completed"), 
                *Mapping.ActionIdentifier.ToString());
        }
        
        if (Mapping.bBindOngoing)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Ongoing, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogPACSInput, VeryVerbose, TEXT("  Bound %s for Ongoing"), 
                *Mapping.ActionIdentifier.ToString());
        }
        
        if (Mapping.bBindCanceled)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Canceled, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogPACSInput, VeryVerbose, TEXT("  Bound %s for Canceled"), 
                *Mapping.ActionIdentifier.ToString());
        }
    }
    
    UE_LOG(LogPACSInput, Log, TEXT("Bound %d input actions from %d mappings (permanent bindings)"), 
        BindingCount, InputHandler->InputConfig->ActionMappings.Num());
    
    // Verify InputComponent state
    UE_LOG(LogPACSInput, Log, TEXT("InputComponent valid: %s, Handler valid: %s, Handler initialized: %s"),
        InputComponent ? TEXT("Yes") : TEXT("No"),
        InputHandler ? TEXT("Yes") : TEXT("No"),
        InputHandler->IsHealthy() ? TEXT("Yes") : TEXT("No"));
#endif
}

void APACS_PlayerController::ClientRequestHMDState_Implementation()
{
    EHMDState DetectedState = EHMDState::NoHMD;
    
    #if !UE_SERVER
    // Use XRBase API for robust HMD detection - check both connected and enabled
    if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayConnected() && 
        UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
    {
        DetectedState = EHMDState::HasHMD;
        UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: HMD detected and enabled"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS PlayerController: HMD not detected or not enabled"));
    }
    #else
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Server build - defaulting to NoHMD"));
    #endif

    ServerReportHMDState(DetectedState);
}

void APACS_PlayerController::ServerReportHMDState_Implementation(EHMDState DetectedState)
{
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Server received HMD state %d"), static_cast<int32>(DetectedState));
    
    // Guard PlayerState access with null check
    if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PlayerState))
    {
        // Store previous state to detect transitions
        EHMDState PreviousState = PACSPS->HMDState;
        PACSPS->HMDState = DetectedState;
        
        // Only trigger spawn if state transitioned from Unknown and player has no pawn
        if (PreviousState == EHMDState::Unknown && GetPawn() == nullptr)
        {
            UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Triggering spawn for player with HMD state %d"), static_cast<int32>(DetectedState));
            if (APACSGameMode* GM = GetWorld()->GetAuthGameMode<APACSGameMode>())
            {
                GM->HandleStartingNewPlayer(this);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS PlayerController: Spawn not triggered - PreviousState: %d, HasPawn: %s"), 
                static_cast<int32>(PreviousState), GetPawn() ? TEXT("true") : TEXT("false"));
        }
    }
    else
    {
        // PlayerState null - queue HMD state for when it becomes available
        UE_LOG(LogTemp, Warning, TEXT("PACS PlayerController: PlayerState null - queueing HMD state"));
        PendingHMDState = DetectedState;
        bHasPendingHMDState = true;
    }
}

void APACS_PlayerController::InitPlayerState()
{
    Super::InitPlayerState();
    
    // Server-side PlayerState initialization - apply any pending HMD state
    if (bHasPendingHMDState)
    {
        UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Applying pending HMD state %d"), static_cast<int32>(PendingHMDState));
        
        if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PlayerState))
        {
            PACSPS->HMDState = PendingHMDState;
            bHasPendingHMDState = false;
            
            // Only trigger spawn if player has no pawn
            if (GetPawn() == nullptr)
            {
                if (APACSGameMode* GM = GetWorld()->GetAuthGameMode<APACSGameMode>())
                {
                    GM->HandleStartingNewPlayer(this);
                }
            }
        }
    }
}

// VR Handler implementations
void APACS_PlayerController::HandleHMDPutOn()
{
    if (auto* Heli = Cast<APACS_CandidateHelicopterCharacter>(GetPawn()))
        Heli->CenterSeatedPose(true);
}

void APACS_PlayerController::HandleHMDRecenter()
{
    HandleHMDPutOn();
}

void APACS_PlayerController::HandleHMDRemoved()
{
    // No action needed when HMD is removed
}

void APACS_PlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (bShowInputContextDebug && IsLocalPlayerController())
    {
        DisplayInputContextDebug();
    }
}

void APACS_PlayerController::DisplayInputContextDebug()
{
    if (!GEngine || !IsLocalPlayerController() || !InputHandler)
    {
        return;
    }
    
    FString DebugText = FString::Printf(TEXT("Input Context: %s"), *InputHandler->GetCurrentContextName());
    
    // Display persistent debug message at top-left
    GEngine->AddOnScreenDebugMessage(
        -1, // Use -1 for persistent message that updates
        0.0f, // No duration (persistent)
        FColor::Yellow,
        DebugText,
        true, // Newer message overrides older ones
        FVector2D(1.2f, 1.2f) // Slightly larger text
    );
}

EPACS_InputHandleResult APACS_PlayerController::HandleInputAction(FName ActionName, const FInputActionValue& Value)
{  
    if (ActionName == TEXT("MenuToggle"))
    {
        if (InputHandler)
        {
            InputHandler->ToggleMenuContext();
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("UI"))
    {
        if (InputHandler)
        {
            InputHandler->ToggleUIContext();
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    
    // Pass through other actions
    return EPACS_InputHandleResult::NotHandled;
}

