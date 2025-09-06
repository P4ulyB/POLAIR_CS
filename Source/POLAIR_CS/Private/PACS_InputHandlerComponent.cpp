#include "PACS_InputHandlerComponent.h"

#if !UE_SERVER
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#endif

DEFINE_LOG_CATEGORY(LogPACSInput);

UPACS_InputHandlerComponent::UPACS_InputHandlerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bAutoActivate = true;
}

void UPACS_InputHandlerComponent::BeginPlay()
{
    Super::BeginPlay();
#if !UE_SERVER
    Initialize();
#endif
}

void UPACS_InputHandlerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_SERVER
    Shutdown();
#endif
    Super::EndPlay(EndPlayReason);
}

#if !UE_SERVER

void UPACS_InputHandlerComponent::Initialize()
{
    const APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !PC->IsLocalController())
    {
        PACS_INPUT_WARNING("InputHandler: Not attached to local PlayerController");
        return;
    }

    if (!ValidateConfig())
    {
        PACS_INPUT_ERROR("InputHandler: Invalid configuration!");
        return;
    }

    if (const ULocalPlayer* LP = PC->GetLocalPlayer())
    {
        CachedSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
        bSubsystemValid = CachedSubsystem.IsValid();
        
        if (!bSubsystemValid)
        {
            PACS_INPUT_ERROR("Enhanced Input Subsystem not available!");
            return;
        }
    }

    BuildActionNameMap();
    
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetValidSubsystem())
    {
        if (PC->GetPawn() && PC->GetPawn()->InputComponent)
        {
            Subsystem->ClearAllMappings();
        }
    }
    
    SetBaseContext(EPACS_InputContextMode::Gameplay);
    bIsInitialized = true;
    
    PACS_INPUT_LOG("InputHandler initialized successfully");
}

void UPACS_InputHandlerComponent::Shutdown()
{
    if (!bIsInitialized) return;

    RemoveAllManagedContexts();

    Receivers.Empty();
    OverlayStack.Empty();
    ActionToNameMap.Empty();
    ManagedContexts.Empty();
    CachedSubsystem.Reset();
    
    bIsInitialized = false;
    bSubsystemValid = false;

    PACS_INPUT_LOG("InputHandler shutdown complete");
}

bool UPACS_InputHandlerComponent::ValidateConfig() const
{
    if (!InputConfig)
    {
        PACS_INPUT_ERROR("InputConfig is null!");
        return false;
    }

    if (!InputConfig->IsValid())
    {
        PACS_INPUT_ERROR("InputConfig validation failed!");
        return false;
    }

    return true;
}

void UPACS_InputHandlerComponent::BuildActionNameMap()
{
    if (!InputConfig) return;
    
    ActionToNameMap.Reset();
    ActionToNameMap.Reserve(InputConfig->ActionMappings.Num());

    for (const FPACS_InputActionMapping& Mapping : InputConfig->ActionMappings)
    {
        if (!Mapping.InputAction)
        {
            PACS_INPUT_WARNING("Null InputAction in mapping for '%s'", 
                *Mapping.ActionIdentifier.ToString());
            continue;
        }

        if (ActionToNameMap.Contains(Mapping.InputAction))
        {
            PACS_INPUT_WARNING("Duplicate InputAction mapping: %s", 
                *Mapping.InputAction->GetName());
            continue;
        }

        ActionToNameMap.Add(Mapping.InputAction, Mapping.ActionIdentifier);
        PACS_INPUT_VERBOSE("Mapped action '%s' -> '%s'", 
            *Mapping.InputAction->GetName(), 
            *Mapping.ActionIdentifier.ToString());
    }

    bActionMapBuilt = true;
}

void UPACS_InputHandlerComponent::EnsureActionMapBuilt()
{
    if (!bActionMapBuilt && InputConfig)
    {
        PACS_INPUT_VERBOSE("Lazy-building action map");
        BuildActionNameMap();
    }
}

UEnhancedInputLocalPlayerSubsystem* UPACS_InputHandlerComponent::GetValidSubsystem()
{
    if (bSubsystemValid && CachedSubsystem.IsValid())
    {
        return CachedSubsystem.Get();
    }

    const APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC) return nullptr;
    
    const ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP) return nullptr;
    
    UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (Subsystem)
    {
        CachedSubsystem = Subsystem;
        bSubsystemValid = true;
        OnSubsystemAvailable();
    }
    else
    {
        bSubsystemValid = false;
    }
    
    return Subsystem;
}

void UPACS_InputHandlerComponent::OnSubsystemAvailable()
{
    PACS_INPUT_LOG("Enhanced Input Subsystem became available");
    UpdateManagedContexts();
}

void UPACS_InputHandlerComponent::OnSubsystemUnavailable()
{
    PACS_INPUT_WARNING("Enhanced Input Subsystem became unavailable");
    bSubsystemValid = false;
}

void UPACS_InputHandlerComponent::RegisterReceiver(UObject* Receiver, int32 Priority)
{
    if (!EnsureGameThread()) return;
    
    if (!Receiver)
    {
        PACS_INPUT_WARNING("RegisterReceiver: Null receiver");
        return;
    }

    if (!Receiver->GetClass()->ImplementsInterface(UPACS_InputReceiver::StaticClass()))
    {
        PACS_INPUT_WARNING("RegisterReceiver: %s doesn't implement IPACS_InputReceiver", 
            *Receiver->GetName());
        return;
    }

    if (Receivers.Num() >= PACS_InputLimits::MaxReceivers)
    {
        PACS_INPUT_ERROR("Max receivers (%d) exceeded! Rejecting %s", 
            PACS_InputLimits::MaxReceivers, *Receiver->GetName());
        return;
    }

    for (const FPACS_InputReceiverEntry& Entry : Receivers)
    {
        if (Entry.ReceiverObject.Get() == Receiver)
        {
            PACS_INPUT_WARNING("Receiver %s already registered", *Receiver->GetName());
            return;
        }
    }

    FPACS_InputReceiverEntry Entry;
    Entry.ReceiverObject = Receiver;
    Entry.Priority = Priority;
    Entry.RegistrationOrder = ++RegistrationCounter;
    
    Receivers.Add(Entry);
    SortReceivers();

    PACS_INPUT_VERBOSE("Registered receiver %s (Priority: %d)", 
        *Receiver->GetName(), Priority);
}

void UPACS_InputHandlerComponent::UnregisterReceiver(UObject* Receiver)
{
    if (!EnsureGameThread()) return;
    
    const int32 RemovedCount = Receivers.RemoveAll([&](const FPACS_InputReceiverEntry& Entry)
    {
        return Entry.ReceiverObject.Get() == Receiver;
    });

    if (RemovedCount > 0)
    {
        PACS_INPUT_VERBOSE("Unregistered receiver %s", 
            Receiver ? *Receiver->GetName() : TEXT("NULL"));
    }
}

void UPACS_InputHandlerComponent::HandleAction(const FInputActionInstance& Instance)
{
    if (!EnsureGameThread()) return;
    if (!bIsInitialized) return;

    EnsureActionMapBuilt();

    const UInputAction* Action = Instance.GetSourceAction();
    if (!Action) return;

    const FName* ActionNamePtr = ActionToNameMap.Find(Action);
    if (!ActionNamePtr)
    {
        PACS_INPUT_VERBOSE("Unmapped action: %s", *Action->GetName());
        return;
    }

    RouteActionInternal(*ActionNamePtr, Instance.GetValue());
}

EPACS_InputHandleResult UPACS_InputHandlerComponent::RouteActionInternal(
    FName ActionName, 
    const FInputActionValue& Value)
{
    if (HasBlockingOverlay() && InputConfig->UIBlockedActions.Contains(ActionName))
    {
        PACS_INPUT_VERBOSE("Action '%s' blocked by overlay", *ActionName.ToString());
        return EPACS_InputHandleResult::HandledConsume;
    }

    for (int32 i = 0; i < Receivers.Num(); ++i)
    {
        FPACS_InputReceiverEntry& Entry = Receivers[i];
        
        if (!Entry.IsValid())
        {
            InvalidReceiverCount++;
            continue;
        }

        IPACS_InputReceiver* Receiver = Entry.GetInterface();
        const EPACS_InputHandleResult Result = Receiver->HandleInputAction(ActionName, Value);
        
        if (Result == EPACS_InputHandleResult::HandledConsume)
        {
            if (InvalidReceiverCount > PACS_InputLimits::InvalidReceiverCleanupThreshold)
            {
                CleanInvalidReceivers();
            }
            return Result;
        }
    }

    if (InvalidReceiverCount > PACS_InputLimits::InvalidReceiverCleanupThreshold)
    {
        CleanInvalidReceivers();
    }

    return EPACS_InputHandleResult::NotHandled;
}

void UPACS_InputHandlerComponent::CleanInvalidReceivers()
{
    const int32 OldCount = Receivers.Num();
    Receivers.RemoveAll([](const FPACS_InputReceiverEntry& Entry) 
    { 
        return !Entry.IsValid(); 
    });
    
    const int32 RemovedCount = OldCount - Receivers.Num();
    if (RemovedCount > 0)
    {
        PACS_INPUT_LOG("Cleaned %d invalid receivers", RemovedCount);
    }
    
    InvalidReceiverCount = 0;
}

void UPACS_InputHandlerComponent::SortReceivers()
{
    Receivers.Sort();
}

void UPACS_InputHandlerComponent::SetBaseContext(EPACS_InputContextMode ContextMode)
{
    if (!EnsureGameThread()) return;
    if (!bIsInitialized) return;

    CurrentBaseMode = ContextMode;
    UpdateManagedContexts();
    
    PACS_INPUT_LOG("Set base context to %s", 
        ContextMode == EPACS_InputContextMode::Gameplay ? TEXT("Gameplay") :
        ContextMode == EPACS_InputContextMode::Menu ? TEXT("Menu") : TEXT("UI"));
}

void UPACS_InputHandlerComponent::ToggleMenuContext()
{
    const EPACS_InputContextMode NewMode = 
        (CurrentBaseMode == EPACS_InputContextMode::Menu) ? 
        EPACS_InputContextMode::Gameplay : 
        EPACS_InputContextMode::Menu;
    
    SetBaseContext(NewMode);
}

void UPACS_InputHandlerComponent::PushOverlay(UInputMappingContext* Context, 
    EPACS_OverlayType Type, int32 Priority)
{
    if (!EnsureGameThread()) return;
    if (!bIsInitialized || !Context) return;

    if (OverlayStack.Num() >= PACS_InputLimits::MaxOverlayContexts)
    {
        PACS_INPUT_ERROR("Max overlay contexts (%d) exceeded!", 
            PACS_InputLimits::MaxOverlayContexts);
        return;
    }

    for (const FPACS_OverlayEntry& Entry : OverlayStack)
    {
        if (Entry.Context == Context)
        {
            PACS_INPUT_WARNING("Context %s already in overlay stack", *Context->GetName());
            return;
        }
    }

    FPACS_OverlayEntry NewEntry;
    NewEntry.Context = Context;
    NewEntry.Type = Type;
    NewEntry.Priority = Priority;
    
    OverlayStack.Add(NewEntry);
    UpdateManagedContexts();
    
    PACS_INPUT_LOG("Pushed %s overlay: %s (Stack depth: %d)", 
        Type == EPACS_OverlayType::Blocking ? TEXT("blocking") : TEXT("non-blocking"),
        *Context->GetName(), OverlayStack.Num());
}

void UPACS_InputHandlerComponent::PopOverlay()
{
    if (!EnsureGameThread()) return;
    if (!bIsInitialized || OverlayStack.Num() == 0) return;

    FPACS_OverlayEntry Popped = OverlayStack.Pop();
    UpdateManagedContexts();
    
    PACS_INPUT_LOG("Popped overlay: %s (Stack depth: %d)", 
        Popped.Context ? *Popped.Context->GetName() : TEXT("NULL"), 
        OverlayStack.Num());
}

void UPACS_InputHandlerComponent::PopAllOverlays()
{
    if (!EnsureGameThread()) return;
    if (!bIsInitialized) return;

    const int32 Count = OverlayStack.Num();
    OverlayStack.Empty();
    UpdateManagedContexts();
    
    PACS_INPUT_LOG("Cleared %d overlays", Count);
}

bool UPACS_InputHandlerComponent::HasBlockingOverlay() const
{
    for (const FPACS_OverlayEntry& Entry : OverlayStack)
    {
        if (Entry.Type == EPACS_OverlayType::Blocking || 
            Entry.Type == EPACS_OverlayType::Partial)
        {
            return true;
        }
    }
    return false;
}

void UPACS_InputHandlerComponent::UpdateManagedContexts()
{
    UEnhancedInputLocalPlayerSubsystem* Subsystem = GetValidSubsystem();
    if (!Subsystem)
    {
        PACS_INPUT_WARNING("Cannot update contexts - subsystem unavailable");
        return;
    }

    RemoveAllManagedContexts();
    ManagedContexts.Empty();

    if (UInputMappingContext* BaseContext = GetBaseContext(CurrentBaseMode))
    {
        const int32 Priority = GetBaseContextPriority(CurrentBaseMode);
        Subsystem->AddMappingContext(BaseContext, Priority);
        ManagedContexts.Add(BaseContext);
    }

    for (const FPACS_OverlayEntry& Entry : OverlayStack)
    {
        if (Entry.Context)
        {
            Subsystem->AddMappingContext(Entry.Context, Entry.Priority);
            ManagedContexts.Add(Entry.Context);
        }
    }
    
    PACS_INPUT_VERBOSE("Updated managed contexts (Count: %d)", ManagedContexts.Num());
}

void UPACS_InputHandlerComponent::RemoveAllManagedContexts()
{
    UEnhancedInputLocalPlayerSubsystem* Subsystem = GetValidSubsystem();
    if (!Subsystem) return;

    for (TObjectPtr<UInputMappingContext> Context : ManagedContexts)
    {
        if (Context)
        {
            Subsystem->RemoveMappingContext(Context);
        }
    }
}

UInputMappingContext* UPACS_InputHandlerComponent::GetBaseContext(EPACS_InputContextMode Mode) const
{
    if (!InputConfig) return nullptr;

    switch (Mode)
    {
        case EPACS_InputContextMode::Gameplay:
            return InputConfig->GameplayContext;
        case EPACS_InputContextMode::Menu:
            return InputConfig->MenuContext;
        case EPACS_InputContextMode::UI:
            return InputConfig->UIContext;
        default:
            return nullptr;
    }
}

int32 UPACS_InputHandlerComponent::GetBaseContextPriority(EPACS_InputContextMode Mode) const
{
    switch (Mode)
    {
        case EPACS_InputContextMode::Gameplay:
            return PACS_InputPriority::Gameplay;
        case EPACS_InputContextMode::Menu:
            return PACS_InputPriority::Menu;
        case EPACS_InputContextMode::UI:
            return PACS_InputPriority::UI;
        default:
            return PACS_InputPriority::Background;
    }
}

#endif // !UE_SERVER