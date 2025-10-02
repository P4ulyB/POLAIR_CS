# UI-Driven NPC Spawn System - Setup Guide

## System Overview
This system allows players to spawn NPCs via UI buttons, integrating with the server-authoritative object pooling system. Players click a button to select an NPC type, then click on the map to place it.

## Architecture Components

### C++ Classes Created
1. **UPACS_SpawnButtonWidget** - Base class for spawn buttons
2. **UPACS_SpawnListWidget** - Manager for button list
3. **Extended PACS_PlayerController** - Added spawn placement mode

### Key Features
- Server-authoritative spawning via `PACS_SpawnOrchestrator`
- Object pooling for optimal performance
- Input context switching (IMC_GameMode ↔ IMC_UI)
- Spawn limit enforcement (per-player and global)
- VR-aware (automatically disabled on HMD clients)

## Setup Instructions

### 1. Configure Input Mapping Context (IMC_UI)

1. Open your `IMC_UI` Input Mapping Context in the editor
2. Add these Input Actions:
   - **IA_PlaceNPC** - Mapped to Left Mouse Button
   - **IA_CancelPlacement** - Mapped to Right Mouse Button and Escape key

3. Ensure the priority is set appropriately (typically 600 for UI context)

### 2. Create Blueprint Widgets

#### A. Create WBP_SpawnButton_Base (Individual Button)

1. Create new Widget Blueprint: `Content/UI/WBP_SpawnButton_Base`
2. Set Parent Class to `UPACS_SpawnButtonWidget`
3. Add these named widgets:
   - **Button** named "SpawnButton" or "Button"
   - **TextBlock** named "ButtonText" or "DisplayName"
   - **Image** named "ButtonIcon" or "Icon" (optional)

4. In the Event Graph:
   - The button click is automatically handled by the C++ base class
   - Implement `OnDataUpdated` event to update visuals
   - Implement `OnAvailabilityChanged` event for enabled/disabled states

Example Blueprint setup:
```
Event OnDataUpdated
├─→ Set Text (ButtonText) = DisplayName
└─→ Set Brush from Texture (ButtonIcon) = ButtonIcon

Event OnAvailabilityChanged
├─→ Branch (bAvailable)
    ├─→ True: Set Button Color (White)
    └─→ False: Set Button Color (Gray)
```

#### B. Create WBP_SpawnList (Button Container)

1. Create new Widget Blueprint: `Content/UI/WBP_SpawnList`
2. Set Parent Class to `UPACS_SpawnListWidget`
3. Add a **VerticalBox** named "ButtonContainer" or "VerticalBox"
4. In Class Defaults, set:
   - **ButtonWidgetClass** = `WBP_SpawnButton_Base`
   - **bFilterByUIVisibility** = true
   - **MaxButtonsToDisplay** = 20 (or your preference)

#### C. Create WBP_MainOverlay (Main UI Container)

1. Create new Widget Blueprint: `Content/UI/WBP_MainOverlay`
2. Add your `WBP_SpawnList` widget to the overlay
3. Position it where you want the spawn buttons to appear
4. This is the widget that gets added to the viewport

### 3. Configure Spawn Data Asset

1. Open your existing `DA_SpawnConfig` data asset
2. For each spawn configuration entry, set:
   - **bVisibleInUI** = true (for buttons you want to show)
   - **DisplayName** = "Civilian NPC" (or appropriate name)
   - **ButtonIcon** = Texture2D reference (optional)
   - **TooltipDescription** = "Click to place a civilian"
   - **PlayerSpawnLimit** = 10 (or desired limit)
   - **GlobalSpawnLimit** = 100 (or desired limit)

### 4. Configure GameMode

Ensure your GameMode's BeginPlay sets the spawn config:
```cpp
void AYourGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (UPACS_SpawnOrchestrator* Orchestrator = GetWorld()->GetSubsystem<UPACS_SpawnOrchestrator>())
    {
        Orchestrator->SetSpawnConfig(YourSpawnConfigAsset);
    }
}
```

### 5. Wire Up PlayerController

The PlayerController automatically creates the UI widget on BeginPlay for local players. No additional setup needed if using default paths.

To use custom widget paths, modify in PlayerController:
```cpp
// In CreateSpawnUI() method, change the path:
LoadClass<UUserWidget>(nullptr, TEXT("/Game/Your/Custom/Path/WBP_MainOverlay"))
```

## Input Flow

1. **Player clicks spawn button** → `UPACS_SpawnButtonWidget::HandleButtonClicked()`
2. **Enter placement mode** → `APACS_PlayerController::EnterSpawnPlacementMode(SpawnTag)`
3. **Switch to IMC_UI context** → Different input mapping active
4. **Player left-clicks on map** → `HandlePlaceNPCAction()` → `ServerRequestSpawnNPC()`
5. **Server spawns NPC** → Uses `PACS_SpawnOrchestrator` pooling system
6. **Exit placement mode** → Returns to IMC_GameMode context

## Testing

### In Editor (PIE)
1. Set Number of Players to 2+ for multiplayer testing
2. Set Net Mode to "Play As Listen Server"
3. Run PIE
4. Click spawn buttons and verify NPCs spawn at click locations
5. Test spawn limits work correctly

### Dedicated Server Testing
1. Build server: `Build.bat POLAIR_CSServer Win64 Development`
2. Launch server with spawn config
3. Connect clients
4. Verify spawning only happens on server
5. Check all clients see spawned NPCs

## Common Issues & Solutions

### Buttons Not Appearing
- Check `bVisibleInUI` is true in SpawnConfig
- Verify ButtonWidgetClass is set in WBP_SpawnList
- Ensure SpawnConfig is set in GameMode

### Click Not Spawning NPCs
- Verify IMC_UI has "IA_PlaceNPC" action mapped
- Check console for authority errors
- Ensure SpawnOrchestrator is initialized

### NPCs Not Using Pooling
- Verify ActorClass implements `IPACS_Poolable` interface
- Check pool settings in SpawnConfig
- Monitor with `pacs.ValidatePooling` console command

## Performance Notes

- Buttons load icons asynchronously to prevent hitches
- Widget only created on clients (never on dedicated server)
- Spawn pooling reuses NPCs for optimal performance
- Network traffic: Single RPC per spawn (Tag + Location)

## Multiplayer Considerations

- All spawning happens on server via `HasAuthority()` checks
- Spawn limits enforced server-side
- NPCs replicated to all clients automatically
- UI state is client-local (not replicated)

## VR Support

The system automatically disables UI widgets for VR players (HMD detected). For VR spawn support, implement laser pointer spawning in the VR pawn class.

## Console Commands

- `pacs.ValidatePooling` - Check pool status
- `pacs.MeasurePerformance` - Monitor spawn performance
- `ShowDebug SpawnSystem` - Display spawn debug info

## Code Integration Points

Key files modified/created:
- `/Source/POLAIR_CS/Public/UI/PACS_SpawnButtonWidget.h`
- `/Source/POLAIR_CS/Public/UI/PACS_SpawnListWidget.h`
- `/Source/POLAIR_CS/Public/Core/PACS_PlayerController.h` (extended)
- `/Source/POLAIR_CS/Private/Core/PACS_PlayerController.cpp` (extended)

The system integrates with existing:
- `UPACS_SpawnOrchestrator` - Pooling system
- `UPACS_InputHandlerComponent` - Context switching
- `UPACS_NPCBehaviorComponent` - NPC control