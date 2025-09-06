# Phase 1.1 - Production Input System Implementation Instructions

## CRITICAL IMPLEMENTATION REQUIREMENTS

**⚠️ MANDATORY: These instructions MUST be followed explicitly. No scope creep, no additional features, no deviations unless absolutely necessary for compilation.**

**⚠️ NAMING CONVENTION: ALL classes MUST use 'PACS_' prefix (PACS_PlayerController, PACS_InputHandlerComponent, etc.)**

**⚠️ UE VERSION: Target Unreal Engine 5.5 exactly**

---

## Overview

Implement a production-ready input routing system with proper Enhanced Input context management. The system provides stable priority-based input routing with granular overlay control.

## Required Files to Create

### 1. Core Header Files
- `PACS_InputHandlerComponent.h`
- `PACS_InputMappingConfig.h` 
- `PACS_PlayerController.h`
- `PACS_InputTypes.h`

### 2. Implementation Files  
- `PACS_InputHandlerComponent.cpp`
- `PACS_InputMappingConfig.cpp`
- `PACS_PlayerController.cpp`

### 3. Required Module Dependencies
Ensure these modules are included in your `Build.cs` file:
```cpp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject", 
    "Engine",
    "InputCore",
    "EnhancedInput",
    "UMG",
    "Slate",
    "SlateCore"
});
```

---

## Implementation Details

### File 1: PACS_InputTypes.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "PACS_InputTypes.generated.h"

// Logging category
DECLARE_LOG_CATEGORY_EXTERN(LogPACSInput, Log, All);

// Production logging macros
#define PACS_INPUT_ERROR(Format, ...) UE_LOG(LogPACSInput, Error, TEXT(Format), ##__VA_ARGS__)
#define PACS_INPUT_WARNING(Format, ...) UE_LOG(LogPACSInput, Warning, TEXT(Format), ##__VA_ARGS__)
#define PACS_INPUT_LOG(Format, ...) UE_LOG(LogPACSInput, Log, TEXT(Format), ##__VA_ARGS__)
#define PACS_INPUT_VERBOSE(Format, ...) UE_LOG(LogPACSInput, VeryVerbose, TEXT(Format), ##__VA_ARGS__)

// Safety constants
namespace PACS_InputLimits
{
    static constexpr int32 MaxOverlayContexts = 10;
    static constexpr int32 MaxReceivers = 100;
    static constexpr int32 MaxActionsPerConfig = 100;
    static constexpr int32 InvalidReceiverCleanupThreshold = 10;
}

namespace PACS_InputPriority
{
    static constexpr int32 Critical     = 10000;
    static constexpr int32 UI          = 1000;
    static constexpr int32 Menu        = 800;
    static constexpr int32 Modal       = 600;
    static constexpr int32 Gameplay    = 400;
    static constexpr int32 Background  = 200;
}

UENUM()
enum class EPACS_InputHandleResult : uint8
{
    NotHandled,
    HandledPassThrough,
    HandledConsume
};

UENUM()
enum class EPACS_InputContextMode : uint8
{
    Gameplay,
    Menu,
    UI
};

UENUM()
enum class EPACS_OverlayType : uint8
{
    None,
    Blocking,
    NonBlocking,
    Partial
};

UINTERFACE(MinimalAPI)
class UPACS_InputReceiver : public UInterface
{
    GENERATED_BODY()
};

class IPACS_InputReceiver
{
    GENERATED_BODY()
public:
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) = 0;
    virtual int32 GetInputPriority() const { return PACS_InputPriority::Gameplay; }
};

USTRUCT()
struct FPACS_InputReceiverEntry
{
    GENERATED_BODY()

    UPROPERTY() 
    TWeakObjectPtr<UObject> ReceiverObject;
    
    UPROPERTY() 
    int32 Priority = 0;
    
    UPROPERTY() 
    uint32 RegistrationOrder = 0;

    IPACS_InputReceiver* GetInterface() const
    {
        if (UObject* Obj = ReceiverObject.Get())
        {
            return Cast<IPACS_InputReceiver>(Obj);
        }
        return nullptr;
    }

    bool IsValid() const 
    { 
        return ReceiverObject.IsValid() && GetInterface() != nullptr; 
    }

    bool operator<(const FPACS_InputReceiverEntry& Other) const
    {
        if (Priority != Other.Priority) return Priority > Other.Priority;
        return RegistrationOrder < Other.RegistrationOrder;
    }
};

USTRUCT()
struct FPACS_OverlayEntry
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UInputMappingContext> Context;
    
    UPROPERTY()
    EPACS_OverlayType Type = EPACS_OverlayType::None;
    
    UPROPERTY()
    int32 Priority = 0;
};
```

### File 2: PACS_InputMappingConfig.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PACS_InputTypes.h"
#include "PACS_InputMappingConfig.generated.h"

USTRUCT(BlueprintType)
struct FPACS_InputActionMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", 
        meta=(DisplayName="Input Action"))
    TObjectPtr<UInputAction> InputAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input",
        meta=(DisplayName="Action Name"))
    FName ActionIdentifier;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindStarted = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindTriggered = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindCompleted = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindOngoing = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindCanceled = false;

    FPACS_InputActionMapping()
        : ActionIdentifier(NAME_None)
    {}
};

UCLASS(BlueprintType, Category="Input")
class UPACS_InputMappingConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action Mapping", 
        meta=(TitleProperty="ActionIdentifier"))
    TArray<FPACS_InputActionMapping> ActionMappings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contexts", 
        meta=(DisplayName="Gameplay Context"))
    TObjectPtr<UInputMappingContext> GameplayContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contexts",
        meta=(DisplayName="Menu Context"))
    TObjectPtr<UInputMappingContext> MenuContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contexts",
        meta=(DisplayName="UI Context"))
    TObjectPtr<UInputMappingContext> UIContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI Protection",
        meta=(DisplayName="Actions Blocked by UI"))
    TArray<FName> UIBlockedActions = { 
        TEXT("Move"),
        TEXT("Look"),
        TEXT("Jump"),
        TEXT("Fire"),
        TEXT("Interact")
    };

    bool IsValid() const
    {
        return GameplayContext != nullptr && 
               MenuContext != nullptr && 
               ActionMappings.Num() > 0 &&
               ActionMappings.Num() <= PACS_InputLimits::MaxActionsPerConfig;
    }

    FName GetActionIdentifier(const UInputAction* InputAction) const;
};
```

### File 3: PACS_InputHandlerComponent.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnhancedInputSubsystems.h"
#include "PACS_InputTypes.h"
#include "PACS_InputMappingConfig.h"
#include "PACS_InputHandlerComponent.generated.h"

class FInputActionInstance;

UCLASS(NotBlueprintable, ClassGroup=(Input), meta=(BlueprintSpawnableComponent=false))
class UPACS_InputHandlerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPACS_InputHandlerComponent();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
    TObjectPtr<UPACS_InputMappingConfig> InputConfig;

    bool IsHealthy() const { return bIsInitialized && InputConfig && InputConfig->IsValid(); }

    // --- Public API ---
    
    UFUNCTION(BlueprintCallable, Category="Input")
    void RegisterReceiver(UObject* Receiver, int32 Priority);

    UFUNCTION(BlueprintCallable, Category="Input")
    void UnregisterReceiver(UObject* Receiver);

    UFUNCTION(BlueprintCallable, Category="Input")
    void SetBaseContext(EPACS_InputContextMode ContextMode);

    UFUNCTION(BlueprintCallable, Category="Input")
    void ToggleMenuContext();

    UFUNCTION(BlueprintCallable, Category="Input")
    void PushOverlay(UInputMappingContext* Context, EPACS_OverlayType Type = EPACS_OverlayType::Blocking, 
        int32 Priority = PACS_InputPriority::UI);

    UFUNCTION(BlueprintCallable, Category="Input")
    void PopOverlay();

    UFUNCTION(BlueprintCallable, Category="Input")
    void PopAllOverlays();

    UFUNCTION(BlueprintPure, Category="Input")
    bool HasBlockingOverlay() const;

    UFUNCTION(BlueprintPure, Category="Input")
    int32 GetOverlayCount() const { return OverlayStack.Num(); }

    void HandleAction(const FInputActionInstance& Instance);

    void OnSubsystemAvailable();
    void OnSubsystemUnavailable();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if !UE_SERVER
    bool bIsInitialized = false;
    bool bActionMapBuilt = false;
    bool bSubsystemValid = false;

    UPROPERTY() 
    TArray<FPACS_InputReceiverEntry> Receivers;
    
    UPROPERTY() 
    TMap<TObjectPtr<UInputAction>, FName> ActionToNameMap;
    
    uint32 RegistrationCounter = 0;
    int32 InvalidReceiverCount = 0;

    UPROPERTY() 
    EPACS_InputContextMode CurrentBaseMode = EPACS_InputContextMode::Gameplay;
    
    UPROPERTY() 
    TArray<FPACS_OverlayEntry> OverlayStack;

    UPROPERTY()
    TSet<TObjectPtr<UInputMappingContext>> ManagedContexts;

    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> CachedSubsystem;

    void Initialize();
    void Shutdown();
    void BuildActionNameMap();
    void EnsureActionMapBuilt();
    
    EPACS_InputHandleResult RouteActionInternal(FName ActionName, const FInputActionValue& Value);
    void CleanInvalidReceivers();
    void SortReceivers();
    
    void UpdateManagedContexts();
    void RemoveAllManagedContexts();
    
    UInputMappingContext* GetBaseContext(EPACS_InputContextMode Mode) const;
    int32 GetBaseContextPriority(EPACS_InputContextMode Mode) const;
    
    UEnhancedInputLocalPlayerSubsystem* GetValidSubsystem();
    bool ValidateConfig() const;
    
    FORCEINLINE bool EnsureGameThread() const
    {
        if (!IsInGameThread())
        {
            PACS_INPUT_ERROR("Input handler called from non-game thread!");
            return false;
        }
        return true;
    }
#endif // !UE_SERVER
};
```

### File 4: PACS_PlayerController.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PACS_InputHandlerComponent.h"
#include "PACS_PlayerController.generated.h"

UCLASS()
class APACS_PlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APACS_PlayerController();

protected:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input", 
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_InputHandlerComponent> InputHandler;

    void BindInputActions();
    void ValidateInputSystem();
};
```

### File 5: PACS_InputMappingConfig.cpp

```cpp
#include "PACS_InputMappingConfig.h"

FName UPACS_InputMappingConfig::GetActionIdentifier(const UInputAction* InputAction) const
{
    for (const FPACS_InputActionMapping& Mapping : ActionMappings)
    {
        if (Mapping.InputAction == InputAction)
        {
            return Mapping.ActionIdentifier;
        }
    }
    return NAME_None;
}
```

### File 6: PACS_InputHandlerComponent.cpp

**⚠️ CRITICAL: This file contains the complete implementation. Follow exactly as written.**

```cpp
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
```

### File 7: PACS_PlayerController.cpp

```cpp
#include "PACS_PlayerController.h"

#if !UE_SERVER
#include "EnhancedInputComponent.h"
#include "PACS_InputTypes.h"
#endif

APACS_PlayerController::APACS_PlayerController()
{
    InputHandler = CreateDefaultSubobject<UPACS_InputHandlerComponent>(TEXT("InputHandler"));
}

void APACS_PlayerController::BeginPlay()
{
    Super::BeginPlay();
    ValidateInputSystem();
}

void APACS_PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
#if !UE_SERVER
    BindInputActions();
#endif
}

void APACS_PlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
#if !UE_SERVER
    if (InputHandler)
    {
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
    if (!InputHandler || !InputHandler->InputConfig)
    {
        UE_LOG(LogPACSInput, Warning, 
            TEXT("Cannot bind input actions - invalid configuration"));
        return;
    }

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC)
    {
        UE_LOG(LogPACSInput, Error, TEXT("Enhanced Input Component not found!"));
        return;
    }

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
            EIC->BindAction(Mapping.InputAction, ETriggerEvent::Started, 
                InputHandler, &UPACS_InputHandlerComponent::HandleAction);
        }
        
        if (Mapping.bBindTriggered)
        {
            EIC->BindAction(Mapping.InputAction, ETriggerEvent::Triggered, 
                InputHandler, &UPACS_InputHandlerComponent::HandleAction);
        }
        
        if (Mapping.bBindCompleted)
        {
            EIC->BindAction(Mapping.InputAction, ETriggerEvent::Completed, 
                InputHandler, &UPACS_InputHandlerComponent::HandleAction);
        }
        
        if (Mapping.bBindOngoing)
        {
            EIC->BindAction(Mapping.InputAction, ETriggerEvent::Ongoing, 
                InputHandler, &UPACS_InputHandlerComponent::HandleAction);
        }
        
        if (Mapping.bBindCanceled)
        {
            EIC->BindAction(Mapping.InputAction, ETriggerEvent::Canceled, 
                InputHandler, &UPACS_InputHandlerComponent::HandleAction);
        }
    }
    
    UE_LOG(LogPACSInput, Log, TEXT("Bound %d input actions (permanent bindings)"), 
        InputHandler->InputConfig->ActionMappings.Num());
#endif
}
```

---

## TESTING REQUIREMENTS

After implementation, create these test assets:

1. **Primary Data Asset**: `DA_PACSInputConfig`
2. **Input Actions**: `IA_MenuToggle`, `IA_Move`, `IA_Look`, `IA_Fire`  
3. **Input Mapping Contexts**: `IMC_Gameplay`, `IMC_Menu`, `IMC_UI`

## COMPILATION CHECKLIST

- [ ] All files use PACS_ prefix consistently
- [ ] All #if !UE_SERVER guards in place  
- [ ] Enhanced Input module dependency added
- [ ] Logging category defined once in .cpp file
- [ ] No additional features beyond specification
- [ ] Compiles without errors in UE 5.5

## POST-IMPLEMENTATION VERIFICATION

1. Create Blueprint based on APACS_PlayerController
2. Assign InputConfig data asset
3. Test context switching works
4. Verify input routing functions
5. Confirm Enhanced Input integration

**⚠️ FINAL REMINDER: Follow these instructions exactly. No scope creep. No additional features. Implementation only.**