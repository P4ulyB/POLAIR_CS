#include "Core/PACS_PlayerController.h"
#include "Core/PACS_PlayerState.h"
#include "Core/PACSGameMode.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "Actors/Pawn/PACS_CandidateHelicopterCharacter.h"
#include "Subsystems/PACSLaunchArgSubsystem.h"
#include "Actors/NPC/PACS_NPCCharacter.h"
#include "EngineUtils.h"
#include "Components/DecalComponent.h"

#if !UE_SERVER
#include "EnhancedInputComponent.h"
#include "Data/PACS_InputTypes.h"
#include "InputActionValue.h"
#endif

APACS_PlayerController::APACS_PlayerController()
{
    InputHandler = CreateDefaultSubobject<UPACS_InputHandlerComponent>(TEXT("InputHandler"));
    EdgeScrollComponent = CreateDefaultSubobject<UPACS_EdgeScrollComponent>(TEXT("EdgeScrollComponent"));
    // HoverProbe is client-only and will be created in BeginPlay for the local controller
    // Don't create it in constructor for dedicated server multiplayer
    PrimaryActorTick.bCanEverTick = true;

}

void APACS_PlayerController::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // HoverProbe creation moved to BeginPlay with IsLocalController() check
    // Client-only components should not be created with HasAuthority()
}

void APACS_PlayerController::BeginPlay()
{
    Super::BeginPlay();
    ValidateInputSystem();

    // Create HoverProbe component on the owning client only (not server)
    // This fixes hover detection in dedicated server multiplayer
    if (IsLocalController() && !HoverProbe)
    {
        UE_LOG(LogTemp, Log, TEXT("Creating HoverProbe component for local controller"));
        HoverProbe = NewObject<UPACS_HoverProbeComponent>(this, UPACS_HoverProbeComponent::StaticClass(), TEXT("HoverProbeComponent"));
        if (HoverProbe)
        {
            HoverProbe->RegisterComponent();
            UE_LOG(LogTemp, Log, TEXT("HoverProbe component created and registered successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create HoverProbe component"));
        }
    }

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

    // Client sends PlayFab player name to server
    if (!HasAuthority())
    {
        FString PlayerName = TEXT("NoUser"); // Default fallback

        if (UGameInstance* GI = GetGameInstance())
        {
            if (UPACSLaunchArgSubsystem* LaunchArgs = GI->GetSubsystem<UPACSLaunchArgSubsystem>())
            {
                if (!LaunchArgs->Parsed.PlayFabPlayerName.IsEmpty())
                {
                    PlayerName = LaunchArgs->Parsed.PlayFabPlayerName;
                }
            }
        }

        ServerSetPlayFabPlayerName(PlayerName);
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
    else if (ActionName == TEXT("Select") || ActionName == TEXT("LeftClick"))
    {
        // Handle selection through hover probe's current target
        if (!HoverProbe)
        {
            UE_LOG(LogTemp, Error, TEXT("[SELECTION DEBUG] HoverProbe component not available"));
            return EPACS_InputHandleResult::HandledConsume;
        }

        // Get current selection for debug logging
        APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>();
        APACS_NPCCharacter* CurrentSelection = PS ? PS->GetSelectedNPC() : nullptr;

        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Player %s clicked - Current selection: %s"),
            PS ? *PS->GetPlayerName() : TEXT("Unknown"),
            CurrentSelection ? *CurrentSelection->GetName() : TEXT("None"));

        // Perform line trace to get the actor under cursor
        FHitResult HitResult;
        if (GetHitResultUnderCursor(SelectionTraceChannel, false, HitResult))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Hit actor: %s at location %s"),
                HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("None"),
                *HitResult.Location.ToString());

            if (APACS_NPCCharacter* NPC = Cast<APACS_NPCCharacter>(HitResult.GetActor()))
            {
                UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Clicked on NPC: %s (Currently selected by: %s)"),
                    *NPC->GetName(),
                    NPC->CurrentSelector ? *NPC->CurrentSelector->GetPlayerName() : TEXT("Nobody"));

                // Request selection of the NPC
                ServerRequestSelect(NPC);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Clicked on non-NPC actor: %s - deselecting"),
                    HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("Unknown"));

                // Clicked empty space - deselect
                ServerRequestDeselect();
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] No hit result - deselecting"));

            // No hit - deselect
            ServerRequestDeselect();
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("RightClick"))
    {
        // Right-click: Move selected NPC to cursor location (never deselect)
        APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>();
        APACS_NPCCharacter* SelectedNPC = nullptr;

        if (HasAuthority())
        {
            // Server: Use server-only selection tracking
            SelectedNPC = PS ? PS->GetSelectedNPC() : nullptr;
        }
        else
        {
            // Client: Find NPC that has us as CurrentSelector (replicated data)
            if (PS && GetWorld())
            {
                for (TActorIterator<APACS_NPCCharacter> ActorItr(GetWorld()); ActorItr; ++ActorItr)
                {
                    if (APACS_NPCCharacter* NPC = *ActorItr)
                    {
                        if (NPC->CurrentSelector == PS)
                        {
                            SelectedNPC = NPC;
                            break;
                        }
                    }
                }
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE DEBUG] Right-click - PlayerState: %s, SelectedNPC: %s, HasAuthority: %s"),
            PS ? *PS->GetPlayerName() : TEXT("NULL"),
            SelectedNPC ? *SelectedNPC->GetName() : TEXT("NULL"),
            HasAuthority() ? TEXT("TRUE") : TEXT("FALSE"));

        if (SelectedNPC)
        {
            // We have a selected NPC - perform line trace to get move target location
            FHitResult HitResult;
            if (GetHitResultUnderCursor(MovementTraceChannel, false, HitResult))
            {
                FVector TargetLocation = HitResult.Location;

                UE_LOG(LogTemp, Log, TEXT("[NPC MOVE] Right-click move: %s to location %s"),
                    *SelectedNPC->GetName(), *TargetLocation.ToString());

                // Send move command via PlayerController RPC (we own this connection)
                ServerRequestNPCMove(SelectedNPC, TargetLocation);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] Right-click failed - no hit result"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[NPC MOVE] Right-click ignored - no NPC selected"));
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("Deselect"))
    {
        // Explicit deselection command (separate from right-click)
        ServerRequestDeselect();
        return EPACS_InputHandleResult::HandledConsume;
    }

    // Pass through other actions
    return EPACS_InputHandleResult::NotHandled;
}

void APACS_PlayerController::ServerSetPlayFabPlayerName_Implementation(const FString& PlayerName)
{
    // Server: Epic pattern - authority check and validation
    if (HasAuthority() && PlayerState)
    {
        // Validate name (basic checks for safety)
        FString SafeName = PlayerName.IsEmpty() ? TEXT("NoUser") : PlayerName;

        // Limit name length for network efficiency
        if (SafeName.Len() > 50)
        {
            SafeName = SafeName.Left(50);
        }

        // Remove invalid characters (basic sanitization)
        SafeName = SafeName.Replace(TEXT("<"), TEXT("")).Replace(TEXT(">"), TEXT(""));

        // Use Epic's SetPlayerName - handles replication automatically
        PlayerState->SetPlayerName(SafeName);

        UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Set PlayFab player name to '%s'"), *SafeName);
    }
}

void APACS_PlayerController::ServerRequestSelect_Implementation(AActor* TargetActor)
{
    // Server authority check
    if (!HasAuthority() || !IsValid(TargetActor))
    {
        UE_LOG(LogTemp, Error, TEXT("[SELECTION DEBUG] ServerRequestSelect failed - No authority or invalid target"));
        return;
    }

    APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>();
    APACS_NPCCharacter* TargetNPC = Cast<APACS_NPCCharacter>(TargetActor);

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] ServerRequestSelect - Player: %s, Target: %s, NPC Cast: %s"),
        PS ? *PS->GetPlayerName() : TEXT("NULL"),
        *TargetActor->GetName(),
        TargetNPC ? TEXT("SUCCESS") : TEXT("FAILED"));

    if (!PS || !TargetNPC)
    {
        UE_LOG(LogTemp, Error, TEXT("[SELECTION DEBUG] ServerRequestSelect failed - PlayerState or NPC cast failed"));
        return;
    }

    // Release previous selection if any
    if (APACS_NPCCharacter* PreviousNPC = PS->GetSelectedNPC())
    {
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Releasing previous selection: %s"), *PreviousNPC->GetName());
        PreviousNPC->CurrentSelector = nullptr;
        PreviousNPC->ForceNetUpdate();
        PS->SetSelectedNPC(nullptr);
    }

    // If target is available (not selected by anyone), claim it
    if (!TargetNPC->CurrentSelector)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] SUCCESS: %s selected %s"),
            *PS->GetPlayerName(), *TargetNPC->GetName());

        TargetNPC->CurrentSelector = PS;
        TargetNPC->ForceNetUpdate();
        PS->SetSelectedNPC(TargetNPC);
    }
    else
    {
        // Already owned by someone else
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] BLOCKED: %s tried to select %s but it's already selected by %s"),
            *PS->GetPlayerName(), *TargetNPC->GetName(),
            *TargetNPC->CurrentSelector->GetPlayerName());
    }
}

void APACS_PlayerController::ServerRequestDeselect_Implementation()
{
    // Server authority check
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("[SELECTION DEBUG] ServerRequestDeselect failed - No authority"));
        return;
    }

    APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>();
    if (!PS)
    {
        UE_LOG(LogTemp, Error, TEXT("[SELECTION DEBUG] ServerRequestDeselect failed - No PlayerState"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] ServerRequestDeselect - Player: %s"), *PS->GetPlayerName());

    // Release current selection
    if (APACS_NPCCharacter* NPC = PS->GetSelectedNPC())
    {
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] SUCCESS: %s deselected %s"),
            *PS->GetPlayerName(), *NPC->GetName());

        NPC->CurrentSelector = nullptr;
        NPC->ForceNetUpdate();
        PS->SetSelectedNPC(nullptr);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] %s tried to deselect but had no selection"),
            *PS->GetPlayerName());
    }
}

void APACS_PlayerController::ServerRequestNPCMove_Implementation(APACS_NPCCharacter* TargetNPC, FVector TargetLocation)
{
    // Server authority check
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("[NPC MOVE] ServerRequestNPCMove failed - No authority"));
        return;
    }

    // Validate parameters
    if (!IsValid(TargetNPC))
    {
        UE_LOG(LogTemp, Error, TEXT("[NPC MOVE] ServerRequestNPCMove failed - Invalid target NPC"));
        return;
    }

    // Verify this player actually has the NPC selected
    APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>();
    if (!PS || PS->GetSelectedNPC() != TargetNPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] ServerRequestNPCMove rejected - Player %s doesn't have NPC %s selected"),
            PS ? *PS->GetPlayerName() : TEXT("NULL"), *TargetNPC->GetName());
        return;
    }

    // Additional check: Verify NPC thinks this player is the selector
    if (TargetNPC->CurrentSelector != PS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] ServerRequestNPCMove rejected - NPC %s selector mismatch"),
            *TargetNPC->GetName());
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[NPC MOVE] ServerRequestNPCMove validated - %s moving %s to %s"),
        *PS->GetPlayerName(), *TargetNPC->GetName(), *TargetLocation.ToString());

    // Call the NPC's movement function directly on server
    TargetNPC->ServerMoveToLocation_Implementation(TargetLocation);
}

void APACS_PlayerController::UpdateNPCDecalVisibility(bool bIsVRClient)
{
    // Only update on local client - this is a client-side rendering decision
    if (!IsLocalController())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    int32 UpdatedDecals = 0;

    // Iterate through all NPCs and update their decal visibility
    for (TActorIterator<APACS_NPCCharacter> ActorItr(World); ActorItr; ++ActorItr)
    {
        if (APACS_NPCCharacter* NPC = *ActorItr)
        {
            // Get the collision decal component
            if (UDecalComponent* CollisionDecal = NPC->GetCollisionDecal())
            {
                // Hide decals from VR clients, show to assessor clients
                bool bShouldBeVisible = !bIsVRClient;
                CollisionDecal->SetVisibility(bShouldBeVisible);
                UpdatedDecals++;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Updated %d NPC decals for %s client"),
        UpdatedDecals, bIsVRClient ? TEXT("VR") : TEXT("Assessor"));
}


