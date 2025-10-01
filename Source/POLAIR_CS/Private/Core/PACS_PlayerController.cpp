#include "Core/PACS_PlayerController.h"
#include "Core/PACS_PlayerState.h"
#include "Core/PACSGameMode.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Actors/Pawn/PACS_CandidateHelicopterCharacter.h"
#include "Actors/NPC/PACS_NPC_Base.h"
#include "Actors/NPC/PACS_NPC_Base_Char.h"
#include "Actors/NPC/PACS_NPC_Base_Veh.h"
#include "Interfaces/PACS_Poolable.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Subsystems/PACSLaunchArgSubsystem.h"
#include "EngineUtils.h"
#include "Components/DecalComponent.h"
#include "Components/PACS_NPCBehaviorComponent.h"

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

        // Perform line trace to get the actor under cursor
        FHitResult HitResult;
        if (GetHitResultUnderCursor(SelectionTraceChannel, false, HitResult))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Hit actor: %s at location %s"),
                HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("None"),
                *HitResult.Location.ToString());

            // Request selection of the actor
            ServerRequestSelect(HitResult.GetActor());

            // Update local selection tracking in NPCBehaviorComponent
            if (NPCBehaviorComponent && HitResult.GetActor())
            {
                if (HitResult.GetActor()->Implements<UPACS_SelectableCharacterInterface>())
                {
                    NPCBehaviorComponent->SetLocallySelectedNPC(HitResult.GetActor());
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] No hit result - deselecting"));

            // No hit - deselect
            ServerRequestDeselect();

            // Clear local selection
            if (NPCBehaviorComponent)
            {
                NPCBehaviorComponent->ClearLocalSelection();
            }
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
        // Explicit deselection command
        ServerRequestDeselect();

        // Clear local selection
        if (NPCBehaviorComponent)
        {
            NPCBehaviorComponent->ClearLocalSelection();
        }

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

    PS->SetSelectedActor(TargetActor);

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] SUCCESS: %s selected NPC %s"),
        *PS->GetPlayerName(), *TargetActor->GetName());
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

    // Clear NPC selection state before clearing PlayerState
    if (AActor* CurrentActor = PS->GetSelectedActor())
    {
        // Try all NPC base classes to clear selection
        if (APACS_NPC_Base* NPC = Cast<APACS_NPC_Base>(CurrentActor))
        {
            NPC->SetSelected(false, nullptr);
            UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Cleared selection state on NPC: %s"),
                *CurrentActor->GetName());
        }
        else if (APACS_NPC_Base_Char* CharNPC = Cast<APACS_NPC_Base_Char>(CurrentActor))
        {
            CharNPC->SetSelected(false, nullptr);
            UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Cleared selection state on Character NPC: %s"),
                *CurrentActor->GetName());
        }
        else if (APACS_NPC_Base_Veh* VehNPC = Cast<APACS_NPC_Base_Veh>(CurrentActor))
        {
            VehNPC->SetSelected(false, nullptr);
            UE_LOG(LogTemp, Log, TEXT("[SELECTION DEBUG] Cleared selection state on Vehicle NPC: %s"),
                *CurrentActor->GetName());
        }
    }

    // Clear selected actor in PlayerState
    PS->SetSelectedActor(nullptr);

    UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] SUCCESS: %s deselected all"),
        *PS->GetPlayerName());
}