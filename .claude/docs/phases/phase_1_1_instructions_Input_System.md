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

# PACS Input System – Automation & Functional Tests (UE 5.5)

**Audience:** Claude Code (to generate tests) + Pauly (to run them)  
**Goal:** Add compile‑ready **Automation Specs** and a **Functional Test** to verify the PACS Input System design using Unreal Engine **5.5 Source Build**.  
**Scope:** Tests build for Editor/Development; **not compiled in Shipping**.

---

## 0) Preconditions & Assumptions

- Your game module is called `<YourGame>` (replace with your real module name everywhere below).  
- You have these classes (or equivalents) already present:
  - `APACS_PlayerController` (owns `UPACS_InputHandlerComponent`)
  - `UPACS_InputHandlerComponent` with public API:
    - `bool IsHealthy() const;`
    - `void SetBaseContext(EPACS_InputContextMode Mode);`
    - `void ToggleMenuContext();`
    - `int32 GetOverlayCount() const;`
    - `bool HasBlockingOverlay() const;`
    - `void PushOverlay(class UInputMappingContext* IMC, EPACS_OverlayType Type, int32 Priority);`
    - `void PopOverlay();`
    - `void RegisterReceiver(UObject* Receiver, int32 Priority);`
    - `void UnregisterReceiver(UObject* Receiver);`
    - `void HandleAction(const struct FInputActionInstance& Instance);`
    - `UPROPERTY(EditAnywhere) UPACS_InputMappingConfig* InputConfig;`
  - `UPACS_InputMappingConfig` with:
    - `TArray<FPACS_InputActionMapping> ActionMappings;`
    - `UInputMappingContext* GameplayContext;`
    - `UInputMappingContext* MenuContext;`
    - `UInputMappingContext* UIContext;`
    - `bool IsValid() const;`
    - `FName GetActionIdentifier(const class UInputAction* Action) const;`
  - Types:
    - `EPACS_InputContextMode { Gameplay, Menu }`
    - `EPACS_OverlayType { NonBlocking, Blocking }`
    - `namespace PACS_InputPriority { constexpr int32 Critical=1000; constexpr int32 UI=400; constexpr int32 Gameplay=200; }`
    - `struct FPACS_InputReceiverEntry { int32 Priority; int32 RegistrationOrder; bool operator<(const FPACS_InputReceiverEntry& RHS) const; }`
    - `struct FPACS_InputActionMapping { class UInputAction* InputAction; FName ActionIdentifier; }`
    - `namespace PACS_InputLimits { constexpr int32 MaxActionsPerConfig = 256; }`
    - Interface `IPACS_InputReceiver` with:
      - `EPACS_InputHandleResult HandleInputAction(FName ActionName, const struct FInputActionValue& Value);`
      - `int32 GetInputPriority() const;`
    - `enum class EPACS_InputHandleResult { NotHandled, HandledPassThrough, HandledConsume };`
- Enhanced Input is in use. Editor has **Functional Testing** plugin enabled (built‑in).

> If any names differ, adjust the code below accordingly.


---

## 1) Build.cs – add test deps for non‑Shipping

**File:** `Source/<YourGame>/<YourGame>.Build.cs`

```csharp
// Add in your constructor after PublicDependencyModuleNames declarations
if (Target.Configuration != UnrealTargetConfiguration.Shipping)
{
    PublicDependencyModuleNames.AddRange(new string[]
    {
        "AutomationController",
        "FunctionalTesting",
        "EnhancedInput"
    });
}
```

> These modules are editor‑safe and excluded in Shipping.


---

## 2) Directory layout for tests

Create these folders if they don’t exist:

```
Source/<YourGame>/Private/Tests/
Source/<YourGame>/Public/Tests/   // optional; we keep tests private in this guide
```


---

## 3) Automation Specs (no world needed)

**Purpose:** Validate config semantics, identifier lookup, and receiver ordering (priority desc, FIFO within equal priority).

**File:** `Source/<YourGame>/Private/Tests/PACS_Input_AutomationSpecs.cpp`

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "PACS_InputMappingConfig.h"
#include "PACS_InputTypes.h" // EPACS_* types, FPACS_InputReceiverEntry, PACS_InputLimits, etc.

// ------- Spec 1: Config validity & identifier lookup -------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_InputConfigValiditySpec,
    "PACS.Input.InputConfig.ValidityAndLookup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_InputConfigValiditySpec::RunTest(const FString& Parameters)
{
    UPACS_InputMappingConfig* Config = NewObject<UPACS_InputMappingConfig>(GetTransientPackage());
    TestNotNull(TEXT("Config allocated"), Config);

    UInputMappingContext* IMC_Gameplay = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_Menu     = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_UI       = NewObject<UInputMappingContext>(GetTransientPackage());
    TestNotNull(TEXT("Gameplay IMC"), IMC_Gameplay);
    TestNotNull(TEXT("Menu IMC"), IMC_Menu);
    TestNotNull(TEXT("UI IMC"), IMC_UI);

    Config->GameplayContext = IMC_Gameplay;
    Config->MenuContext     = IMC_Menu;
    Config->UIContext       = IMC_UI;

    UInputAction* IA_Move = NewObject<UInputAction>(GetTransientPackage());
    IA_Move->Rename(TEXT("IA_Move"));
    FPACS_InputActionMapping MapMove;
    MapMove.InputAction      = IA_Move;
    MapMove.ActionIdentifier = TEXT("Move");

    UInputAction* IA_Fire = NewObject<UInputAction>(GetTransientPackage());
    IA_Fire->Rename(TEXT("IA_Fire"));
    FPACS_InputActionMapping MapFire;
    MapFire.InputAction      = IA_Fire;
    MapFire.ActionIdentifier = TEXT("Fire");

    Config->ActionMappings = { MapMove, MapFire };

    TestTrue(TEXT("Config reports valid"), Config->IsValid());
    TestEqual(TEXT("Lookup Move by action ptr"), Config->GetActionIdentifier(IA_Move), FName(TEXT("Move")));
    TestEqual(TEXT("Lookup Fire by action ptr"), Config->GetActionIdentifier(IA_Fire), FName(TEXT("Fire")));

    // Guardrail: exceed mapping cap
    TArray<FPACS_InputActionMapping> Big;
    Big.SetNum(PACS_InputLimits::MaxActionsPerConfig + 1);
    Config->ActionMappings = Big;
    TestFalse(TEXT("Too many mappings -> invalid"), Config->IsValid());

    return true;
}

// ------- Spec 2: Receiver ordering (priority desc, FIFO for equals) -------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_ReceiverOrderingSpec,
    "PACS.Input.Receivers.Ordering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_ReceiverOrderingSpec::RunTest(const FString& Parameters)
{
    FPACS_InputReceiverEntry A; A.Priority = 400;  A.RegistrationOrder = 1;
    FPACS_InputReceiverEntry B; B.Priority = 1000; B.RegistrationOrder = 2;
    FPACS_InputReceiverEntry C; C.Priority = 400;  C.RegistrationOrder = 3;

    TArray<FPACS_InputReceiverEntry> Arr = { A, B, C };
    Arr.Sort();

    TestTrue(TEXT("First is highest priority (B)"), Arr[0].Priority == 1000);
    TestTrue(TEXT("Second is A (FIFO within 400)"), Arr[1].Priority == 400 && Arr[1].RegistrationOrder == 1);
    TestTrue(TEXT("Third is C (FIFO within 400)"),  Arr[2].Priority == 400 && Arr[2].RegistrationOrder == 3);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```


---

## 4) Functional Test (runtime in PIE)

**Purpose:** Exercise `UPACS_InputHandlerComponent` end‑to‑end in PIE: init health, base context switch, overlay push/pop, and receiver routing/consumption.

### 4.1 Test receiver (implements your interface)

**File:** `Source/<YourGame>/Private/Tests/PACS_TestReceiver.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PACS_InputTypes.h"
#include "PACS_TestReceiver.generated.h"

UCLASS()
class UPACS_TestReceiver : public UObject, public IPACS_InputReceiver
{
    GENERATED_BODY()

public:
    UPROPERTY() EPACS_InputHandleResult Response = EPACS_InputHandleResult::NotHandled;
    UPROPERTY() int32 PriorityOverride = PACS_InputPriority::Gameplay;

    UPROPERTY() FName  LastAction = NAME_None;
    UPROPERTY() struct FInputActionValue LastValue;

    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override
    {
        LastAction = ActionName;
        LastValue  = Value;
        return Response;
    }
    virtual int32 GetInputPriority() const override { return PriorityOverride; }
};
```

> Keep in **Private/Tests**; we’re not exposing this to production code.


### 4.2 Functional test actor

**File:** `Source/<YourGame>/Private/Tests/PACS_Input_FunctionalTest.cpp`

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "FunctionalTest.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"

#include "PACS_TestReceiver.h"
#include "PACS_InputHandlerComponent.h"
#include "PACS_InputMappingConfig.h"
#include "PACS_PlayerController.h"

#include "PACS_Input_FunctionalTest.generated.h"

UCLASS()
class APACS_Input_FunctionalTest : public AFunctionalTest
{
    GENERATED_BODY()

public:
    APACS_Input_FunctionalTest()
    {
        Author = TEXT("PACS");
        Description = TEXT("Verifies InputHandler init, base context switching, overlay semantics, and receiver routing.");
        bIsEnabled = true;
    }

    UPROPERTY() UPACS_InputMappingConfig* Config = nullptr;
    UPROPERTY() UInputMappingContext* IMC_Gameplay = nullptr;
    UPROPERTY() UInputMappingContext* IMC_Menu = nullptr;
    UPROPERTY() UInputMappingContext* IMC_UI = nullptr;
    UPROPERTY() UInputAction* IA_Test = nullptr;

    virtual void StartTest() override
    {
        APACS_PlayerController* PC = GetWorld() ? Cast<APACS_PlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
        if (!PC) { FailTest(TEXT("APACS_PlayerController not present. Set your GameMode to use it.")); return; }

        UPACS_InputHandlerComponent* Handler = PC->FindComponentByClass<UPACS_InputHandlerComponent>();
        if (!Handler) { FailTest(TEXT("UPACS_InputHandlerComponent missing from PlayerController.")); return; }

        Config = NewObject<UPACS_InputMappingConfig>(this);
        IMC_Gameplay = NewObject<UInputMappingContext>(this);
        IMC_Menu     = NewObject<UInputMappingContext>(this);
        IMC_UI       = NewObject<UInputMappingContext>(this);
        IA_Test      = NewObject<UInputAction>(this); IA_Test->Rename(TEXT("IA_Test"));

        FPACS_InputActionMapping M;
        M.InputAction      = IA_Test;
        M.ActionIdentifier = TEXT("TestAction");
        Config->ActionMappings = { M };
        Config->GameplayContext = IMC_Gameplay;
        Config->MenuContext     = IMC_Menu;
        Config->UIContext       = IMC_UI;

        Handler->InputConfig = Config;

        if (!Handler->IsHealthy())
        {
            FailTest(TEXT("Handler IsHealthy() == false. Ensure Enhanced Input + contexts available."));
            return;
        }

        // Base context toggle
        Handler->SetBaseContext(EPACS_InputContextMode::Gameplay);
        Handler->ToggleMenuContext();  // -> Menu
        Handler->ToggleMenuContext();  // -> Gameplay

        // Overlay push/pop & blocking
        const int32 PrevCount = Handler->GetOverlayCount();
        Handler->PushOverlay(NewObject<UInputMappingContext>(this), EPACS_OverlayType::Blocking, PACS_InputPriority::UI);
        if (!Handler->HasBlockingOverlay()) { FailTest(TEXT("HasBlockingOverlay() should be true after push.")); return; }
        Handler->PopOverlay();
        if (Handler->GetOverlayCount() != PrevCount) { FailTest(TEXT("Overlay count mismatch after pop.")); return; }

        // Receivers
        UPACS_TestReceiver* RConsume = NewObject<UPACS_TestReceiver>(this);
        RConsume->Response = EPACS_InputHandleResult::HandledConsume;
        RConsume->PriorityOverride = PACS_InputPriority::Critical;

        UPACS_TestReceiver* RPass = NewObject<UPACS_TestReceiver>(this);
        RPass->Response = EPACS_InputHandleResult::HandledPassThrough;

        Handler->RegisterReceiver(RPass,    RPass->GetInputPriority());
        Handler->RegisterReceiver(RConsume, RConsume->GetInputPriority());

        FInputActionInstance Instance(IA_Test);
        Instance.Value = FInputActionValue(1.0f);
        Handler->HandleAction(Instance);

        if (RConsume->LastAction != TEXT("TestAction"))
        {
            FailTest(TEXT("Top-priority receiver did not receive/consume TestAction."));
            return;
        }

        Handler->UnregisterReceiver(RConsume);
        Handler->UnregisterReceiver(RPass);

        FinishTest(EFunctionalTestResult::Succeeded, TEXT("PACS Input functional test passed."));
    }
};

#endif // WITH_DEV_AUTOMATION_TESTS
```

**Map Setup:** Place one `APACS_Input_FunctionalTest` actor in a small test map. Set your GameMode to use `APACS_PlayerController`. No content assets required (tests use transient assets).


---

## 5) How to run the tests

### 5.1 Automation Specs (no map required)
- Editor: **Window → Developer Tools → Session Frontend → Automation**  
- Filter: `PACS.Input`  
- Run:
  - `PACS.Input.InputConfig.ValidityAndLookup`
  - `PACS.Input.Receivers.Ordering`

### 5.2 Functional Test (needs map with actor)
- Open the map containing `APACS_Input_FunctionalTest`.
- Editor: **Window → Functional Testing** → select the actor → **Run**.  
- Or run from **Session Frontend → Automation → Functional Tests**.

### 5.3 CLI (optional, headless)
From an x64 Native Tools Developer Command Prompt or PowerShell in your repo root:

```powershell
# Replace <Proj> and <Map> with your names; build Editor if needed
# Run automation in editor (use -ExecCmds=Automation RunTests to target individual tests)
Engine\Binaries\Win64\UnrealEditor-Cmd.exe <Proj>.uproject -run=Automation -unattended -nop4 -TestExit="Automation Test Queue Empty"
```

Examples:
```powershell
# Run all PACS.Input specs
Engine\Binaries\Win64\UnrealEditor-Cmd.exe <Proj>.uproject -ExecCmds="Automation RunTests PACS.Input" -unattended -nop4 -TestExit="Automation Test Queue Empty"

# Run functional tests only
Engine\Binaries\Win64\UnrealEditor-Cmd.exe <Proj>.uproject -ExecCmds="Automation RunTests Functional" -unattended -nop4 -TestExit="Automation Test Queue Empty"
```

> To export JSON results, add: `-ReportOutputPath="<absolute_path>" -ReportOutputFormat=Json`


---

## 6) Completion Definition (for Claude Code)

Claude must:
1. Update `<YourGame>.Build.cs` with test dependencies (non‑Shipping only).  
2. Create files with exact paths:
   - `Source/<YourGame>/Private/Tests/PACS_Input_AutomationSpecs.cpp`
   - `Source/<YourGame>/Private/Tests/PACS_TestReceiver.h`
   - `Source/<YourGame>/Private/Tests/PACS_Input_FunctionalTest.cpp`
3. Ensure includes/types resolve (adjust includes for your actual header locations).  
4. Compile Editor target; tests must appear in **Session Frontend → Automation**.  
5. Provide a small test map containing one `APACS_Input_FunctionalTest` actor and GameMode using `APACS_PlayerController`.  
6. Verify all tests **pass** in Editor and via CLI.  
7. Commit changes with message: `test(pacs): add automation specs and functional test for input system`.

**Acceptance Criteria:**
- Both automation specs succeed locally.  
- Functional test succeeds in PIE and via Session Frontend.  
- No Shipping build impact; Editor/Development only.  
- JSON test report can be generated via CLI.


---

## 7) Troubleshooting

- **Tests not visible** → Ensure `WITH_DEV_AUTOMATION_TESTS` is true for your configuration and plugin *Functional Testing* is enabled.  
- **IsHealthy false** → Confirm Enhanced Input subsystem active, local PC present, and contexts created.  
- **Linker errors** → Move headers that define `IPACS_InputReceiver`/enums into a module that tests can include; avoid circular includes.  
- **No `APACS_PlayerController` in map** → Set GameMode override in the test map’s World Settings.

---

## 8) Notes on Style & Safety

- Tests live under `Private/Tests` to keep scope tight.  
- Use **transient assets** to avoid content dependencies.  
- Priority ordering spec is a canary for regression in `operator<` comparisons.

---

## 9) Next steps (optional)

- Add a **non‑blocking overlay** case to ensure routing passes to gameplay receiver when UI overlay is non‑blocking.  
- Add a **stress test** that registers/unregisters 1000 receivers and confirms stable ordering and no crashes.

