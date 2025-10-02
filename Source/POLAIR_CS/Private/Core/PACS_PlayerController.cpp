#include "Core/PACS_PlayerController.h"
#include "Core/PACS_PlayerState.h"
#include "Core/PACSGameMode.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture2D.h"
#include "Actors/Pawn/PACS_CandidateHelicopterCharacter.h"
#include "Actors/NPC/PACS_NPC_Base.h"
#include "Actors/NPC/PACS_NPC_Base_Char.h"
#include "Actors/NPC/PACS_NPC_Base_Veh.h"
#include "Interfaces/PACS_Poolable.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Subsystems/PACSLaunchArgSubsystem.h"
#include "Subsystems/PACS_SpawnOrchestrator.h"
#include "Subsystems/PACS_MemoryTracker.h"
#include "Data/PACS_SpawnConfig.h"
#include "EngineUtils.h"
#include "Components/DecalComponent.h"
#include "Components/PACS_NPCBehaviorComponent.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/Pawn/PACS_AssessorPawn.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/PACS_SpawnButtonWidget.h"
#include "UI/PACS_SpawnListWidget.h"
// Removed Landscape include - simplified check below

#if !UE_SERVER
#include "EnhancedInputComponent.h"
#include "Data/PACS_InputTypes.h"
#include "InputActionValue.h"
#endif

APACS_PlayerController::APACS_PlayerController()
{
    InputHandler = CreateDefaultSubobject<UPACS_InputHandlerComponent>(TEXT("InputHandler"));
    EdgeScrollComponent = CreateDefaultSubobject<UPACS_EdgeScrollComponent>(TEXT("EdgeScrollComponent"));
    NPCBehaviorComponent = CreateDefaultSubobject<UPACS_NPCBehaviorComponent>(TEXT("NPCBehaviorComponent"));

    PrimaryActorTick.bCanEverTick = true;
}

void APACS_PlayerController::PostInitializeComponents()
{
    Super::PostInitializeComponents();
}

void APACS_PlayerController::BeginPlay()
{
    Super::BeginPlay();
    ValidateInputSystem();

    if (IsLocalController() && !HoverProbe)
    {
        UE_LOG(LogTemp, Log, TEXT("Creating HoverProbe component for local controller"));
        HoverProbe = NewObject<UPACS_HoverProbeComponent>(this, UPACS_HoverProbeComponent::StaticClass(), TEXT("HoverProbeComponent"));
        if (HoverProbe)
        {
            HoverProbe->RegisterComponent();

            // Apply configuration from PlayerController properties
            // If HoverProbeObjectTypes is empty, set default to SelectionObject
            TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = HoverProbeObjectTypes;
            if (ObjectTypes.Num() == 0)
            {
                // Default to SelectionObject type (ECC_GameTraceChannel2)
                ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel2));
                UE_LOG(LogTemp, Log, TEXT("HoverProbe: No object types configured, defaulting to SelectionObject (ECC_GameTraceChannel2)"));
            }

            HoverProbe->ApplyConfiguration(
                HoverProbeActiveContexts,
                ObjectTypes,
                HoverProbeRateHz,
                bHoverProbeConfirmVisibility
            );

            // Force enable tick as an extra safety measure
            HoverProbe->SetComponentTickEnabled(true);
            HoverProbe->PrimaryComponentTick.bCanEverTick = true;
            HoverProbe->PrimaryComponentTick.bStartWithTickEnabled = true;

            UE_LOG(LogTemp, Warning, TEXT("HoverProbe component created - TickEnabled=%d, CanEverTick=%d, Owner=%s"),
                HoverProbe->IsComponentTickEnabled(),
                HoverProbe->PrimaryComponentTick.bCanEverTick,
                *GetName());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create HoverProbe component"));
        }
    }

    // Register PlayerController as input receiver for debugging
    if (InputHandler && IsLocalController())
    {
        InputHandler->RegisterReceiver(this, PACS_InputPriority::UI);
        UE_LOG(LogTemp, Log, TEXT("PC registered as UI receiver"));
    }

    // Create spawn UI for local players (delayed to ensure subsystems are ready)
    if (IsLocalController())
    {
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::BeginPlay - Scheduling CreateSpawnUI for next tick (IsLocalController=true)"));
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            CreateSpawnUI();
        });
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::BeginPlay - Skipping CreateSpawnUI (IsLocalController=false, Role=%s)"),
            *UEnum::GetValueAsString(GetLocalRole()));
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
        UE_LOG(LogTemp, Error, 
            TEXT("InputHandler component missing! Input will not work."));
        return;
    }
    
    if (!InputHandler->IsHealthy())
    {
        UE_LOG(LogTemp, Warning, 
            TEXT("InputHandler not healthy - check configuration"));
    }
#endif
}

void APACS_PlayerController::BindInputActions()
{
#if !UE_SERVER
    if (!InputHandler)
    {
        UE_LOG(LogTemp, Warning, 
            TEXT("Cannot bind input actions - InputHandler is null"));
        return;
    }

    // Skip binding if handler isn't initialized yet - it will call us back when ready
    if (!InputHandler->IsHealthy())
    {
        UE_LOG(LogTemp, Log, 
            TEXT("Deferring input binding - InputHandler not ready yet (IsHealthy=%s)"), 
            InputHandler->IsHealthy() ? TEXT("true") : TEXT("false"));
        return;
    }

    if (!InputHandler->InputConfig)
    {
        UE_LOG(LogTemp, Warning, 
            TEXT("Cannot bind input actions - InputConfig not set (check Blueprint configuration)"));
        return;
    }

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC)
    {
        UE_LOG(LogTemp, Error, TEXT("Enhanced Input Component not found!"));
        return;
    }

    // Clear any existing bindings first
    EIC->ClearActionBindings();
    UE_LOG(LogTemp, Log, TEXT("Cleared existing action bindings"));

    int32 BindingCount = 0;
    for (const FPACS_InputActionMapping& Mapping : InputHandler->InputConfig->ActionMappings)
    {
        if (!Mapping.InputAction)
        {
            UE_LOG(LogTemp, Warning, TEXT("Null InputAction for %s"), 
                *Mapping.ActionIdentifier.ToString());
            continue;
        }

        if (Mapping.bBindStarted)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Started, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogTemp, VeryVerbose, TEXT("  Bound %s for Started"), 
                *Mapping.ActionIdentifier.ToString());
        }
        
        if (Mapping.bBindTriggered)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Triggered, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogTemp, VeryVerbose, TEXT("  Bound %s for Triggered"), 
                *Mapping.ActionIdentifier.ToString());
        }
        
        if (Mapping.bBindCompleted)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Completed, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogTemp, VeryVerbose, TEXT("  Bound %s for Completed"), 
                *Mapping.ActionIdentifier.ToString());
        }
        
        if (Mapping.bBindOngoing)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Ongoing, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogTemp, VeryVerbose, TEXT("  Bound %s for Ongoing"), 
                *Mapping.ActionIdentifier.ToString());
        }
        
        if (Mapping.bBindCanceled)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Canceled, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
            BindingCount++;
            UE_LOG(LogTemp, VeryVerbose, TEXT("  Bound %s for Canceled"), 
                *Mapping.ActionIdentifier.ToString());
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Bound %d input actions from %d mappings (permanent bindings)"), 
        BindingCount, InputHandler->InputConfig->ActionMappings.Num());
    
    // Verify InputComponent state
    UE_LOG(LogTemp, Log, TEXT("InputComponent valid: %s, Handler valid: %s, Handler initialized: %s"),
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

    // Handle marquee drag detection
    if (bLeftMousePressed && !bIsMarqueeActive)
    {
        FVector2D CurrentPos;
        if (GetMousePosition(CurrentPos.X, CurrentPos.Y))
        {
            // Check if mouse has moved beyond drag threshold
            const float DragDistance = FVector2D::Distance(CurrentPos, MarqueeStartPos);
            if (DragDistance > MarqueeDragThreshold)
            {
                UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] Drag threshold exceeded: %f > %f"),
                    DragDistance, MarqueeDragThreshold);
                StartMarquee();
            }
        }
    }

    // Update marquee current position while dragging
    if (bIsMarqueeActive)
    {
        GetMousePosition(MarqueeCurrentPos.X, MarqueeCurrentPos.Y);
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
    // PRIORITY 1: Handle spawn placement inputs when in placement mode (GameplayTag-based system)
    // This must be checked BEFORE marquee selection to have higher priority
    if (bSpawnPlacementMode && (ActionName == TEXT("LeftClick") || ActionName == TEXT("Select") ||
                                  ActionName == TEXT("RightClick") || ActionName == TEXT("Cancel")))
    {
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Handling input in spawn placement mode - Action: %s"),
            *ActionName.ToString());

        // Left click to confirm placement
        if (ActionName == TEXT("LeftClick") || ActionName == TEXT("Select"))
        {
            const float Magnitude = Value.GetMagnitude();
            // Only on button release (Completed event)
            if (Magnitude == 0.0f)
            {
                UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Left click released - placing NPC"));
                HandlePlaceNPCAction(Value);
                return EPACS_InputHandleResult::HandledConsume;
            }
        }
        // Right click to cancel placement
        else if (ActionName == TEXT("RightClick") || ActionName == TEXT("Cancel"))
        {
            const float Magnitude = Value.GetMagnitude();
            // Only on button release
            if (Magnitude == 0.0f)
            {
                UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Right click released - cancelling placement"));
                HandleCancelPlacementAction(Value);
                return EPACS_InputHandleResult::HandledConsume;
            }
        }
    }
    // PRIORITY 2: Marquee selection system - only when NOT in spawn placement mode
    else if (ActionName == TEXT("Select") || ActionName == TEXT("LeftClick"))
    {
        // Enhanced Input sends discrete Started and Completed events
        // Started event: magnitude > 0 when the button is first pressed
        // Completed event: magnitude == 0 when the button is released
        const float Magnitude = Value.GetMagnitude();

        UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] LeftClick: Magnitude=%f, bIsLeftClickHeld=%d"),
            Magnitude, bIsLeftClickHeld);

        // Detect Started event (button pressed)
        if (Magnitude > 0.0f && !bIsLeftClickHeld)
        {
            bIsLeftClickHeld = true;
            bLeftMousePressed = true;

            // Record start position for potential marquee
            GetMousePosition(MarqueeStartPos.X, MarqueeStartPos.Y);
            MarqueeCurrentPos = MarqueeStartPos;

            UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] Mouse STARTED (pressed) at: %s"),
                *MarqueeStartPos.ToString());
        }
        // Detect Completed event (button released)
        else if (Magnitude == 0.0f && bIsLeftClickHeld)
        {
            bIsLeftClickHeld = false;
            bLeftMousePressed = false;

            UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] Mouse COMPLETED (released). IsMarqueeActive=%d"),
                bIsMarqueeActive);

            if (bIsMarqueeActive)
            {
                // Finalize marquee selection
                FinalizeMarquee();
            }
            else
            {
                // Normal single-click selection
                FHitResult HitResult;
                if (GetHitResultUnderCursor(SelectionTraceChannel, false, HitResult))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Hit actor: %s at location %s"),
                        HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("None"),
                        *HitResult.Location.ToString());

                    // Request selection of the actor (server will notify client to update NPCBehaviorComponent)
                    ServerRequestSelect(HitResult.GetActor());
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] No hit result - deselecting"));

                    // No hit - deselect (server will notify client to update NPCBehaviorComponent)
                    ServerRequestDeselect();
                }
            }

            // Clear marquee state
            ClearMarquee();
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    else if (ActionName == TEXT("RightClick"))
    {
        // Right-click: Pass through to NPCBehaviorComponent for movement commands
        // The NPCBehaviorComponent will handle this if an NPC is selected
        return EPACS_InputHandleResult::NotHandled;
    }
    else if (ActionName == TEXT("Deselect"))
    {
        // Explicit deselection command (server will notify client to update NPCBehaviorComponent)
        ServerRequestDeselect();

        return EPACS_InputHandleResult::HandledConsume;
    }

    // PRIORITY 3: Legacy spawn placement mode for backward compatibility
    if (bIsPlacingSpawn)
    {
        // Left click to confirm placement
        if (ActionName == TEXT("LeftClick") || ActionName == TEXT("Select"))
        {
            const float Magnitude = Value.GetMagnitude();
            // Only on button release (Completed event)
            if (Magnitude == 0.0f)
            {
                HandleSpawnPlacementClick();
                return EPACS_InputHandleResult::HandledConsume;
            }
        }
        // Right click to cancel placement
        else if (ActionName == TEXT("RightClick") || ActionName == TEXT("Cancel"))
        {
            const float Magnitude = Value.GetMagnitude();
            // Only on button release
            if (Magnitude == 0.0f)
            {
                HandleSpawnPlacementCancel();
                return EPACS_InputHandleResult::HandledConsume;
            }
        }
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

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] ServerRequestSelect - Player: %s, Target: %s"),
        PS ? *PS->GetPlayerName() : TEXT("NULL"),
        *TargetActor->GetName());

    if (!PS)
    {
        UE_LOG(LogTemp, Error, TEXT("[SELECTION DEBUG] ServerRequestSelect failed - PlayerState null"));
        return;
    }

    // Check if target implements IPACS_Poolable (all NPCs do)
    if (!TargetActor->Implements<UPACS_Poolable>())
    {
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Target %s is not a poolable NPC - ignoring selection"),
            *TargetActor->GetName());
        return;
    }

    // Check selection state - try all NPC base classes
    bool bIsAlreadySelected = false;
    APlayerState* CurrentSelector = nullptr;

    if (APACS_NPC_Base* BaseNPC = Cast<APACS_NPC_Base>(TargetActor))
    {
        bIsAlreadySelected = BaseNPC->IsSelected();
        CurrentSelector = BaseNPC->GetCurrentSelector();
    }
    else if (APACS_NPC_Base_Char* CharNPC = Cast<APACS_NPC_Base_Char>(TargetActor))
    {
        bIsAlreadySelected = CharNPC->IsSelected();
        CurrentSelector = CharNPC->GetCurrentSelector();
    }
    else if (APACS_NPC_Base_Veh* VehNPC = Cast<APACS_NPC_Base_Veh>(TargetActor))
    {
        bIsAlreadySelected = VehNPC->IsSelected();
        CurrentSelector = VehNPC->GetCurrentSelector();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Could not cast target to any NPC base class"));
        return;
    }

    // Check if NPC is already selected by another player
    if (bIsAlreadySelected)
    {
        if (CurrentSelector && CurrentSelector != PS)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] NPC %s already selected by %s - cannot select"),
                *TargetActor->GetName(),
                *CurrentSelector->GetPlayerName());
            return; // Can't select - already taken by someone else
        }
    }

    // Clear previous selection if any
    if (AActor* PreviousActor = PS->GetSelectedActor())
    {
        if (PreviousActor != TargetActor) // Don't deselect if clicking same actor
        {
            if (APACS_NPC_Base* PreviousNPC = Cast<APACS_NPC_Base>(PreviousActor))
            {
                PreviousNPC->SetSelected(false, nullptr);
                UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Deselected previous NPC: %s"),
                    *PreviousActor->GetName());
            }
            else if (APACS_NPC_Base_Char* PreviousCharNPC = Cast<APACS_NPC_Base_Char>(PreviousActor))
            {
                PreviousCharNPC->SetSelected(false, nullptr);
            }
            else if (APACS_NPC_Base_Veh* PreviousVehNPC = Cast<APACS_NPC_Base_Veh>(PreviousActor))
            {
                PreviousVehNPC->SetSelected(false, nullptr);
            }
        }
    }

    // Select the new NPC - try all NPC base classes
    if (APACS_NPC_Base* BaseNPC = Cast<APACS_NPC_Base>(TargetActor))
    {
        BaseNPC->SetSelected(true, PS);
    }
    else if (APACS_NPC_Base_Char* CharNPC = Cast<APACS_NPC_Base_Char>(TargetActor))
    {
        CharNPC->SetSelected(true, PS);
    }
    else if (APACS_NPC_Base_Veh* VehNPC = Cast<APACS_NPC_Base_Veh>(TargetActor))
    {
        VehNPC->SetSelected(true, PS);
    }

    // For single selection, clear previous selections and set just this one
    PS->SetSelectedActor(TargetActor);

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] SUCCESS: %s selected NPC %s"),
        *PS->GetPlayerName(), *TargetActor->GetName());

    // Notify the owning client to update their NPCBehaviorComponent
    // In Server RPC, 'this' is the PlayerController that called the RPC
    ClientUpdateSelectedNPCs(PS->GetSelectedActors());
}

void APACS_PlayerController::ServerRequestSelectMultiple_Implementation(const TArray<AActor*>& TargetActors)
{
    // Server authority check
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("[SELECTION DEBUG] ServerRequestSelectMultiple failed - No authority"));
        return;
    }

    APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>();
    if (!PS)
    {
        UE_LOG(LogTemp, Error, TEXT("[SELECTION DEBUG] ServerRequestSelectMultiple failed - PlayerState null"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] ServerRequestSelectMultiple - Player: %s, Targets: %d"),
        PS ? *PS->GetPlayerName() : TEXT("NULL"),
        TargetActors.Num());

    // Clear previous selections that are NOT in the new selection list
    TArray<AActor*> PreviousActors = PS->GetSelectedActors();
    for (AActor* PreviousActor : PreviousActors)
    {
        if (!PreviousActor) continue;

        // Only clear if not in the new selection list
        if (!TargetActors.Contains(PreviousActor))
        {
            // Clear selection state on NPC being deselected
            if (APACS_NPC_Base* NPC = Cast<APACS_NPC_Base>(PreviousActor))
            {
                NPC->SetSelected(false, nullptr);
            }
            else if (APACS_NPC_Base_Char* CharNPC = Cast<APACS_NPC_Base_Char>(PreviousActor))
            {
                CharNPC->SetSelected(false, nullptr);
            }
            else if (APACS_NPC_Base_Veh* VehNPC = Cast<APACS_NPC_Base_Veh>(PreviousActor))
            {
                VehNPC->SetSelected(false, nullptr);
            }
        }
    }
    PS->ClearSelectedActors();

    // Now select all new actors that are available
    int32 SuccessCount = 0;
    for (AActor* TargetActor : TargetActors)
    {
        if (!TargetActor) continue;

        // Check if target implements IPACS_Poolable (all NPCs do)
        if (!TargetActor->Implements<UPACS_Poolable>())
        {
            UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Target %s is not a poolable NPC - skipping"),
                *TargetActor->GetName());
            continue;
        }

        // Check if NPC is already selected by another player
        APlayerState* CurrentSelector = nullptr;
        bool bIsAlreadySelected = false;

        if (APACS_NPC_Base* BaseNPC = Cast<APACS_NPC_Base>(TargetActor))
        {
            CurrentSelector = BaseNPC->GetCurrentSelector();
            bIsAlreadySelected = BaseNPC->IsSelected();
        }
        else if (APACS_NPC_Base_Char* CharNPC = Cast<APACS_NPC_Base_Char>(TargetActor))
        {
            CurrentSelector = CharNPC->GetCurrentSelector();
            bIsAlreadySelected = CharNPC->IsSelected();
        }
        else if (APACS_NPC_Base_Veh* VehNPC = Cast<APACS_NPC_Base_Veh>(TargetActor))
        {
            CurrentSelector = VehNPC->GetCurrentSelector();
            bIsAlreadySelected = VehNPC->IsSelected();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Could not cast %s to any NPC base class - skipping"),
                *TargetActor->GetName());
            continue;
        }

        // Skip if already selected by another player
        if (bIsAlreadySelected && CurrentSelector && CurrentSelector != PS)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] NPC %s already selected by %s - skipping"),
                *TargetActor->GetName(), *CurrentSelector->GetPlayerName());
            continue;
        }

        // Skip if already selected by us (no need to re-select)
        if (bIsAlreadySelected && CurrentSelector == PS)
        {
            PS->AddSelectedActor(TargetActor);  // Just add to our list
            SuccessCount++;
            UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] NPC %s already selected by us - keeping selected"),
                *TargetActor->GetName());
            continue;
        }

        // Select the NPC
        if (APACS_NPC_Base* BaseNPC = Cast<APACS_NPC_Base>(TargetActor))
        {
            BaseNPC->SetSelected(true, PS);
        }
        else if (APACS_NPC_Base_Char* CharNPC = Cast<APACS_NPC_Base_Char>(TargetActor))
        {
            CharNPC->SetSelected(true, PS);
        }
        else if (APACS_NPC_Base_Veh* VehNPC = Cast<APACS_NPC_Base_Veh>(TargetActor))
        {
            VehNPC->SetSelected(true, PS);
        }

        PS->AddSelectedActor(TargetActor);
        SuccessCount++;

        UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Selected NPC %s (%d/%d)"),
            *TargetActor->GetName(), SuccessCount, TargetActors.Num());
    }

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] SUCCESS: %s selected %d/%d NPCs"),
        *PS->GetPlayerName(), SuccessCount, TargetActors.Num());

    // Notify the owning client to update their NPCBehaviorComponent
    // In Server RPC, 'this' is the PlayerController that called the RPC
    ClientUpdateSelectedNPCs(PS->GetSelectedActors());
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

    // Clear NPC selection state for all selected actors
    TArray<AActor*> SelectedActors = PS->GetSelectedActors();
    int32 ClearedCount = 0;

    for (AActor* CurrentActor : SelectedActors)
    {
        if (!CurrentActor) continue;

        // Try all NPC base classes to clear selection
        if (APACS_NPC_Base* NPC = Cast<APACS_NPC_Base>(CurrentActor))
        {
            NPC->SetSelected(false, nullptr);
            ClearedCount++;
            UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Cleared selection state on NPC: %s"),
                *CurrentActor->GetName());
        }
        else if (APACS_NPC_Base_Char* CharNPC = Cast<APACS_NPC_Base_Char>(CurrentActor))
        {
            CharNPC->SetSelected(false, nullptr);
            ClearedCount++;
            UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Cleared selection state on Character NPC: %s"),
                *CurrentActor->GetName());
        }
        else if (APACS_NPC_Base_Veh* VehNPC = Cast<APACS_NPC_Base_Veh>(CurrentActor))
        {
            VehNPC->SetSelected(false, nullptr);
            ClearedCount++;
            UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Cleared selection state on Vehicle NPC: %s"),
                *CurrentActor->GetName());
        }
    }

    // Clear all selected actors in PlayerState
    PS->ClearSelectedActors();

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] SUCCESS: %s deselected %d NPCs"),
        *PS->GetPlayerName(), ClearedCount);

    // Notify the owning client to clear their NPCBehaviorComponent
    ClientUpdateSelectedNPCs(TArray<AActor*>());  // Empty array to clear
}

void APACS_PlayerController::ServerRequestMoveMultiple_Implementation(const TArray<AActor*>& NPCs, FVector_NetQuantize TargetLocation)
{
    // Server authority check
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("[MOVEMENT DEBUG] ServerRequestMoveMultiple failed - No authority"));
        return;
    }

    APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>();
    if (!PS)
    {
        UE_LOG(LogTemp, Error, TEXT("[MOVEMENT DEBUG] ServerRequestMoveMultiple failed - PlayerState null"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[MOVEMENT DEBUG] ServerRequestMoveMultiple - Player: %s, NPCs: %d, Target: %s"),
        *PS->GetPlayerName(), NPCs.Num(), *TargetLocation.ToString());

    // Move each NPC that this player owns
    int32 MovedCount = 0;
    for (AActor* NPC : NPCs)
    {
        if (!NPC) continue;

        // Verify this player owns the NPC
        bool bIsOwnedByPlayer = false;
        if (NPC->Implements<UPACS_SelectableCharacterInterface>())
        {
            IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(NPC);
            if (Selectable && Selectable->GetCurrentSelector() == PS)
            {
                bIsOwnedByPlayer = true;
            }
        }

        if (bIsOwnedByPlayer)
        {
            // Execute movement
            if (NPC->Implements<UPACS_SelectableCharacterInterface>())
            {
                IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(NPC);
                Selectable->MoveToLocation(TargetLocation);
                MovedCount++;

                UE_LOG(LogTemp, Log, TEXT("[MOVEMENT DEBUG] Moving NPC %s to %s"),
                    *NPC->GetName(), *TargetLocation.ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[MOVEMENT DEBUG] Player %s doesn't own NPC %s - skipping"),
                *PS->GetPlayerName(), *NPC->GetName());
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[MOVEMENT DEBUG] SUCCESS: Moved %d/%d NPCs to location"),
        MovedCount, NPCs.Num());
}

void APACS_PlayerController::ClientUpdateSelectedNPCs_Implementation(const TArray<AActor*>& SelectedNPCs)
{
    // Update NPCBehaviorComponent with the current selections
    if (NPCBehaviorComponent)
    {
        NPCBehaviorComponent->SetLocallySelectedNPCs(SelectedNPCs);

        UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Client updated NPCBehaviorComponent with %d selections"),
            SelectedNPCs.Num());
    }
}

void APACS_PlayerController::StartMarquee()
{
    bIsMarqueeActive = true;

    // Disable edge scrolling during marquee
    if (EdgeScrollComponent)
    {
        EdgeScrollComponent->SetEnabled(false);
        UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] Edge scrolling disabled"));
    }

    // Disable hover probe during marquee
    if (HoverProbe)
    {
        HoverProbe->SetComponentTickEnabled(false);
        UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] Hover probe disabled"));
    }

    // Disable rotation on AssessorPawn
    if (APACS_AssessorPawn* AssessorPawn = Cast<APACS_AssessorPawn>(GetPawn()))
    {
        // Note: AssessorPawn doesn't have SetRotationEnabled yet
        // This would need to be added to the pawn class
        UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] AssessorPawn rotation would be disabled here"));
    }

    // Start update timer
    if (MarqueeUpdateRate > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(MarqueeUpdateTimer, this,
            &APACS_PlayerController::UpdateMarquee, 1.0f / MarqueeUpdateRate, true);
        UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] Update timer started at %f Hz"), MarqueeUpdateRate);
    }

    UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] Marquee selection STARTED"));
}

void APACS_PlayerController::UpdateMarquee()
{
    if (!bIsMarqueeActive)
    {
        return;
    }

    QueryActorsInMarquee();
}

void APACS_PlayerController::FinalizeMarquee()
{
    if (!bIsMarqueeActive)
    {
        return;
    }

    APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>();
    if (!PS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] No PlayerState available"));
        return;
    }

    // Start with currently selected actors from NPCBehaviorComponent (client-side tracking)
    TArray<AActor*> ActorsToSelect;
    if (NPCBehaviorComponent)
    {
        ActorsToSelect = NPCBehaviorComponent->GetSelectedNPCs();
        UE_LOG(LogTemp, Warning, TEXT("[MARQUEE DEBUG] Starting with %d existing selections from NPCBehaviorComponent"),
            ActorsToSelect.Num());
    }
    int32 PreviouslySelectedCount = ActorsToSelect.Num();

    // Add newly marqueed actors that are available
    for (const TWeakObjectPtr<AActor>& ActorPtr : MarqueeHoveredActors)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            // Skip if already in our selection list
            if (ActorsToSelect.Contains(Actor))
            {
                continue;
            }

            // Verify still available before adding to selection list
            if (Actor->Implements<UPACS_SelectableCharacterInterface>())
            {
                IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(Actor);

                // Check if available (not selected) OR already selected by us
                APlayerState* CurrentSelector = Selectable ? Selectable->GetCurrentSelector() : nullptr;
                if (Selectable && (CurrentSelector == nullptr || CurrentSelector == PS))
                {
                    ActorsToSelect.Add(Actor);
                }
            }
        }
    }

    // Use the new batch selection RPC
    if (ActorsToSelect.Num() > 0)
    {
        ServerRequestSelectMultiple(ActorsToSelect);
        UE_LOG(LogTemp, Log, TEXT("[MARQUEE DEBUG] Marquee selection finalized: %d total actors (%d previous + %d new)"),
            ActorsToSelect.Num(), PreviouslySelectedCount, ActorsToSelect.Num() - PreviouslySelectedCount);
    }
    else
    {
        // If no valid actors, just deselect all
        ServerRequestDeselect();
        UE_LOG(LogTemp, Log, TEXT("[MARQUEE DEBUG] Marquee selection finalized: No valid actors to select"));
    }
}

void APACS_PlayerController::ClearMarquee()
{
    // Clear all hover states
    for (const TWeakObjectPtr<AActor>& ActorPtr : MarqueeHoveredActors)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            if (UPACS_SelectionPlaneComponent* SelectionComp = Actor->FindComponentByClass<UPACS_SelectionPlaneComponent>())
            {
                SelectionComp->SetHoverState(false);
            }
        }
    }

    // Clear state
    MarqueeHoveredActors.Empty();
    MarqueeStartPos = FVector2D::ZeroVector;
    MarqueeCurrentPos = FVector2D::ZeroVector;
    bIsMarqueeActive = false;
    bLeftMousePressed = false;

    // Stop update timer
    GetWorld()->GetTimerManager().ClearTimer(MarqueeUpdateTimer);

    // Re-enable systems
    if (EdgeScrollComponent)
    {
        EdgeScrollComponent->SetEnabled(true);
    }

    if (HoverProbe)
    {
        HoverProbe->SetComponentTickEnabled(true);
    }
}

void APACS_PlayerController::QueryActorsInMarquee()
{
    if (!bIsMarqueeActive)
    {
        return;
    }

    // Build screen rectangle
    const FVector2D MinPoint(
        FMath::Min(MarqueeStartPos.X, MarqueeCurrentPos.X),
        FMath::Min(MarqueeStartPos.Y, MarqueeCurrentPos.Y)
    );
    const FVector2D MaxPoint(
        FMath::Max(MarqueeStartPos.X, MarqueeCurrentPos.X),
        FMath::Max(MarqueeStartPos.Y, MarqueeCurrentPos.Y)
    );

    // Clear previous hover states
    for (const TWeakObjectPtr<AActor>& ActorPtr : MarqueeHoveredActors)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            if (UPACS_SelectionPlaneComponent* SelectionComp = Actor->FindComponentByClass<UPACS_SelectionPlaneComponent>())
            {
                SelectionComp->SetHoverState(false);
            }
        }
    }
    MarqueeHoveredActors.Empty();

    // Query all actors with selectable interface
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UPACS_SelectableCharacterInterface::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (!Actor || !Actor->Implements<UPACS_SelectableCharacterInterface>())
        {
            continue;
        }

        IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(Actor);
        if (!Selectable)
        {
            continue;
        }

        // Skip if already selected by someone
        if (Selectable->GetCurrentSelector() != nullptr)
        {
            continue;
        }

        // Check if selection plane state is Available (value 3)
        UPACS_SelectionPlaneComponent* SelectionComp = Actor->FindComponentByClass<UPACS_SelectionPlaneComponent>();
        if (!SelectionComp || SelectionComp->GetSelectionState() != 3) // 3 = Available
        {
            continue;
        }

        // Project actor location to screen
        FVector2D ScreenPos;
        if (UGameplayStatics::ProjectWorldToScreen(this, Actor->GetActorLocation(), ScreenPos))
        {
            // Check if within marquee bounds
            if (ScreenPos.X >= MinPoint.X && ScreenPos.X <= MaxPoint.X &&
                ScreenPos.Y >= MinPoint.Y && ScreenPos.Y <= MaxPoint.Y)
            {
                // Add to hovered list
                MarqueeHoveredActors.Add(Actor);

                // Apply hover visual
                SelectionComp->SetHoverState(true);
            }
        }
    }
}

// ========================================================================================
// NPC Spawn Placement Implementation
// ========================================================================================

void APACS_PlayerController::BeginSpawnPlacement(int32 ConfigIndex)
{
#if !UE_SERVER
    // Client-side only
    if (HasAuthority())
    {
        return;
    }

    // Validate config index
    UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>();
    if (!Orchestrator || !Orchestrator->GetSpawnConfig())
    {
        UE_LOG(LogTemp, Warning, TEXT("BeginSpawnPlacement: SpawnOrchestrator or config not ready"));
        return;
    }

    const TArray<FSpawnClassConfig>& Configs = Orchestrator->GetSpawnConfig()->GetSpawnConfigs();
    if (!Configs.IsValidIndex(ConfigIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("BeginSpawnPlacement: Invalid config index %d"), ConfigIndex);
        return;
    }

    // Check if this config should be visible in UI
    if (!Configs[ConfigIndex].bVisibleInUI)
    {
        UE_LOG(LogTemp, Warning, TEXT("BeginSpawnPlacement: Config %d is not visible in UI"), ConfigIndex);
        return;
    }

    // Set placement state
    ActiveSpawnConfigIndex = ConfigIndex;
    bIsPlacingSpawn = true;

    // Switch to UI input context (disables gameplay inputs)
    if (InputHandler)
    {
        InputHandler->SetBaseContext(EPACS_InputContextMode::UI);
    }

    UE_LOG(LogTemp, Log, TEXT("BeginSpawnPlacement: Started placement for config %d (%s)"),
        ConfigIndex, *Configs[ConfigIndex].DisplayName.ToString());
#endif
}

void APACS_PlayerController::CancelSpawnPlacement()
{
#if !UE_SERVER
    if (!bIsPlacingSpawn)
    {
        return;
    }

    // Clear placement state
    ActiveSpawnConfigIndex = -1;
    bIsPlacingSpawn = false;

    // Return to gameplay input context
    if (InputHandler)
    {
        InputHandler->SetBaseContext(EPACS_InputContextMode::Gameplay);
    }

    UE_LOG(LogTemp, Log, TEXT("CancelSpawnPlacement: Placement cancelled"));
#endif
}

TArray<int32> APACS_PlayerController::GetAvailableSpawnConfigs() const
{
    TArray<int32> AvailableIndices;

#if !UE_SERVER
    UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>();
    if (Orchestrator && Orchestrator->GetSpawnConfig())
    {
        const TArray<FSpawnClassConfig>& Configs = Orchestrator->GetSpawnConfig()->GetSpawnConfigs();
        for (int32 i = 0; i < Configs.Num(); ++i)
        {
            if (Configs[i].bVisibleInUI)
            {
                AvailableIndices.Add(i);
            }
        }
    }
#endif

    return AvailableIndices;
}

FText APACS_PlayerController::GetSpawnConfigDisplayName(int32 ConfigIndex) const
{
#if !UE_SERVER
    UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>();
    if (Orchestrator && Orchestrator->GetSpawnConfig())
    {
        const TArray<FSpawnClassConfig>& Configs = Orchestrator->GetSpawnConfig()->GetSpawnConfigs();
        if (Configs.IsValidIndex(ConfigIndex))
        {
            return Configs[ConfigIndex].DisplayName;
        }
    }
#endif

    return FText::GetEmpty();
}

UTexture2D* APACS_PlayerController::GetSpawnConfigIcon(int32 ConfigIndex) const
{
#if !UE_SERVER
    UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>();
    if (Orchestrator && Orchestrator->GetSpawnConfig())
    {
        const TArray<FSpawnClassConfig>& Configs = Orchestrator->GetSpawnConfig()->GetSpawnConfigs();
        if (Configs.IsValidIndex(ConfigIndex))
        {
            return Configs[ConfigIndex].ButtonIcon.LoadSynchronous();
        }
    }
#endif

    return nullptr;
}

void APACS_PlayerController::HandleSpawnPlacementClick()
{
#if !UE_SERVER
    if (!bIsPlacingSpawn || ActiveSpawnConfigIndex < 0)
    {
        return;
    }

    // Get spawn location from cursor
    FVector PlacementLocation;
    if (GetSpawnLocationFromCursor(PlacementLocation))
    {
        // Send spawn request to server
        ServerSpawnNPCAtLocation(ActiveSpawnConfigIndex, PlacementLocation);

        // End placement mode
        CancelSpawnPlacement();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("HandleSpawnPlacementClick: Failed to get valid spawn location"));
    }
#endif
}

void APACS_PlayerController::HandleSpawnPlacementCancel()
{
#if !UE_SERVER
    CancelSpawnPlacement();
#endif
}

bool APACS_PlayerController::GetSpawnLocationFromCursor(FVector& OutLocation) const
{
#if !UE_SERVER
    // Use Visibility channel for spawn placement - it hits landscapes, terrain, and world geometry
    // Don't use SelectionTraceChannel as it's configured only for selectable NPCs
    FHitResult HitResult;
    if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, HitResult))
    {
        // Check if we hit something
        if (HitResult.bBlockingHit)
        {
            // Log what we hit for debugging
            AActor* HitActor = HitResult.GetActor();
            UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Spawn trace hit %s at location %s"),
                HitActor ? *HitActor->GetName() : TEXT("Landscape/World"),
                *HitResult.Location.ToString());

            // Use the hit location regardless of what we hit (landscape, terrain, or actor)
            // The Visibility channel will hit anything visible, giving us a good spawn point
            OutLocation = HitResult.Location + FVector(0, 0, 10.0f);

            // If we hit an actor (like a building), use the impact point which is more accurate
            if (HitActor)
            {
                OutLocation = HitResult.ImpactPoint + FVector(0, 0, 10.0f);
                UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Hit actor %s, using impact point for spawn"),
                    *HitActor->GetName());
            }

            // Validate spawn location is reasonable
            if (OutLocation.Z < -10000.0f || OutLocation.Z > 100000.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController: Invalid spawn height: %f"), OutLocation.Z);
                return false;
            }

            UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Valid spawn location found: %s"), *OutLocation.ToString());
            return true;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController: No valid hit under cursor"));
#endif

    return false;
}

bool APACS_PlayerController::ServerSpawnNPCAtLocation_Validate(int32 ConfigIndex, FVector_NetQuantize Location)
{
    // Basic validation checks
    if (!HasAuthority())
    {
        return false;
    }

    // Validate spawn orchestrator is ready
    UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>();
    if (!Orchestrator || !Orchestrator->IsReady())
    {
        return false;
    }

    // Validate config index
    const TArray<FSpawnClassConfig>& Configs = Orchestrator->GetSpawnConfig()->GetSpawnConfigs();
    if (!Configs.IsValidIndex(ConfigIndex))
    {
        return false;
    }

    // Validate location is reasonable
    if (Location.Size() > 100000.0f || FMath::Abs(Location.Z) > 50000.0f)
    {
        return false;
    }

    // Check player spawn limit
    if (Configs[ConfigIndex].PlayerSpawnLimit > 0 && PlayerSpawnedCount >= Configs[ConfigIndex].PlayerSpawnLimit)
    {
        return false;
    }

    return true;
}

void APACS_PlayerController::ServerSpawnNPCAtLocation_Implementation(int32 ConfigIndex, FVector_NetQuantize Location)
{
    if (!HasAuthority())
    {
        return;
    }

    UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>();
    if (!Orchestrator || !Orchestrator->IsReady())
    {
        ClientSpawnFailed(ConfigIndex, (uint8)ESpawnFailureReason::SystemNotReady);
        return;
    }

    const TArray<FSpawnClassConfig>& Configs = Orchestrator->GetSpawnConfig()->GetSpawnConfigs();
    if (!Configs.IsValidIndex(ConfigIndex))
    {
        ClientSpawnFailed(ConfigIndex, (uint8)ESpawnFailureReason::InvalidLocation);
        return;
    }

    const FSpawnClassConfig& Config = Configs[ConfigIndex];

    // Check player spawn limit
    if (Config.PlayerSpawnLimit > 0 && PlayerSpawnedCount >= Config.PlayerSpawnLimit)
    {
        ClientSpawnFailed(ConfigIndex, (uint8)ESpawnFailureReason::PlayerLimitReached);
        return;
    }

    // Check memory budget
    UPACS_MemoryTracker* MemTracker = GetWorld()->GetSubsystem<UPACS_MemoryTracker>();
    if (MemTracker && !MemTracker->CanAllocateMemoryMB(1.0f)) // 1MB check
    {
        ClientSpawnFailed(ConfigIndex, (uint8)ESpawnFailureReason::GlobalLimitReached);
        return;
    }

    // Prepare spawn parameters
    FSpawnRequestParams Params;
    Params.Transform = FTransform(Location);
    Params.Owner = this;
    Params.Instigator = GetPawn();

    // Request actor from pool
    AActor* SpawnedActor = Orchestrator->AcquireActor(Config.SpawnTag, Params);

    if (SpawnedActor)
    {
        PlayerSpawnedCount++;
        ClientSpawnSucceeded(ConfigIndex);

        UE_LOG(LogTemp, Log, TEXT("ServerSpawnNPCAtLocation: Successfully spawned %s at %s for player %s"),
            *Config.DisplayName.ToString(),
            *Location.ToString(),
            *GetPlayerState<APlayerState>()->GetPlayerName());
    }
    else
    {
        ClientSpawnFailed(ConfigIndex, (uint8)ESpawnFailureReason::PoolExhausted);
        UE_LOG(LogTemp, Warning, TEXT("ServerSpawnNPCAtLocation: Failed to acquire actor from pool"));
    }
}

void APACS_PlayerController::ClientSpawnSucceeded_Implementation(int32 ConfigIndex)
{
#if !UE_SERVER
    UE_LOG(LogTemp, Log, TEXT("ClientSpawnSucceeded: Spawn successful for config %d"), ConfigIndex);

    // Could trigger UI feedback here
    // OnSpawnSucceededDelegate.Broadcast(ConfigIndex);
#endif
}

void APACS_PlayerController::ClientSpawnFailed_Implementation(int32 ConfigIndex, uint8 FailureReason)
{
#if !UE_SERVER
    ESpawnFailureReason Reason = (ESpawnFailureReason)FailureReason;

    FString ReasonString = TEXT("Unknown");
    switch (Reason)
    {
        case ESpawnFailureReason::PoolExhausted:
            ReasonString = TEXT("Pool exhausted");
            break;
        case ESpawnFailureReason::InvalidLocation:
            ReasonString = TEXT("Invalid location");
            break;
        case ESpawnFailureReason::PlayerLimitReached:
            ReasonString = TEXT("Player spawn limit reached");
            break;
        case ESpawnFailureReason::GlobalLimitReached:
            ReasonString = TEXT("Global spawn limit reached");
            break;
        case ESpawnFailureReason::NotAuthorized:
            ReasonString = TEXT("Not authorized");
            break;
        case ESpawnFailureReason::SystemNotReady:
            ReasonString = TEXT("System not ready");
            break;
    }

    UE_LOG(LogTemp, Warning, TEXT("ClientSpawnFailed: Spawn failed for config %d - %s"),
        ConfigIndex, *ReasonString);

    // Could trigger UI feedback here
    // OnSpawnFailedDelegate.Broadcast(ConfigIndex, Reason);
#endif
}

// ========================================
// New GameplayTag-based Spawn System
// ========================================

void APACS_PlayerController::EnterSpawnPlacementMode(FGameplayTag SpawnTag)
{
    // Client-only operation
    if (!IsLocalController())
    {
        return;
    }

    // Validate spawn tag
    if (!SpawnTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController: Invalid spawn tag provided to EnterSpawnPlacementMode"));
        return;
    }

    // Store the spawn tag
    PendingSpawnTag = SpawnTag;
    bSpawnPlacementMode = true;

    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Entering spawn placement mode for tag: %s"),
        *SpawnTag.ToString());

    // Switch input context to UI mode
    if (InputHandler)
    {
        InputHandler->SetBaseContext(EPACS_InputContextMode::UI);
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Switched to UI input context"));
    }

    // Clear any active selection (both locally and on server)
    if (NPCBehaviorComponent)
    {
        NPCBehaviorComponent->ClearLocalSelection();
    }

    // Also request server to deselect any selected NPCs
    ServerRequestDeselect();

    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Cleared all selections for spawn placement mode"));
}

void APACS_PlayerController::ExitSpawnPlacementMode()
{
    // Client-only operation
    if (!IsLocalController())
    {
        return;
    }

    // Clear placement state
    PendingSpawnTag = FGameplayTag();
    bSpawnPlacementMode = false;

    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Exiting spawn placement mode"));

    // Return to gameplay input context
    if (InputHandler)
    {
        InputHandler->SetBaseContext(EPACS_InputContextMode::Gameplay);
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Switched back to Gameplay input context"));
    }

    // Re-enable all spawn buttons via the spawn UI widget
    // Find the SpawnListWidget and enable all its buttons
    if (SpawnUIWidget && SpawnUIWidget->WidgetTree)
    {
        // Find the spawn list widget
        TArray<UWidget*> AllWidgets;
        SpawnUIWidget->WidgetTree->GetAllWidgets(AllWidgets);

        for (UWidget* Widget : AllWidgets)
        {
            // Look for the SpawnListWidget which contains the buttons
            if (UPACS_SpawnListWidget* SpawnList = Cast<UPACS_SpawnListWidget>(Widget))
            {
                // Enable all buttons in the spawn list
                SpawnList->EnableAllButtons();
                UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Re-enabled all spawn buttons"));
                break;
            }
        }
    }
}

void APACS_PlayerController::HandlePlaceNPCAction(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::HandlePlaceNPCAction - Called, bSpawnPlacementMode=%s, PendingSpawnTag=%s"),
        bSpawnPlacementMode ? TEXT("true") : TEXT("false"),
        PendingSpawnTag.IsValid() ? *PendingSpawnTag.ToString() : TEXT("Invalid"));

    // Only handle if in placement mode
    if (!bSpawnPlacementMode || !PendingSpawnTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController::HandlePlaceNPCAction - Not in placement mode or invalid tag"));
        return;
    }

    // Get spawn location from cursor
    FVector NPCSpawnLocation;
    if (!GetSpawnLocationFromCursor(NPCSpawnLocation))
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController::HandlePlaceNPCAction - Failed to get valid spawn location"));
        ClientNotifySpawnResult(false, TEXT("Invalid spawn location - click on ground"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::HandlePlaceNPCAction - Sending spawn request for %s at %s"),
        *PendingSpawnTag.ToString(), *NPCSpawnLocation.ToString());

    // Send spawn request to server
    ServerRequestSpawnNPC(PendingSpawnTag, NPCSpawnLocation);

    // Exit placement mode
    ExitSpawnPlacementMode();
}

void APACS_PlayerController::HandleCancelPlacementAction(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::HandleCancelPlacementAction - Called, bSpawnPlacementMode=%s"),
        bSpawnPlacementMode ? TEXT("true") : TEXT("false"));

    // Simply exit placement mode
    if (bSpawnPlacementMode)
    {
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::HandleCancelPlacementAction - Cancelling spawn placement"));
        ExitSpawnPlacementMode();
    }
}

void APACS_PlayerController::ServerRequestSpawnNPC_Implementation(FGameplayTag SpawnTag, FVector_NetQuantize Location)
{
    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::ServerRequestSpawnNPC - Received request for tag %s at location %s"),
        SpawnTag.IsValid() ? *SpawnTag.ToString() : TEXT("Invalid"),
        *Location.ToString());

    // CRITICAL: Server authority check
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_PlayerController::ServerRequestSpawnNPC - Called without authority"));
        return;
    }

    // Validate spawn tag
    if (!SpawnTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController::ServerRequestSpawnNPC - Invalid spawn tag"));
        ClientNotifySpawnResult(false, TEXT("Invalid spawn configuration"));
        return;
    }

    // Get spawn orchestrator
    UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>();
    if (!Orchestrator || !Orchestrator->IsReady())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_PlayerController: SpawnOrchestrator not ready"));
        ClientNotifySpawnResult(false, TEXT("Spawn system not ready"));
        return;
    }

    // Get spawn config
    UPACS_SpawnConfig* SpawnConfig = Orchestrator->GetSpawnConfig();
    if (!SpawnConfig)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_PlayerController: No spawn config available"));
        ClientNotifySpawnResult(false, TEXT("Spawn configuration not loaded"));
        return;
    }

    // Find config for this tag
    FSpawnClassConfig Config;
    if (!SpawnConfig->GetConfigForTag(SpawnTag, Config))
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController: No config found for spawn tag: %s"),
            *SpawnTag.ToString());
        ClientNotifySpawnResult(false, TEXT("Unknown spawn type"));
        return;
    }

    // Check player spawn limit
    if (Config.PlayerSpawnLimit > 0 && PlayerSpawnedCount >= Config.PlayerSpawnLimit)
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController: Player spawn limit reached (%d/%d)"),
            PlayerSpawnedCount, Config.PlayerSpawnLimit);
        ClientNotifySpawnResult(false, FString::Printf(TEXT("Spawn limit reached (%d/%d)"),
            PlayerSpawnedCount, Config.PlayerSpawnLimit));
        return;
    }

    // Set up spawn parameters
    FSpawnRequestParams Params;
    Params.Transform = FTransform(Location);
    Params.Owner = this;
    Params.Instigator = GetPawn();

    // Use the pooling system to spawn
    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::ServerRequestSpawnNPC - Acquiring actor from pool for tag %s"),
        *SpawnTag.ToString());

    AActor* SpawnedNPC = Orchestrator->AcquireActor(SpawnTag, Params);

    if (SpawnedNPC)
    {
        PlayerSpawnedCount++;

        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::ServerRequestSpawnNPC - SUCCESS! Spawned %s at %s (Player spawn count: %d)"),
            *SpawnedNPC->GetName(), *Location.ToString(), PlayerSpawnedCount);

        ClientNotifySpawnResult(true, FString::Printf(TEXT("NPC spawned successfully (%s)"),
            *Config.DisplayName.ToString()));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController::ServerRequestSpawnNPC - FAILED! Pool exhausted for tag: %s"),
            *SpawnTag.ToString());
        ClientNotifySpawnResult(false, TEXT("Pool exhausted - cannot spawn more NPCs"));
    }
}

void APACS_PlayerController::ClientNotifySpawnResult_Implementation(bool bSuccess, const FString& Message)
{
    // This is where you could trigger UI notifications
    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Spawn result - Success: %s, Message: %s"),
        bSuccess ? TEXT("true") : TEXT("false"), *Message);

    // Could broadcast to UI widgets here
    // For example: OnSpawnResultDelegate.Broadcast(bSuccess, Message);
}

// ========================================
// Spawn UI Management
// ========================================

void APACS_PlayerController::CreateSpawnUI()
{
    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::CreateSpawnUI - Starting UI creation process"));

    // Only create UI on local clients, never on dedicated server
    if (!IsLocalController())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController::CreateSpawnUI - Not local controller, skipping"));
        return;
    }

    if (IsRunningDedicatedServer())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController::CreateSpawnUI - Running on dedicated server, skipping"));
        return;
    }

    // Don't create if already exists
    if (SpawnUIWidget)
    {
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::CreateSpawnUI - SpawnUIWidget already exists"));
        return;
    }

    // Check if widget class is set
    if (!SpawnUIWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_PlayerController::CreateSpawnUI - SpawnUIWidgetClass not set! Set it in BP_PlayerController Class Defaults to WBP_MainOverlay."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::CreateSpawnUI - Widget class is set: %s"),
        *SpawnUIWidgetClass->GetName());

    // Check if VR player - don't show UI for HMD users
    if (APACS_PlayerState* PS = GetPlayerState<APACS_PlayerState>())
    {
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::CreateSpawnUI - HMD State: %s"),
            *UEnum::GetValueAsString(PS->HMDState));

        if (PS->HMDState == EHMDState::HasHMD)
        {
            UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::CreateSpawnUI - Skipping UI for VR player"));
            return;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_PlayerController::CreateSpawnUI - PlayerState not available yet"));
    }

    // Note: SpawnOrchestrator is server-only, so clients won't have it
    // The spawn buttons will get their data from the replicated SpawnConfig on the server
    // when the user clicks them. We don't need to check orchestrator readiness here.
    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::CreateSpawnUI - Proceeding to create widget (client-side UI)"))

    // Create the widget
    UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::CreateSpawnUI - Creating widget from class: %s"),
        *SpawnUIWidgetClass->GetName());

    SpawnUIWidget = CreateWidget<UUserWidget>(this, SpawnUIWidgetClass);
    if (SpawnUIWidget)
    {
        SpawnUIWidget->AddToViewport();
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController::CreateSpawnUI - SUCCESS! Spawn UI widget created and added to viewport"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PACS_PlayerController: Failed to create spawn UI widget from class: %s"),
            *SpawnUIWidgetClass->GetName());
    }
}

void APACS_PlayerController::DestroySpawnUI()
{
    if (SpawnUIWidget)
    {
        SpawnUIWidget->RemoveFromParent();
        SpawnUIWidget = nullptr;
        UE_LOG(LogTemp, Log, TEXT("PACS_PlayerController: Spawn UI widget destroyed"));
    }
}