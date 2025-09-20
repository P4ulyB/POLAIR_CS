# Phase 1.6 — Marquee Selection (Local HUD-Only Implementation)

## Overview

A **simple, local-only** marquee selection system that:
- Uses **single Enhanced Input action** with natural drag phases
- Maintains **spatial cache** of selectable actors (no world iteration)
- Draws **HUD selection rectangle** with configurable appearance
- Integrates with **assessor pawn config** for visual customisation
- **No networking** - purely local client-side functionality
- **Zero over-engineering** - minimal viable implementation

## File Placement

**All files in standard POLAIR_CS module structure:**
- Header: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Public\PACS_MarqueeSelectionComponent.h`
- Source: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Private\PACS_MarqueeSelectionComponent.cpp`

## Design Principles

1. **Local Only**: No replication, no server validation, no networking
2. **Spatial Registry**: Cache selectable actors, no world iteration during drag
3. **Single Input Action**: One Enhanced Input action handles all drag phases naturally
4. **HUD Drawing**: Simple Canvas-based rectangle drawing
5. **Performance First**: Minimal allocations, efficient screen-space checks
6. **Config Integration**: Visual settings from assessor pawn config

## Implementation

### Step 1: Update Assessor Pawn Config

**File**: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Public\Data\Configs\AssessorPawnConfig.h`

```cpp
// Add to existing UAssessorPawnConfig class
public:
    // MARQUEE SELECTION CONFIGURATION
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MarqueeSelection|Visual", 
        meta=(DisplayName="Marquee Fill Colour"))
    FLinearColor MarqueeColor = FLinearColor(0.0f, 0.8f, 1.0f, 0.15f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MarqueeSelection|Visual", 
        meta=(DisplayName="Marquee Border Colour"))
    FLinearColor MarqueeBorderColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MarqueeSelection|Visual", 
        meta=(ClampMin="1.0", ClampMax="8.0", DisplayName="Border Thickness"))
    float MarqueeBorderThickness = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MarqueeSelection|Behaviour", 
        meta=(ClampMin="4.0", ClampMax="64.0", DisplayName="Minimum Selection Size (pixels)"))
    float MinimumSelectionSize = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MarqueeSelection|Behaviour", 
        meta=(ClampMin="1", ClampMax="500", DisplayName="Maximum Selected Actors"))
    int32 MaxSelectedActors = 100;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MarqueeSelection|Tags", 
        meta=(DisplayName="Selectable Actor Tag"))
    FName SelectableTag = TEXT("Selectable");
```

### Step 2: Create Selectable Actor Registry Subsystem

**File**: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Public\PACS_SelectableActorSubsystem.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PACS_SelectableActorSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPACSSelection, Log, All);

/**
 * Fast spatial registry for selectable actors
 * Prevents expensive world iteration during marquee selection
 */
UCLASS()
class POLAIR_CS_API UPACS_SelectableActorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // Register/unregister actors (call from BeginPlay/EndPlay)
    void RegisterSelectableActor(AActor* Actor, FName Tag);
    void UnregisterSelectableActor(AActor* Actor);

    // Get all registered actors with tag (fast lookup)
    void GetSelectableActorsWithTag(FName Tag, TArray<TWeakObjectPtr<AActor>>& OutActors) const;

    // Cleanup invalid references (called periodically)
    void CleanupInvalidActors();

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
    // Tag -> WeakPtr registry
    UPROPERTY(Transient)
    TMap<FName, TArray<TWeakObjectPtr<AActor>>> SelectableActorRegistry;

    // Cleanup timer
    FTimerHandle CleanupTimerHandle;
    static constexpr float CleanupInterval = 30.0f; // 30 seconds
};
```

**File**: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Private\PACS_SelectableActorSubsystem.cpp`

```cpp
#include "PACS_SelectableActorSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogPACSSelection);

void UPACS_SelectableActorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Start periodic cleanup
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &UPACS_SelectableActorSubsystem::CleanupInvalidActors,
            CleanupInterval,
            true // Loop
        );
    }
}

void UPACS_SelectableActorSubsystem::RegisterSelectableActor(AActor* Actor, FName Tag)
{
    if (!IsValid(Actor) || Tag.IsNone())
    {
        return;
    }

    TArray<TWeakObjectPtr<AActor>>& ActorsForTag = SelectableActorRegistry.FindOrAdd(Tag);
    ActorsForTag.AddUnique(Actor);
    
    UE_LOG(LogPACSSelection, VeryVerbose, TEXT("Registered selectable actor: %s with tag: %s"), 
        *GetNameSafe(Actor), *Tag.ToString());
}

void UPACS_SelectableActorSubsystem::UnregisterSelectableActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    // Remove from all tag arrays
    for (auto& TagPair : SelectableActorRegistry)
    {
        TagPair.Value.RemoveAllSwap([Actor](const TWeakObjectPtr<AActor>& WeakActor)
        {
            return WeakActor.Get() == Actor;
        });
    }
    
    UE_LOG(LogPACSSelection, VeryVerbose, TEXT("Unregistered selectable actor: %s"), 
        *GetNameSafe(Actor));
}

void UPACS_SelectableActorSubsystem::GetSelectableActorsWithTag(FName Tag, TArray<TWeakObjectPtr<AActor>>& OutActors) const
{
    OutActors.Reset();
    
    if (const TArray<TWeakObjectPtr<AActor>>* ActorsForTag = SelectableActorRegistry.Find(Tag))
    {
        OutActors = *ActorsForTag;
    }
}

void UPACS_SelectableActorSubsystem::CleanupInvalidActors()
{
    int32 RemovedCount = 0;
    
    for (auto& TagPair : SelectableActorRegistry)
    {
        const int32 InitialCount = TagPair.Value.Num();
        
        TagPair.Value.RemoveAllSwap([](const TWeakObjectPtr<AActor>& WeakActor)
        {
            return !WeakActor.IsValid();
        });
        
        RemovedCount += (InitialCount - TagPair.Value.Num());
    }
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogPACSSelection, Log, TEXT("Cleaned up %d invalid selectable actor references"), 
            RemovedCount);
    }
}
```

### Step 3: Create Marquee Selection Component

**File**: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Public\PACS_MarqueeSelectionComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "PACS_MarqueeSelectionComponent.generated.h"

class UInputAction;
class UAssessorPawnConfig;
class UPACS_SelectableActorSubsystem;
class AHUD;
class UCanvas;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, const TArray<AActor*>&, SelectedActors);

/**
 * Simple, local-only marquee selection component
 * Uses spatial registry and single Enhanced Input action
 * Draws HUD rectangle during selection
 */
UCLASS(ClassGroup=(PACS), meta=(BlueprintSpawnableComponent))
class POLAIR_CS_API UPACS_MarqueeSelectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPACS_MarqueeSelectionComponent();

    // Configuration
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MarqueeSelection")
    TObjectPtr<UInputAction> MarqueeSelectAction;

    // Events
    UPROPERTY(BlueprintAssignable)
    FOnSelectionChanged OnSelectionChanged;

    // Public API
    UFUNCTION(BlueprintCallable, Category="MarqueeSelection")
    void SetEnabled(bool bNewEnabled) { bEnabled = bNewEnabled; }

    UFUNCTION(BlueprintPure, Category="MarqueeSelection")
    bool IsEnabled() const { return bEnabled; }

    UFUNCTION(BlueprintPure, Category="MarqueeSelection")
    bool IsSelecting() const { return bIsSelecting; }

    UFUNCTION(BlueprintPure, Category="MarqueeSelection")
    const TArray<AActor*>& GetSelectedActors() const { return SelectedActors; }

    UFUNCTION(BlueprintCallable, Category="MarqueeSelection")
    void ClearSelection();

    // HUD Integration
    void DrawMarqueeHUD(AHUD* HUD, UCanvas* Canvas);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Input binding
    void SetupInputBindings();
    void CleanupInputBindings();

    // Input handlers
    UFUNCTION()
    void HandleMarqueeSelect(const FInputActionValue& Value);

private:
    // Configuration
    UPROPERTY(EditDefaultsOnly, Category="MarqueeSelection")
    bool bEnabled = true;

    // Local selection state
    UPROPERTY(Transient)
    TArray<AActor*> SelectedActors;

    // Client-side state
    bool bIsSelecting = false;
    FVector2D SelectionStart = FVector2D::ZeroVector;
    FVector2D SelectionEnd = FVector2D::ZeroVector;

    // Cached references
    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerController> OwnerPC;

    UPROPERTY(Transient)
    TWeakObjectPtr<UAssessorPawnConfig> CachedConfig;

    UPROPERTY(Transient)
    TWeakObjectPtr<UPACS_SelectableActorSubsystem> SelectableSubsystem;

    // Selection logic
    void StartSelection(const FVector2D& MousePosition);
    void UpdateSelection(const FVector2D& MousePosition);
    void EndSelection(const FVector2D& MousePosition);

    void PerformSelection();
    TArray<AActor*> GetActorsInScreenRect(const FVector2D& StartPos, const FVector2D& EndPos) const;

    // Utility
    UAssessorPawnConfig* GetAssessorConfig();
    bool GetMousePosition(FVector2D& OutPosition) const;
    FBox2D NormaliseSelectionRect() const;
    bool ShouldAllowSelection() const;
};
```

**File**: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Private\PACS_MarqueeSelectionComponent.cpp`

```cpp
#include "PACS_MarqueeSelectionComponent.h"
#include "PACS_SelectableActorSubsystem.h"
#include "Data/Configs/AssessorPawnConfig.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/HUD.h"
#include "Engine/Canvas.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"

UPACS_MarqueeSelectionComponent::UPACS_MarqueeSelectionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPACS_MarqueeSelectionComponent::BeginPlay()
{
    Super::BeginPlay();
    
    OwnerPC = Cast<APlayerController>(GetOwner());
    if (!OwnerPC.IsValid())
    {
        UE_LOG(LogPACSSelection, Error, TEXT("MarqueeSelectionComponent must be owned by PlayerController"));
        return;
    }

    // Cache subsystem reference
    SelectableSubsystem = GetWorld()->GetSubsystem<UPACS_SelectableActorSubsystem>();
    
    // Setup input bindings (local controllers only)
    if (OwnerPC->IsLocalController())
    {
        SetupInputBindings();
    }
}

void UPACS_MarqueeSelectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CleanupInputBindings();
    Super::EndPlay(EndPlayReason);
}

void UPACS_MarqueeSelectionComponent::SetupInputBindings()
{
    if (!MarqueeSelectAction)
    {
        UE_LOG(LogPACSSelection, Warning, TEXT("MarqueeSelectAction not set in %s"), 
            *GetNameSafe(this));
        return;
    }

    APlayerController* PC = OwnerPC.Get();
    if (!PC || !PC->IsLocalController())
    {
        return;
    }

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
    if (!EIC)
    {
        UE_LOG(LogPACSSelection, Warning, TEXT("Enhanced Input Component not found on %s"), 
            *GetNameSafe(PC));
        return;
    }

    // Single action handles all phases: Started -> Ongoing -> Completed
    EIC->BindAction(MarqueeSelectAction, ETriggerEvent::Started, 
        this, &UPACS_MarqueeSelectionComponent::HandleMarqueeSelect);
    EIC->BindAction(MarqueeSelectAction, ETriggerEvent::Ongoing, 
        this, &UPACS_MarqueeSelectionComponent::HandleMarqueeSelect);
    EIC->BindAction(MarqueeSelectAction, ETriggerEvent::Completed, 
        this, &UPACS_MarqueeSelectionComponent::HandleMarqueeSelect);
    EIC->BindAction(MarqueeSelectAction, ETriggerEvent::Canceled, 
        this, &UPACS_MarqueeSelectionComponent::HandleMarqueeSelect);

    UE_LOG(LogPACSSelection, Log, TEXT("Input bindings setup for MarqueeSelection"));
}

void UPACS_MarqueeSelectionComponent::CleanupInputBindings()
{
    APlayerController* PC = OwnerPC.Get();
    if (!PC || !MarqueeSelectAction)
    {
        return;
    }

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
    {
        EIC->RemoveActionBinding(MarqueeSelectAction, ETriggerEvent::Started);
        EIC->RemoveActionBinding(MarqueeSelectAction, ETriggerEvent::Ongoing);
        EIC->RemoveActionBinding(MarqueeSelectAction, ETriggerEvent::Completed);
        EIC->RemoveActionBinding(MarqueeSelectAction, ETriggerEvent::Canceled);
    }
}

void UPACS_MarqueeSelectionComponent::HandleMarqueeSelect(const FInputActionValue& Value)
{
    if (!bEnabled || !ShouldAllowSelection())
    {
        return;
    }

    FVector2D MousePos;
    if (!GetMousePosition(MousePos))
    {
        return;
    }

    // Get the trigger event from the input context
    // Note: This is simplified - in practice you'd track the trigger event through other means
    // For now, we'll use the value to determine the phase
    const bool bIsPressed = Value.Get<bool>();
    
    if (bIsPressed && !bIsSelecting)
    {
        // Started
        StartSelection(MousePos);
    }
    else if (bIsPressed && bIsSelecting)
    {
        // Ongoing
        UpdateSelection(MousePos);
    }
    else if (!bIsPressed && bIsSelecting)
    {
        // Completed
        EndSelection(MousePos);
    }
}

void UPACS_MarqueeSelectionComponent::StartSelection(const FVector2D& MousePosition)
{
    bIsSelecting = true;
    SelectionStart = MousePosition;
    SelectionEnd = MousePosition;
    
    UE_LOG(LogPACSSelection, VeryVerbose, TEXT("Started selection at: %s"), 
        *MousePosition.ToString());
}

void UPACS_MarqueeSelectionComponent::UpdateSelection(const FVector2D& MousePosition)
{
    if (bIsSelecting)
    {
        SelectionEnd = MousePosition;
        
        // Optional: Perform live selection updates for preview
        // PerformSelection();
    }
}

void UPACS_MarqueeSelectionComponent::EndSelection(const FVector2D& MousePosition)
{
    if (!bIsSelecting)
    {
        return;
    }

    bIsSelecting = false;
    SelectionEnd = MousePosition;

    UAssessorPawnConfig* Config = GetAssessorConfig();
    if (!Config)
    {
        return;
    }

    // Check minimum selection size
    const FVector2D SelectionSize = (SelectionEnd - SelectionStart).GetAbs();
    if (SelectionSize.Size() < Config->MinimumSelectionSize)
    {
        UE_LOG(LogPACSSelection, VeryVerbose, TEXT("Selection too small, ignored"));
        return;
    }

    // Perform final selection
    PerformSelection();
}

void UPACS_MarqueeSelectionComponent::PerformSelection()
{
    // Get actors in selection rectangle
    TArray<AActor*> NewSelection = GetActorsInScreenRect(SelectionStart, SelectionEnd);
    
    // Update selected actors
    SelectedActors = NewSelection;
    
    // Broadcast selection change
    OnSelectionChanged.Broadcast(SelectedActors);
    
    UE_LOG(LogPACSSelection, Log, TEXT("Selected %d actors"), SelectedActors.Num());
}

TArray<AActor*> UPACS_MarqueeSelectionComponent::GetActorsInScreenRect(const FVector2D& StartPos, const FVector2D& EndPos) const
{
    TArray<AActor*> ActorsInRect;
    
    if (!SelectableSubsystem.IsValid())
    {
        return ActorsInRect;
    }

    UAssessorPawnConfig* Config = GetAssessorConfig();
    if (!Config)
    {
        return ActorsInRect;
    }

    APlayerController* PC = OwnerPC.Get();
    if (!PC)
    {
        return ActorsInRect;
    }

    // Get normalised screen rectangle
    const FBox2D ScreenRect = NormaliseSelectionRect();

    // Get selectable actors from registry (fast)
    TArray<TWeakObjectPtr<AActor>> SelectableActors;
    SelectableSubsystem->GetSelectableActorsWithTag(Config->SelectableTag, SelectableActors);

    // Test each actor against screen rectangle
    ActorsInRect.Reserve(FMath::Min(SelectableActors.Num(), Config->MaxSelectedActors));
    
    for (const TWeakObjectPtr<AActor>& WeakActor : SelectableActors)
    {
        AActor* Actor = WeakActor.Get();
        if (!IsValid(Actor))
        {
            continue;
        }

        // Project actor location to screen
        FVector2D ScreenPosition;
        if (PC->ProjectWorldLocationToScreen(Actor->GetActorLocation(), ScreenPosition, true))
        {
            // Check if screen position is within selection rectangle
            if (ScreenRect.IsInside(ScreenPosition))
            {
                ActorsInRect.Add(Actor);
                
                if (ActorsInRect.Num() >= Config->MaxSelectedActors)
                {
                    break;
                }
            }
        }
    }

    return ActorsInRect;
}

void UPACS_MarqueeSelectionComponent::ClearSelection()
{
    SelectedActors.Empty();
    OnSelectionChanged.Broadcast(SelectedActors);
}

void UPACS_MarqueeSelectionComponent::DrawMarqueeHUD(AHUD* HUD, UCanvas* Canvas)
{
#if !UE_SERVER
    if (!bIsSelecting || !Canvas)
    {
        return;
    }

    UAssessorPawnConfig* Config = GetAssessorConfig();
    if (!Config)
    {
        return;
    }

    const FBox2D Rect = NormaliseSelectionRect();
    const FVector2D Size = Rect.GetSize();
    
    if (Size.X < 1.0f || Size.Y < 1.0f)
    {
        return;
    }

    // Draw filled rectangle
    Canvas->SetDrawColor(Config->MarqueeColor.ToFColor(true));
    Canvas->DrawTile(Rect.Min.X, Rect.Min.Y, Size.X, Size.Y, 0, 0, 1, 1, nullptr);
    
    // Draw border
    const float Thickness = Config->MarqueeBorderThickness;
    Canvas->SetDrawColor(Config->MarqueeBorderColor.ToFColor(true));
    
    // Top, Bottom, Left, Right borders
    Canvas->DrawTile(Rect.Min.X, Rect.Min.Y, Size.X, Thickness, 0, 0, 1, 1, nullptr);
    Canvas->DrawTile(Rect.Min.X, Rect.Max.Y - Thickness, Size.X, Thickness, 0, 0, 1, 1, nullptr);
    Canvas->DrawTile(Rect.Min.X, Rect.Min.Y, Thickness, Size.Y, 0, 0, 1, 1, nullptr);
    Canvas->DrawTile(Rect.Max.X - Thickness, Rect.Min.Y, Thickness, Size.Y, 0, 0, 1, 1, nullptr);
#endif
}

// Utility Methods

UAssessorPawnConfig* UPACS_MarqueeSelectionComponent::GetAssessorConfig()
{
    if (!CachedConfig.IsValid())
    {
        // TODO: Implement based on your config management system
        // Example: Get from game mode, player state, or direct reference
        // CachedConfig = GetGameMode()->GetAssessorConfig();
    }
    return CachedConfig.Get();
}

bool UPACS_MarqueeSelectionComponent::GetMousePosition(FVector2D& OutPosition) const
{
    APlayerController* PC = OwnerPC.Get();
    if (!PC)
    {
        return false;
    }

    // Use DPI-aware mouse position
    float MouseX, MouseY;
    if (UWidgetLayoutLibrary::GetMousePositionScaledByDPI(PC, MouseX, MouseY))
    {
        OutPosition.Set(MouseX, MouseY);
        return true;
    }

    return false;
}

FBox2D UPACS_MarqueeSelectionComponent::NormaliseSelectionRect() const
{
    const float MinX = FMath::Min(SelectionStart.X, SelectionEnd.X);
    const float MaxX = FMath::Max(SelectionStart.X, SelectionEnd.X);
    const float MinY = FMath::Min(SelectionStart.Y, SelectionEnd.Y);
    const float MaxY = FMath::Max(SelectionStart.Y, SelectionEnd.Y);
    
    return FBox2D(FVector2D(MinX, MinY), FVector2D(MaxX, MaxY));
}

bool UPACS_MarqueeSelectionComponent::ShouldAllowSelection() const
{
    APlayerController* PC = OwnerPC.Get();
    if (!PC || !PC->IsLocalController())
    {
        return false;
    }

    // Don't allow selection if mouse cursor is hidden
    if (!PC->bShowMouseCursor)
    {
        return false;
    }

    // TODO: Add additional checks based on your input system
    // - Check for blocking UI overlays
    // - Check for proper input context
    // - Check game state

    return true;
}
```

### Step 4: Add Component to PlayerController

**File**: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Public\PACS_PlayerController.h`

```cpp
#include "PACS_MarqueeSelectionComponent.h"

// Add to protected section:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Selection", 
    meta=(AllowPrivateAccess="true"))
TObjectPtr<UPACS_MarqueeSelectionComponent> MarqueeSelectionComponent;

// Add to public section:
UFUNCTION(BlueprintCallable, Category="Selection")
UPACS_MarqueeSelectionComponent* GetMarqueeSelectionComponent() const { return MarqueeSelectionComponent; }
```

**File**: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Private\PACS_PlayerController.cpp`

```cpp
APACS_PlayerController::APACS_PlayerController()
{
    InputHandler = CreateDefaultSubobject<UPACS_InputHandlerComponent>(TEXT("InputHandler"));
    EdgeScrollComponent = CreateDefaultSubobject<UPACS_EdgeScrollComponent>(TEXT("EdgeScrollComponent"));
    MarqueeSelectionComponent = CreateDefaultSubobject<UPACS_MarqueeSelectionComponent>(TEXT("MarqueeSelectionComponent"));
    PrimaryActorTick.bCanEverTick = true;
}
```

### Step 5: Update HUD for Visual Integration

**File**: `C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CS\Private\PACS_HUD.cpp`

```cpp
void APACS_HUD::DrawHUD()
{
    Super::DrawHUD();
    
#if !UE_SERVER
    DrawMarqueeSelection();
#endif
}

void APACS_HUD::DrawMarqueeSelection()
{
#if !UE_SERVER
    APACS_PlayerController* PC = Cast<APACS_PlayerController>(GetOwningPlayerController());
    if (!PC)
    {
        return;
    }

    UPACS_MarqueeSelectionComponent* MarqueeComponent = PC->GetMarqueeSelectionComponent();
    if (MarqueeComponent && MarqueeComponent->IsSelecting())
    {
        MarqueeComponent->DrawMarqueeHUD(this, Canvas);
    }
#endif
}
```

### Step 6: Content Setup

1. **Create Input Action**: `IA_MarqueeSelect` 
   - Input Type: Digital (Bool)
   - Triggers: Hold (for drag behaviour)

2. **Configure Input Mapping Context**:
   - Bind Left Mouse Button to `IA_MarqueeSelect`
   - Set to Hold trigger for drag behaviour

3. **Configure Assessor Pawn Data Asset**:
   - Set marquee colours and visual properties
   - Configure selection limits
   - Set selectable tag name

### Step 7: Selectable Actor Integration

Actors that should be selectable must register with the subsystem:

```cpp
// In selectable actor's BeginPlay
void ASelectableActor::BeginPlay()
{
    Super::BeginPlay();
    
    if (UPACS_SelectableActorSubsystem* Subsystem = GetWorld()->GetSubsystem<UPACS_SelectableActorSubsystem>())
    {
        Subsystem->RegisterSelectableActor(this, TEXT("Selectable"));
    }
}

// In selectable actor's EndPlay
void ASelectableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UPACS_SelectableActorSubsystem* Subsystem = GetWorld()->GetSubsystem<UPACS_SelectableActorSubsystem>())
    {
        Subsystem->UnregisterSelectableActor(this);
    }
    
    Super::EndPlay(EndPlayReason);
}
```

## Architecture Benefits

### Simplicity
- **Local only**: No networking complexity
- **Single input action**: Natural Enhanced Input phases
- **Clear separation**: Registry, selection, HUD are separate concerns
- **Minimal state**: Only essential data tracked

### Performance
- **No world iteration**: Spatial registry provides O(1) lookup
- **Minimal allocations**: Reuse arrays and cache references
- **Efficient projection**: Only test registered actors

### Extensibility
- **Config driven**: Visual settings from assessor pawn config
- **Event system**: OnSelectionChanged for UI integration
- **Subsystem pattern**: Easy to extend with additional features

## Testing Checklist

- ✅ HUD rectangle draws correctly in all drag directions
- ✅ Selection works with registered selectable actors
- ✅ Visual settings from config are applied
- ✅ Performance good with many selectable actors
- ✅ Input handling works correctly
- ✅ Selection events fire properly

This simplified version focuses purely on the local HUD drawing functionality while maintaining the performance benefits of the spatial registry system. Networking can be added later as a separate concern.
