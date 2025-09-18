# Phase 1.5 — Edge Scrolling Implementation (Component-Based Architecture)

## Overview

Implement edge scrolling as a dedicated Actor Component that:
- Lives on the PlayerController (client-side only)
- Does NOT modify existing InputHandler code
- Uses InputHandler as read-only permission system
- Provides production-quality DPI awareness, UI detection, and cursor protection
- Maintains all architectural principles while ensuring system isolation

## Ground Rules

1. **Do NOT modify InputHandler** — use existing methods as read-only queries
2. **Component isolation** — all edge scroll logic contained in new component
3. **Client-side only** — no replication, purely visual navigation
4. **Production quality** — proper error handling, logging, and performance optimization
5. **Testable and toggleable** — can be easily enabled/disabled for debugging

## Architecture Overview

```
PlayerController
├── InputHandlerComponent (UNCHANGED - read-only queries)
└── EdgeScrollComponent (NEW - owns all edge scroll logic)
    └── Queries InputHandler for permission
    └── Applies input to AssessorPawn via AddPlanarInput()
```

## Implementation Steps

### Step 1: Extend AssessorPawnConfig with Context Control

**File**: `Source/POLAIR_CS/Public/Data/Configs/AssessorPawnConfig.h`

Add edge scrolling configuration with context filtering:

```cpp
// Edge Scrolling Configuration
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|EdgeScroll")
bool bEdgeScrollEnabled = true;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="4", ClampMax="128", EditCondition="bEdgeScrollEnabled"), Category="Assessor|EdgeScroll")
int32 EdgeMarginPx = 24;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.1", ClampMax="1.0", EditCondition="bEdgeScrollEnabled"), Category="Assessor|EdgeScroll")
float EdgeMaxSpeedScale = 0.8f;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="0.2", EditCondition="bEdgeScrollEnabled"), Category="Assessor|EdgeScroll")
float EdgeScrollDeadZone = 0.05f;

// Context Control: Edge scrolling will ONLY work when these contexts are active
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|EdgeScroll",
    meta=(EditCondition="bEdgeScrollEnabled", DisplayName="Allowed Input Contexts"))
TArray<TObjectPtr<UInputMappingContext>> EdgeScrollAllowedContexts;

// Optional simple world bounds (leave invalid for no clamping)
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|WorldBounds")
FBox OptionalWorldBounds = FBox(ForceInit);
```

### Step 2: Create EdgeScrollComponent

**File**: `Source/POLAIR_CS/Public/PACS_EdgeScrollComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PACS_EdgeScrollComponent.generated.h"

class UPACS_InputHandlerComponent;
class APACS_AssessorPawn;
class UAssessorPawnConfig;

/**
 * Edge scrolling component for PlayerController
 * Handles mouse edge detection and applies movement to AssessorPawn
 * Client-side only - no replication required
 */
UCLASS(NotBlueprintable, ClassGroup=(Input), meta=(BlueprintSpawnableComponent=false))
class POLAIR_CS_API UPACS_EdgeScrollComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPACS_EdgeScrollComponent();

    // Enable/disable edge scrolling (for debugging/testing)
    UFUNCTION(BlueprintCallable, Category="EdgeScroll")
    void SetEnabled(bool bNewEnabled) { bEnabled = bNewEnabled; }

    UFUNCTION(BlueprintPure, Category="EdgeScroll")
    bool IsEnabled() const { return bEnabled; }

    UFUNCTION(BlueprintPure, Category="EdgeScroll")
    bool IsActivelyScrolling() const { return bIsActivelyScrolling; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // Configuration
    UPROPERTY(EditDefaultsOnly, Category="EdgeScroll")
    bool bEnabled = true;

    // State tracking
    bool bIsActivelyScrolling = false;
    bool bComponentReady = false;
    
    // Performance caching
    mutable bool bPermissionCacheValid = false;
    mutable bool bCachedPermissionResult = false;
    mutable float PermissionCacheTime = 0.0f;
    static constexpr float PermissionCacheLifetime = 0.1f; // 100ms cache
    
    // Viewport caching (invalidated on window resize)
    FVector2D CachedViewportSize = FVector2D::ZeroVector;
    float CachedDPIScale = 1.0f;
    bool bViewportCacheValid = false;
    
    // Component references (cached for performance)
    UPROPERTY()
    TWeakObjectPtr<UPACS_InputHandlerComponent> CachedInputHandler;
    
    UPROPERTY()
    TWeakObjectPtr<APACS_AssessorPawn> CachedAssessorPawn;

    // Core logic methods
    bool ShouldAllowEdgeScrolling() const;
    FVector2D ComputeEdgeScrollInput() const;
    
    // Helper methods
    bool GetDPIAwareMousePosition(FVector2D& OutMousePos) const;
    bool IsMouseOverInteractiveUI(const FVector2D& MousePos, const FVector2D& ViewportSize, int32 EdgeMarginPx) const;
    bool IsMouseCapturedOrButtonPressed() const;
    bool IsCurrentContextAllowedForEdgeScrolling() const;
    bool UpdateViewportCache() const;
    void UpdateComponentReadiness();
    void InvalidateCaches();
    
    // Safe getters
    UPACS_InputHandlerComponent* GetInputHandler() const;
    APACS_AssessorPawn* GetAssessorPawn() const;
    const UAssessorPawnConfig* GetAssessorConfig() const;
    APlayerController* GetPlayerController() const;

#if !UE_SERVER
    // Delegate handle for window resize events
    FDelegateHandle ViewportResizedHandle;
    void OnViewportResized(FViewport* Viewport, uint32 ResizeType);
#endif
};
```

### Step 3: Implement EdgeScrollComponent

**File**: `Source/POLAIR_CS/Private/PACS_EdgeScrollComponent.cpp`

```cpp
#include "PACS_EdgeScrollComponent.h"
#include "PACS_InputHandlerComponent.h"
#include "PACS_PlayerController.h"
#include "Pawns/Assessor/PACS_AssessorPawn.h"
#include "Data/Configs/AssessorPawnConfig.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"

#if !UE_SERVER
#include "Blueprint/WidgetLayoutLibrary.h"
#endif

UPACS_EdgeScrollComponent::UPACS_EdgeScrollComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // After input processing
    
    // Only tick on clients
    PrimaryComponentTick.bTickEvenWhenPaused = false;
    SetComponentTickEnabled(true);
    
#if UE_SERVER
    SetComponentTickEnabled(false);
#endif
}

void UPACS_EdgeScrollComponent::BeginPlay()
{
    Super::BeginPlay();
    
#if !UE_SERVER
    // Subscribe to viewport resize events for cache invalidation
    if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
    {
        ViewportResizedHandle = ViewportClient->ViewportResizedEvent.AddUObject(
            this, &UPACS_EdgeScrollComponent::OnViewportResized);
    }
    
    UE_LOG(LogPACSInput, Log, TEXT("EdgeScrollComponent initialized on %s"), 
        *GetNameSafe(GetOwner()));
#endif
}

void UPACS_EdgeScrollComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_SERVER
    // Clean up viewport resize delegate
    if (ViewportResizedHandle.IsValid())
    {
        if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
        {
            ViewportClient->ViewportResizedEvent.Remove(ViewportResizedHandle);
        }
        ViewportResizedHandle.Reset();
    }
#endif
    
    Super::EndPlay(EndPlayReason);
}

void UPACS_EdgeScrollComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
#if !UE_SERVER
    if (!bEnabled)
    {
        bIsActivelyScrolling = false;
        return;
    }
    
    UpdateComponentReadiness();
    
    if (!bComponentReady)
    {
        bIsActivelyScrolling = false;
        return;
    }
    
    // Check if edge scrolling should be allowed
    if (!ShouldAllowEdgeScrolling())
    {
        bIsActivelyScrolling = false;
        return;
    }
    
    // Compute edge scroll input
    FVector2D EdgeAxis = ComputeEdgeScrollInput();
    
    const bool bWasActivelyScrolling = bIsActivelyScrolling;
    bIsActivelyScrolling = !EdgeAxis.IsNearlyZero();
    
    // Log state changes
    if (bIsActivelyScrolling != bWasActivelyScrolling)
    {
        UE_LOG(LogPACSInput, VeryVerbose, TEXT("Edge scrolling %s"), 
            bIsActivelyScrolling ? TEXT("started") : TEXT("stopped"));
    }
    
    // Apply edge scroll input to pawn
    if (bIsActivelyScrolling)
    {
        if (APACS_AssessorPawn* AssessorPawn = GetAssessorPawn())
        {
            AssessorPawn->AddPlanarInput(EdgeAxis);
            UE_LOG(LogPACSInput, VeryVerbose, TEXT("Applied edge scroll input: %s"), *EdgeAxis.ToString());
        }
    }
#endif
}

bool UPACS_EdgeScrollComponent::ShouldAllowEdgeScrolling() const
{
#if UE_SERVER
    return false;
#endif

    // Use cached result if still valid (performance optimization)
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (bPermissionCacheValid && (CurrentTime - PermissionCacheTime) < PermissionCacheLifetime)
    {
        return bCachedPermissionResult;
    }

    bool bAllowed = false;
    FString DisallowReason;

    // Check InputHandler permission (read-only queries - no modifications)
    UPACS_InputHandlerComponent* InputHandler = GetInputHandler();
    if (!InputHandler)
    {
        DisallowReason = TEXT("NoInputHandler");
    }
    else if (!InputHandler->IsHealthy())
    {
        DisallowReason = TEXT("InputHandlerNotHealthy");
    }
    else if (InputHandler->HasBlockingOverlay())
    {
        DisallowReason = TEXT("HasBlockingOverlay");
    }
    else if (!IsCurrentContextAllowedForEdgeScrolling())
    {
        DisallowReason = TEXT("ContextNotAllowed");
    }
    else
    {
        // Check window focus and mouse state
        if (!FSlateApplication::IsInitialized())
        {
            DisallowReason = TEXT("SlateNotInitialized");
        }
        else
        {
            TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
            if (!ActiveWindow.IsValid() || !ActiveWindow->HasFocusedDescendants())
            {
                DisallowReason = TEXT("WindowNotFocused");
            }
            else if (IsMouseCapturedOrButtonPressed())
            {
                DisallowReason = TEXT("MouseCaptured");
            }
            else
            {
                // Check pawn and config
                const UAssessorPawnConfig* Config = GetAssessorConfig();
                if (!Config || !Config->bEdgeScrollEnabled)
                {
                    DisallowReason = TEXT("ConfigDisabled");
                }
                else
                {
                    bAllowed = true;
                }
            }
        }
    }

    // Cache the result
    bCachedPermissionResult = bAllowed;
    bPermissionCacheValid = true;
    PermissionCacheTime = CurrentTime;

    // Log state changes (throttled)
    static bool bLastAllowed = false;
    static float LastLogTime = 0.0f;
    const bool bShouldLog = (bAllowed != bLastAllowed) || 
                           ((CurrentTime - LastLogTime) > 5.0f && !DisallowReason.IsEmpty());
    
    if (bShouldLog)
    {
        UE_LOG(LogPACSInput, Log, TEXT("Edge scrolling %s%s"), 
            bAllowed ? TEXT("ALLOWED") : TEXT("BLOCKED"),
            bAllowed ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *DisallowReason));
        
        bLastAllowed = bAllowed;
        LastLogTime = CurrentTime;
    }

    return bAllowed;
}

FVector2D UPACS_EdgeScrollComponent::ComputeEdgeScrollInput() const
{
#if UE_SERVER
    return FVector2D::ZeroVector;
#endif

    const UAssessorPawnConfig* Config = GetAssessorConfig();
    if (!Config)
    {
        return FVector2D::ZeroVector;
    }

    // Update viewport cache if needed
    if (!UpdateViewportCache())
    {
        return FVector2D::ZeroVector;
    }

    // Get DPI-aware mouse position
    FVector2D MousePos;
    if (!GetDPIAwareMousePosition(MousePos))
    {
        return FVector2D::ZeroVector;
    }

    // Check for UI hover blocking (only when near edges for performance)
    if (IsMouseOverInteractiveUI(MousePos, CachedViewportSize, Config->EdgeMarginPx))
    {
        UE_LOG(LogPACSInput, VeryVerbose, TEXT("Edge scroll blocked by UI hover"));
        return FVector2D::ZeroVector;
    }

    // Calculate DPI-aware edge margins
    const float EdgeMargin = static_cast<float>(Config->EdgeMarginPx) * CachedDPIScale;
    FVector2D Axis(0.0f, 0.0f);

    // Horizontal edge detection
    if (MousePos.X <= EdgeMargin)
    {
        Axis.X = FMath::GetMappedRangeValueClamped(
            FVector2D(0.0f, EdgeMargin), 
            FVector2D(-1.0f, 0.0f), 
            MousePos.X
        );
    }
    else if (MousePos.X >= CachedViewportSize.X - EdgeMargin)
    {
        Axis.X = FMath::GetMappedRangeValueClamped(
            FVector2D(CachedViewportSize.X - EdgeMargin, CachedViewportSize.X),
            FVector2D(0.0f, 1.0f),
            MousePos.X
        );
    }

    // Vertical edge detection
    if (MousePos.Y <= EdgeMargin)
    {
        Axis.Y = FMath::GetMappedRangeValueClamped(
            FVector2D(0.0f, EdgeMargin),
            FVector2D(1.0f, 0.0f),
            MousePos.Y
        );
    }
    else if (MousePos.Y >= CachedViewportSize.Y - EdgeMargin)
    {
        Axis.Y = FMath::GetMappedRangeValueClamped(
            FVector2D(CachedViewportSize.Y - EdgeMargin, CachedViewportSize.Y),
            FVector2D(0.0f, -1.0f),
            MousePos.Y
        );
    }

    // Apply scaling and dead zone
    Axis = Axis.GetClampedToMaxSize(1.0f) * Config->EdgeMaxSpeedScale;
    
    if (Axis.SizeSquared() < (Config->EdgeScrollDeadZone * Config->EdgeScrollDeadZone))
    {
        return FVector2D::ZeroVector;
    }

    return Axis;
}

bool UPACS_EdgeScrollComponent::GetDPIAwareMousePosition(FVector2D& OutMousePos) const
{
#if UE_SERVER
    return false;
#endif

    APlayerController* PC = GetPlayerController();
    if (!PC)
    {
        return false;
    }

    // Try UMG's DPI-aware method first
    float MouseX, MouseY;
    if (UWidgetLayoutLibrary::GetMousePositionScaledByDPI(PC, MouseX, MouseY))
    {
        OutMousePos.Set(MouseX, MouseY);
        return true;
    }

    // Fallback to standard method
    if (PC->GetMousePosition(MouseX, MouseY))
    {
        OutMousePos.Set(MouseX, MouseY);
        return true;
    }

    return false;
}

bool UPACS_EdgeScrollComponent::IsMouseOverInteractiveUI(const FVector2D& MousePos, const FVector2D& ViewportSize, int32 EdgeMarginPx) const
{
#if UE_SERVER
    return false;
#endif

    // Performance optimization: only check UI if mouse is actually near edges
    const float EdgeMargin = static_cast<float>(EdgeMarginPx) * CachedDPIScale;
    const bool bNearEdge = (MousePos.X <= EdgeMargin) || 
                          (MousePos.X >= ViewportSize.X - EdgeMargin) ||
                          (MousePos.Y <= EdgeMargin) || 
                          (MousePos.Y >= ViewportSize.Y - EdgeMargin);
    
    if (!bNearEdge)
    {
        return false;
    }

    if (!FSlateApplication::IsInitialized())
    {
        return false;
    }

    // Get the game viewport window
    TSharedPtr<SWindow> GameWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
    if (!GameWindow.IsValid())
    {
        return false;
    }

    // Check for interactive widgets under cursor
    TSharedPtr<SWidget> WidgetUnderCursor = FSlateApplication::Get().LocateWidgetInWindow(
        GameWindow, MousePos, false);
    
    if (WidgetUnderCursor.IsValid())
    {
        const bool bIsInteractive = WidgetUnderCursor->IsInteractable() || 
                                   WidgetUnderCursor->GetVisibility().IsHitTestVisible();
        
        if (bIsInteractive)
        {
            UE_LOG(LogPACSInput, VeryVerbose, TEXT("Edge scroll blocked: interactive UI at mouse position"));
            return true;
        }
    }
    
    return false;
}

bool UPACS_EdgeScrollComponent::IsMouseCapturedOrButtonPressed() const
{
#if UE_SERVER
    return false;
#endif

    if (!FSlateApplication::IsInitialized())
    {
        return false;
    }

    const FSlateApplication& SlateApp = FSlateApplication::Get();
    
    // Check if any mouse buttons are pressed
    if (SlateApp.GetPressedMouseButtons().Num() > 0)
    {
        return true;
    }
    
    // Check if mouse is captured (for future rotation mode)
    if (SlateApp.HasMouseCapture())
    {
        return true;
    }
    
    return false;
}

bool UPACS_EdgeScrollComponent::IsCurrentContextAllowedForEdgeScrolling() const
{
#if UE_SERVER
    return false;
#endif

    const UAssessorPawnConfig* Config = GetAssessorConfig();
    if (!Config)
    {
        return false;
    }

    // If no allowed contexts are specified, allow in any context (backward compatibility)
    if (Config->EdgeScrollAllowedContexts.Num() == 0)
    {
        UE_LOG(LogPACSInput, VeryVerbose, TEXT("No context restrictions - edge scrolling allowed"));
        return true;
    }

    UPACS_InputHandlerComponent* InputHandler = GetInputHandler();
    if (!InputHandler)
    {
        return false;
    }

    // Get the currently active base context from InputHandler
    // We need to check which IMC corresponds to the current context mode
    UInputMappingContext* CurrentBaseContext = nullptr;
    const FString CurrentContextName = InputHandler->GetCurrentContextName();
    
    // Map context names to IMCs through the InputHandler's config
    if (InputHandler->InputConfig)
    {
        if (CurrentContextName == TEXT("Gameplay"))
        {
            CurrentBaseContext = InputHandler->InputConfig->GameplayContext;
        }
        else if (CurrentContextName == TEXT("Menu"))
        {
            CurrentBaseContext = InputHandler->InputConfig->MenuContext;
        }
        else if (CurrentContextName == TEXT("UI"))
        {
            CurrentBaseContext = InputHandler->InputConfig->UIContext;
        }
    }

    if (!CurrentBaseContext)
    {
        UE_LOG(LogPACSInput, VeryVerbose, TEXT("Edge scroll blocked: no current base context found for '%s'"), *CurrentContextName);
        return false;
    }

    // Check if current context is in the allowed list
    const bool bContextAllowed = Config->EdgeScrollAllowedContexts.Contains(CurrentBaseContext);
    
    UE_LOG(LogPACSInput, VeryVerbose, TEXT("Edge scroll context check: '%s' -> %s (IMC: %s)"), 
        *CurrentContextName,
        bContextAllowed ? TEXT("ALLOWED") : TEXT("BLOCKED"),
        *GetNameSafe(CurrentBaseContext));

    return bContextAllowed;
}

bool UPACS_EdgeScrollComponent::UpdateViewportCache() const
{
#if UE_SERVER
    return false;
#endif

    if (bViewportCacheValid)
    {
        return true;
    }

    APlayerController* PC = GetPlayerController();
    if (!PC)
    {
        return false;
    }

    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP || !LP->ViewportClient)
    {
        return false;
    }

    LP->ViewportClient->GetViewportSize(CachedViewportSize);
    CachedDPIScale = LP->ViewportClient->GetDPIScale();
    
    if (CachedViewportSize.X <= 0 || CachedViewportSize.Y <= 0)
    {
        return false;
    }

    bViewportCacheValid = true;
    UE_LOG(LogPACSInput, VeryVerbose, TEXT("Updated viewport cache: Size=%s, DPI=%.2f"),
        *CachedViewportSize.ToString(), CachedDPIScale);
    
    return true;
}

void UPACS_EdgeScrollComponent::UpdateComponentReadiness()
{
#if !UE_SERVER
    const bool bWasReady = bComponentReady;
    
    bComponentReady = GetInputHandler() != nullptr &&
                     GetAssessorPawn() != nullptr &&
                     GetPlayerController() != nullptr &&
                     GetPlayerController()->IsLocalController();
    
    if (bComponentReady != bWasReady)
    {
        UE_LOG(LogPACSInput, Log, TEXT("EdgeScrollComponent readiness: %s"), 
            bComponentReady ? TEXT("READY") : TEXT("NOT READY"));
        
        if (!bComponentReady)
        {
            InvalidateCaches();
        }
    }
#endif
}

void UPACS_EdgeScrollComponent::InvalidateCaches()
{
    bPermissionCacheValid = false;
    bViewportCacheValid = false;
    
    // Clear weak references to force re-lookup
    CachedInputHandler.Reset();
    CachedAssessorPawn.Reset();
}

// Safe getter implementations
UPACS_InputHandlerComponent* UPACS_EdgeScrollComponent::GetInputHandler() const
{
    if (!CachedInputHandler.IsValid())
    {
        if (APACS_PlayerController* PC = Cast<APACS_PlayerController>(GetOwner()))
        {
            CachedInputHandler = PC->GetInputHandler();
        }
    }
    return CachedInputHandler.Get();
}

APACS_AssessorPawn* UPACS_EdgeScrollComponent::GetAssessorPawn() const
{
    if (!CachedAssessorPawn.IsValid())
    {
        if (APlayerController* PC = GetPlayerController())
        {
            CachedAssessorPawn = Cast<APACS_AssessorPawn>(PC->GetPawn());
        }
    }
    return CachedAssessorPawn.Get();
}

const UAssessorPawnConfig* UPACS_EdgeScrollComponent::GetAssessorConfig() const
{
    if (APACS_AssessorPawn* Pawn = GetAssessorPawn())
    {
        return Pawn->Config;
    }
    return nullptr;
}

APlayerController* UPACS_EdgeScrollComponent::GetPlayerController() const
{
    return Cast<APlayerController>(GetOwner());
}

#if !UE_SERVER
void UPACS_EdgeScrollComponent::OnViewportResized(FViewport* Viewport, uint32 ResizeType)
{
    // Invalidate viewport cache when window is resized
    bViewportCacheValid = false;
    UE_LOG(LogPACSInput, VeryVerbose, TEXT("Viewport resized - invalidating cache"));
}
#endif
```

### Step 4: Add Component to PlayerController

**File**: `Source/POLAIR_CS/Public/PACS_PlayerController.h`

Add the component:

```cpp
#include "PACS_EdgeScrollComponent.h"

// Add to protected section:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input", 
    meta=(AllowPrivateAccess="true"))
TObjectPtr<UPACS_EdgeScrollComponent> EdgeScrollComponent;

// Add to public section for debugging:
UFUNCTION(BlueprintCallable, Category="Input")
UPACS_EdgeScrollComponent* GetEdgeScrollComponent() const { return EdgeScrollComponent; }
```

**File**: `Source/POLAIR_CS/Private/PACS_PlayerController.cpp`

Initialize the component:

```cpp
APACS_PlayerController::APACS_PlayerController()
{
    InputHandler = CreateDefaultSubobject<UPACS_InputHandlerComponent>(TEXT("InputHandler"));
    EdgeScrollComponent = CreateDefaultSubobject<UPACS_EdgeScrollComponent>(TEXT("EdgeScrollComponent"));
    PrimaryActorTick.bCanEverTick = true;
}
```

### Step 5: Maintain Input Accumulation Fix in AssessorPawn

**File**: `Source/POLAIR_CS/Private/Pawns/Assessor/PACS_AssessorPawn.cpp`

Ensure proper input clamping (unchanged from previous implementation):

```cpp
void APACS_AssessorPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Combine all input sources FIRST, then clamp once
    FVector2D CombinedInput(InputRight, InputForward);
    
    // Single clamp point - prevents any speed exploitation
    CombinedInput = CombinedInput.GetClampedToMaxSize(1.0f);
    
    // Convert to world movement
    const FVector Fwd = AxisBasis->GetForwardVector();
    const FVector Right = AxisBasis->GetRightVector(); 
    const float Speed = (Config ? Config->MoveSpeed : 2400.f);
    
    FVector Delta = (Fwd * CombinedInput.Y + Right * CombinedInput.X) * Speed * DeltaSeconds;
    Delta.Z = 0.f;
    
    // Optional world bounds clamping
    if (Config && Config->OptionalWorldBounds.IsValid)
    {
        const FVector CurrentLocation = GetActorLocation();
        const FVector ProposedLocation = CurrentLocation + Delta;
        const FVector ClampedLocation = Config->OptionalWorldBounds.GetClosestPointTo(ProposedLocation);
        
        // Check if we hit bounds (for debugging)
        if (!ClampedLocation.Equals(ProposedLocation, 0.1f))
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("AssessorPawn movement clamped to world bounds"));
            Delta = ClampedLocation - CurrentLocation;
        }
    }
    
    // Apply movement
    if (!Delta.IsNearlyZero())
    {
        AddActorWorldOffset(Delta, false);
    }

    // Update camera arm
    SpringArm->TargetArmLength = TargetArmLength;

    // Clear per-frame inputs
    InputForward = 0.f;
    InputRight = 0.f;
}
```

## Content Setup

### Step 6: Configure Data Asset with Context Control

Update `DA_AssessorPawn`:
- Set `bEdgeScrollEnabled = true`
- Configure `EdgeMarginPx = 24`
- Set `EdgeMaxSpeedScale = 0.8`
- Set `EdgeScrollDeadZone = 0.05`
- **Add your GameplayContext IMC to `EdgeScrollAllowedContexts` array**
- Leave `OptionalWorldBounds` invalid unless you need movement clamping

**Important**: In the `EdgeScrollAllowedContexts` array, add only the Input Mapping Contexts where you want edge scrolling to work. For example:
- Add `IMC_Gameplay` to allow edge scrolling during gameplay
- Do NOT add `IMC_Menu` or `IMC_UI` to block edge scrolling in those contexts
- Leave the array empty to allow edge scrolling in all contexts (backward compatibility)

## Testing and Debugging

### Step 7: Component Testing

The component-based design makes testing much easier:

**Enable/Disable Testing**:
```cpp
// In Blueprint or C++
PlayerController->GetEdgeScrollComponent()->SetEnabled(false); // Disable for testing
```

**Context Control Testing**:
```cpp
// Test different contexts
EdgeScrollComponent->SetEnabled(true);

// Should work: When GameplayContext is active and in allowed list
InputHandler->SetBaseContext(EPACS_InputContextMode::Gameplay);

// Should NOT work: When MenuContext is active (not in allowed list)  
InputHandler->SetBaseContext(EPACS_InputContextMode::Menu);

// Verify context blocking
bool bScrolling = EdgeScrollComponent->IsActivelyScrolling(); // Should be false in Menu context
```

**Console Commands** (optional):
- `EdgeScroll.Enable 0/1` - Toggle edge scrolling
- `EdgeScroll.Debug 0/1` - Toggle debug logging

## Architecture Benefits

### Clean Separation of Concerns

**InputHandler** (UNCHANGED):
- Context management
- Input routing  
- Permission queries

**EdgeScrollComponent** (NEW):
- Mouse edge detection
- DPI handling
- UI hover detection
- Movement application

**PlayerController**:
- Owns both components
- No edge scroll logic

### Key Advantages

1. **Zero Risk to Existing Code**: InputHandler remains completely untouched
2. **Easy Testing**: Component can be disabled/enabled independently  
3. **Clear Ownership**: All edge scroll logic in one place
4. **Performance**: Smart caching and early exits
5. **Future Extensible**: Easy to add touch edge scrolling or other variants
6. **Debugging**: Comprehensive logging with throttling
7. **Production Quality**: Proper error handling and graceful degradation

## Completion Criteria

Before marking production-ready:

1. ✅ **InputHandler Unchanged**: No modifications to existing, working code
2. ✅ **Component Isolation**: All edge scroll logic contained in component
3. ✅ **DPI Consistency**: Works correctly across different displays and PIE
4. ✅ **UI Safety**: No camera movement when hovering over interactive UI
5. ✅ **Cursor Protection**: Proper handling of mouse capture and button states
6. ✅ **Performance**: Smart caching with proper invalidation
7. ✅ **Input Math**: Combined inputs properly clamped
8. ✅ **Error Handling**: Graceful degradation with logging
9. ✅ **Testability**: Component can be easily enabled/disabled
10. ✅ **Documentation**: Clear architecture and integration patterns

This component-based approach provides all the production quality features while maintaining complete isolation from your existing, working systems. Claude Code can implement this without touching any existing InputHandler code, eliminating the risk of breaking working functionality.