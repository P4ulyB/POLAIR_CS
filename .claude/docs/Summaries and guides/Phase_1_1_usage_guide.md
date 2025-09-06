# PACS Input System - Usage Guide

## Overview

The PACS Input System provides robust, priority-based input routing with Enhanced Input context management. It supports base context switching (Game/Menu/UI) and overlay stacking for temporary UI elements with granular blocking control.

---

## Table of Contents

1. [Initial Setup](#initial-setup)
2. [Configuration](#configuration)
3. [Creating Input Receivers](#creating-input-receivers)
4. [Context Management](#context-management)
5. [Overlay System](#overlay-system)
6. [Common Usage Patterns](#common-usage-patterns)
7. [Debugging and Troubleshooting](#debugging-and-troubleshooting)
8. [Best Practices](#best-practices)

---

## Initial Setup

### 1. Project Configuration

Ensure your project has Enhanced Input enabled:

**Project Settings > Engine > Input**
- Default Player Input Class: `Enhanced Player Input`
- Default Input Component Class: `Enhanced Input Component`

### 2. PlayerController Setup

Create a Blueprint based on `APACS_PlayerController` or use it directly:

```cpp
// In your GameMode
APlayerControllerClass = APACS_PlayerController::StaticClass();
```

### 3. Required Assets

Create these assets in your Content Browser:

#### Input Actions
- `IA_MenuToggle` - Toggle between Game/Menu contexts
- `IA_Move` - Player movement (Vector2D)
- `IA_Look` - Camera look (Vector2D)
- `IA_Fire` - Primary action
- `IA_Interact` - Interaction
- `IA_CloseUI` - Close UI overlays

#### Input Mapping Contexts
- `IMC_Gameplay` - Core gameplay controls
- `IMC_Menu` - Menu navigation
- `IMC_UI` - UI-specific controls

#### Primary Data Asset
- `DA_PACSInputConfig` (Type: `PACS Input Mapping Config`)

---

## Configuration

### Setting Up the Input Config Data Asset

1. **Create Data Asset:**
   - Right-click in Content Browser
   - Miscellaneous > Data Asset
   - Select `PACS Input Mapping Config`
   - Name it `DA_PACSInputConfig`

2. **Configure Action Mappings:**
```
Input Action: IA_MenuToggle
Action Identifier: "MenuToggle"
Bind Started: true
Bind Completed: false

Input Action: IA_Move  
Action Identifier: "Move"
Bind Started: false
Bind Triggered: true

Input Action: IA_Look
Action Identifier: "Look"  
Bind Started: false
Bind Triggered: true
```

3. **Set Required Contexts:**
- Gameplay Context: `IMC_Gameplay`
- Menu Context: `IMC_Menu`
- UI Context: `IMC_UI`

4. **Configure UI Blocked Actions:**
```
- "Move"
- "Look"
- "Fire"
- "Interact"
```

5. **Assign to PlayerController:**
   - Open your PlayerController Blueprint
   - Find InputHandler component
   - Set Input Config to `DA_PACSInputConfig`

---

## Creating Input Receivers

Input receivers handle specific input events. Implement the `IPACS_InputReceiver` interface:

### C++ Example

```cpp
// MenuSystem.h
UCLASS()
class UMenuSystem : public UObject, public IPACS_InputReceiver
{
    GENERATED_BODY()

public:
    // IPACS_InputReceiver interface
    virtual EPACS_InputHandleResult HandleInputAction(
        FName ActionName, 
        const FInputActionValue& Value) override;
    
    virtual int32 GetInputPriority() const override 
    { 
        return PACS_InputPriority::Menu; 
    }

    void Initialize(UPACS_InputHandlerComponent* Handler);
    void Shutdown();

private:
    UPROPERTY()
    TObjectPtr<UPACS_InputHandlerComponent> InputHandler;
    
    bool bIsMenuOpen = false;
};

// MenuSystem.cpp
void UMenuSystem::Initialize(UPACS_InputHandlerComponent* Handler)
{
    InputHandler = Handler;
    if (InputHandler)
    {
        InputHandler->RegisterReceiver(this, PACS_InputPriority::Menu);
    }
}

void UMenuSystem::Shutdown()
{
    if (InputHandler)
    {
        InputHandler->UnregisterReceiver(this);
        InputHandler = nullptr;
    }
}

EPACS_InputHandleResult UMenuSystem::HandleInputAction(
    FName ActionName, 
    const FInputActionValue& Value)
{
    if (ActionName == "MenuToggle")
    {
        if (InputHandler)
        {
            InputHandler->ToggleMenuContext();
        }
        return EPACS_InputHandleResult::HandledConsume;
    }
    
    if (ActionName == "Move" && bIsMenuOpen)
    {
        // Handle menu navigation
        FVector2D MoveVector = Value.Get<FVector2D>();
        NavigateMenu(MoveVector);
        return EPACS_InputHandleResult::HandledConsume;
    }
    
    return EPACS_InputHandleResult::NotHandled;
}
```

### Priority Guidelines

Use these constants for consistent priority ordering:

```cpp
PACS_InputPriority::Critical   = 10000  // System-level (console)
PACS_InputPriority::UI        = 1000   // UI overlays
PACS_InputPriority::Menu      = 800    // Menu systems
PACS_InputPriority::Modal     = 600    // Modal dialogs
PACS_InputPriority::Gameplay  = 400    // Normal gameplay
PACS_InputPriority::Background = 200   // Background systems
```

---

## Context Management

The system uses a two-tier approach: **Base Contexts** and **Overlay Stack**.

### Base Context Switching

Base contexts are mutually exclusive (Game OR Menu OR UI):

```cpp
// Switch to specific context
InputHandler->SetBaseContext(EPACS_InputContextMode::Gameplay);
InputHandler->SetBaseContext(EPACS_InputContextMode::Menu);

// Toggle between Game and Menu
InputHandler->ToggleMenuContext();
```

### When to Use Base Contexts

- **Gameplay**: Normal game state
- **Menu**: Main menu, pause menu, settings
- **UI**: Full-screen UI that replaces gameplay

---

## Overlay System

Overlays are temporary contexts stacked on top of the base context. They support granular blocking control.

### Overlay Types

```cpp
EPACS_OverlayType::Blocking     // Blocks gameplay input (inventory, dialogue)
EPACS_OverlayType::NonBlocking  // Doesn't block input (HUD updates, notifications)
EPACS_OverlayType::Partial      // Blocks some input (quick menu, minimap)
```

### Using Overlays

```cpp
// Push a blocking overlay (inventory)
InputHandler->PushOverlay(InventoryContext, EPACS_OverlayType::Blocking);

// Push a non-blocking overlay (notification)
InputHandler->PushOverlay(NotificationContext, EPACS_OverlayType::NonBlocking);

// Pop the top overlay
InputHandler->PopOverlay();

// Clear all overlays
InputHandler->PopAllOverlays();

// Check if input is blocked
bool bInputBlocked = InputHandler->HasBlockingOverlay();
```

### Overlay Priority

Higher priority overlays receive input first:

```cpp
// Default UI priority
InputHandler->PushOverlay(Context, EPACS_OverlayType::Blocking, PACS_InputPriority::UI);

// Higher priority overlay
InputHandler->PushOverlay(Context, EPACS_OverlayType::Blocking, PACS_InputPriority::Critical);
```

---

## Common Usage Patterns

### 1. Inventory System

```cpp
class UInventorySystem : public UObject, public IPACS_InputReceiver
{
public:
    void OpenInventory()
    {
        // Push blocking overlay
        InputHandler->PushOverlay(InventoryContext, EPACS_OverlayType::Blocking);
        
        // Show inventory widget
        InventoryWidget->SetVisibility(ESlateVisibility::Visible);
        bIsOpen = true;
    }
    
    void CloseInventory()
    {
        // Pop overlay
        InputHandler->PopOverlay();
        
        // Hide inventory widget
        InventoryWidget->SetVisibility(ESlateVisibility::Hidden);
        bIsOpen = false;
    }
    
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override
    {
        if (ActionName == "CloseUI" && bIsOpen)
        {
            CloseInventory();
            return EPACS_InputHandleResult::HandledConsume;
        }
        
        return EPACS_InputHandleResult::NotHandled;
    }
};
```

### 2. Notification System

```cpp
class UNotificationSystem : public UObject
{
public:
    void ShowNotification(const FString& Message)
    {
        // Push non-blocking overlay
        InputHandler->PushOverlay(NotificationContext, EPACS_OverlayType::NonBlocking);
        
        // Show notification (doesn't block gameplay)
        DisplayNotification(Message);
        
        // Auto-hide after delay
        GetWorld()->GetTimerManager().SetTimer(
            HideTimer, 
            this, 
            &UNotificationSystem::HideNotification, 
            3.0f
        );
    }
    
private:
    void HideNotification()
    {
        InputHandler->PopOverlay();
        HideNotificationWidget();
    }
};
```

### 3. Player Controller Integration

```cpp
// In your custom PlayerController
class AMyPlayerController : public APACS_PlayerController
{
public:
    virtual void BeginPlay() override
    {
        Super::BeginPlay();
        
        // Initialize game systems
        InventorySystem = NewObject<UInventorySystem>(this);
        InventorySystem->Initialize(InputHandler);
        
        MenuSystem = NewObject<UMenuSystem>(this);
        MenuSystem->Initialize(InputHandler);
    }
    
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
    {
        // Clean up receivers
        if (InventorySystem) InventorySystem->Shutdown();
        if (MenuSystem) MenuSystem->Shutdown();
        
        Super::EndPlay(EndPlayReason);
    }
};
```

---

## Debugging and Troubleshooting

### Logging

Enable detailed logging in console:
```
log LogPACSInput VeryVerbose
```

### Common Issues

**Input Not Working:**
- Check Enhanced Input is enabled in project settings
- Verify InputConfig is assigned to PlayerController
- Ensure Input Actions are properly mapped in data asset
- Check action identifiers match between data asset and receivers

**Context Switching Problems:**
- Verify contexts exist and are valid
- Check if overlays are blocking intended input
- Use `HasBlockingOverlay()` to debug blocking state

**Receiver Not Getting Input:**
- Check registration priority (higher priority = earlier execution)
- Verify receiver implements `IPACS_InputReceiver` correctly
- Check if higher priority receiver is consuming input

**UI Input Bleeding Through:**
- Add action names to `UIBlockedActions` in data asset
- Use blocking overlays for UI that should block gameplay
- Check overlay type is set correctly

### Debug Commands

```cpp
// Check receiver count
int32 ReceiverCount = InputHandler->GetReceiverCount();

// Check overlay state
bool bBlocked = InputHandler->HasBlockingOverlay();
int32 OverlayCount = InputHandler->GetOverlayCount();

// Check component health
bool bHealthy = InputHandler->IsHealthy();
```

---

## Best Practices

### Receiver Management

1. **Register Early:** Register receivers in `BeginPlay()` or initialization
2. **Unregister Safely:** Always unregister in `EndPlay()` or cleanup
3. **Use Appropriate Priorities:** Follow the priority constants
4. **Handle Gracefully:** Return `NotHandled` for unrecognized actions

### Context Management

1. **Minimal Base Switching:** Use base contexts for major mode changes only
2. **Prefer Overlays:** Use overlays for temporary UI elements
3. **Match Overlay Types:** Choose blocking/non-blocking based on intended behavior
4. **Clean Stack:** Pop overlays when UI closes

### Input Action Design

1. **Consistent Naming:** Use clear, descriptive action identifiers
2. **Appropriate Events:** Bind `Started` for buttons, `Triggered` for axes
3. **Avoid Conflicts:** Don't reuse action names for different purposes
4. **Document Mappings:** Keep data asset well-organized

### Performance

1. **Limit Receivers:** Stay under 100 receivers total
2. **Efficient Handlers:** Keep `HandleInputAction()` fast
3. **Clean Up:** Remove unused receivers promptly
4. **Monitor Stack:** Keep overlay stack shallow (< 10 contexts)

### Error Handling

1. **Validate Pointers:** Check InputHandler validity before use
2. **Handle Failures:** Gracefully handle registration failures
3. **Log Issues:** Use appropriate log levels for debugging
4. **Test Edge Cases:** Test context switching during level transitions

---

## Example Project Structure

```
Content/
├── Input/
│   ├── Actions/
│   │   ├── IA_MenuToggle
│   │   ├── IA_Move
│   │   ├── IA_Look
│   │   ├── IA_Fire
│   │   └── IA_Interact
│   ├── Contexts/
│   │   ├── IMC_Gameplay
│   │   ├── IMC_Menu
│   │   └── IMC_UI
│   └── DataAssets/
│       └── DA_PACSInputConfig
├── Blueprints/
│   └── Controllers/
│       └── BP_PlayerController (based on APACS_PlayerController)
└── UI/
    ├── Menus/
    └── HUD/
```

This structure keeps all input-related assets organized and easily maintainable.

---

## Summary

The PACS Input System provides a robust foundation for complex input management. Key benefits:

- **Priority-based routing** ensures critical input is handled first
- **Context management** provides clean mode switching
- **Overlay system** enables complex UI interactions
- **Granular blocking** prevents unwanted input bleeding
- **Production-ready** with comprehensive error handling

Follow the patterns in this guide for reliable, maintainable input handling in your UE5 project.