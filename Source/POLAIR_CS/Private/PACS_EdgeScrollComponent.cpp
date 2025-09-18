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
    // No ViewportResizedEvent in UE5.5 - use tick-based viewport checking instead

    UE_LOG(LogTemp, Log, TEXT("EdgeScrollComponent initialized on %s"),
        *GetNameSafe(GetOwner()));
#endif
}

void UPACS_EdgeScrollComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_SERVER
    // No viewport delegate cleanup needed in UE5.5
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
        UE_LOG(LogTemp, VeryVerbose, TEXT("Edge scrolling %s"),
            bIsActivelyScrolling ? TEXT("started") : TEXT("stopped"));
    }

    // Apply edge scroll input to pawn
    if (bIsActivelyScrolling)
    {
        if (APACS_AssessorPawn* AssessorPawn = GetAssessorPawn())
        {
            AssessorPawn->AddPlanarInput(EdgeAxis);
            UE_LOG(LogTemp, VeryVerbose, TEXT("Applied edge scroll input: %s"), *EdgeAxis.ToString());
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
        UE_LOG(LogTemp, Log, TEXT("Edge scrolling %s%s"),
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
        UE_LOG(LogTemp, VeryVerbose, TEXT("Edge scroll blocked by UI hover"));
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

    // Apply scaling and dead zone (UE5.5 compatible clamping)
    float AxisSize = Axis.Size();
    if (AxisSize > 1.0f && AxisSize > 0.0f)
    {
        Axis = Axis * (1.0f / AxisSize);
    }
    Axis = Axis * Config->EdgeMaxSpeedScale;

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

    // UE5.5 simplified approach: Check for modal windows or active menus
    FSlateApplication& SlateApp = FSlateApplication::Get();

    // Check for modal windows (always block edge scrolling)
    if (SlateApp.GetActiveModalWindow().IsValid())
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Edge scroll blocked: Modal window active"));
        return true;
    }

    // Check if any menus are open
    if (SlateApp.AnyMenusVisible())
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Edge scroll blocked: Menu visible"));
        return true;
    }

    // Check if mouse is in edge margin area - if not, we dont care about UI detection
    const float LeftEdge = EdgeMarginPx;
    const float RightEdge = ViewportSize.X - EdgeMarginPx;
    const float TopEdge = EdgeMarginPx;
    const float BottomEdge = ViewportSize.Y - EdgeMarginPx;

    bool bInEdgeArea = (MousePos.X <= LeftEdge || MousePos.X >= RightEdge ||
                       MousePos.Y <= TopEdge || MousePos.Y >= BottomEdge);

    if (!bInEdgeArea)
    {
        return false; // Not in edge area, no need to check for UI
    }

    // For edge areas, use conservative approach - only allow if no UI interaction is happening
    return false; // Allow edge scrolling in edge areas if no modal/menu
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

    // Check if mouse is captured (UE5.5 API)
    if (SlateApp.GetMouseCaptureWindow() != nullptr)
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
        UE_LOG(LogTemp, VeryVerbose, TEXT("No context restrictions - edge scrolling allowed"));
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
        UE_LOG(LogTemp, VeryVerbose, TEXT("Edge scroll blocked: no current base context found for '%s'"), *CurrentContextName);
        return false;
    }

    // Check if current context is in the allowed list
    const bool bContextAllowed = Config->EdgeScrollAllowedContexts.Contains(CurrentBaseContext);

    UE_LOG(LogTemp, VeryVerbose, TEXT("Edge scroll context check: '%s' -> %s (IMC: %s)"),
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

    FVector2D NewViewportSize; // Non-const variable for GetViewportSize
    LP->ViewportClient->GetViewportSize(NewViewportSize);
    CachedViewportSize = NewViewportSize;
    CachedDPIScale = LP->ViewportClient->GetDPIScale();

    if (CachedViewportSize.X <= 0 || CachedViewportSize.Y <= 0)
    {
        return false;
    }

    bViewportCacheValid = true;
    UE_LOG(LogTemp, VeryVerbose, TEXT("Updated viewport cache: Size=%s, DPI=%.2f"),
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
        UE_LOG(LogTemp, Log, TEXT("EdgeScrollComponent readiness: %s"),
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
            const_cast<UPACS_EdgeScrollComponent*>(this)->CachedInputHandler = PC->GetInputHandler();
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
            const_cast<UPACS_EdgeScrollComponent*>(this)->CachedAssessorPawn = Cast<APACS_AssessorPawn>(PC->GetPawn());
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

// Viewport resize handling moved to UpdateViewportCache() method in UE5.5