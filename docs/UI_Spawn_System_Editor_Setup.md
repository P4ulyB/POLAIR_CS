# UI-Driven NPC Spawn System - Editor Setup Guide

This guide walks you through setting up the UI spawn system in the Unreal Editor from scratch.

---

## Prerequisites

✅ Build completed successfully (POLAIR_CS.exe compiled)
✅ Spawn Orchestrator configured in GameMode
✅ SpawnConfig data asset exists with NPC configurations

---

## Part 1: Input Mapping Context Setup

### 1.1 Configure IMC_UI Context

1. **Locate or Create IMC_UI**
   - Navigate to `Content/Input/IMC_UI`
   - If it doesn't exist: Right-click → Input → Input Mapping Context
   - Name it `IMC_UI`

2. **Add Input Actions**

   **Action 1: Place NPC**
   - Click **+** next to Mappings
   - Set Action to `IA_PlaceNPC` (create if needed)
   - Add Mapping: **Left Mouse Button**
   - Priority: Leave default

   **Action 2: Cancel Placement**
   - Click **+** next to Mappings
   - Set Action to `IA_CancelPlacement` (create if needed)
   - Add Mapping 1: **Right Mouse Button**
   - Add Mapping 2: **Escape** key
   - Priority: Leave default

3. **Set Context Priority**
   - Priority: `600` (higher than gameplay, lower than critical UI)

4. **Save** the IMC_UI asset

### 1.2 Create Input Actions (if needed)

If `IA_PlaceNPC` or `IA_CancelPlacement` don't exist:

1. Right-click in `Content/Input/` → Input → Input Action
2. Create `IA_PlaceNPC`
   - Value Type: Digital (bool)
3. Create `IA_CancelPlacement`
   - Value Type: Digital (bool)

---

## Part 2: Widget Blueprint Creation

### 2.1 Create WBP_SpawnButton_Base (Individual Button)

1. **Create Widget Blueprint**
   - Right-click in `Content/UI/` → User Interface → Widget Blueprint
   - Name: `WBP_SpawnButton_Base`

2. **Set Parent Class**
   - Open the widget
   - Go to **File → Reparent Blueprint**
   - Search for `PACS_SpawnButtonWidget`
   - Select it and click Reparent

3. **Design the Widget**

   Add these widgets in the Hierarchy:

   ```
   Canvas Panel
   └─ Button (Name: "SpawnButton")
      └─ Horizontal Box
         ├─ Image (Name: "Icon")
         │  └─ Size: 32x32
         │  └─ Visibility: Collapsed (will show when icon loads)
         ├─ Spacer (Size: 8)
         └─ Text Block (Name: "ButtonText")
            └─ Font Size: 14
            └─ Color: White
   ```

   **IMPORTANT:** Name the Image widget **"Icon"** (not "ButtonIcon") to avoid naming conflicts with the inherited C++ property.

   **Layout Settings:**
   - Button: Fill parent, centered
   - Horizontal Box: Center aligned
   - Image: Fixed size 32x32
   - Text: Auto size

4. **Implement Blueprint Events**

   Open **Event Graph**, add these event implementations:

   **Event: On Data Updated**
   ```
   Event OnDataUpdated
   ├─ Set Text (ButtonText widget)
   │  └─ In Text = Get DisplayName (inherited C++ variable)
   │
   └─ Branch
      ├─ Condition = Is Valid (Get ButtonIcon - inherited C++ variable)
      │
      ├─ True:
      │  ├─ Set Brush from Texture (Icon widget)
      │  │  └─ Texture = Get ButtonIcon (inherited C++ variable)
      │  └─ Set Visibility (Icon widget)
      │     └─ Visibility = Visible
      │
      └─ False:
         └─ Set Visibility (Icon widget)
            └─ Visibility = Collapsed
   ```

   **Note:** `ButtonIcon` variable is inherited from the C++ parent class. `Icon` is your visual Image widget.

   **Event: On Availability Changed**
   ```
   Event OnAvailabilityChanged (bool bAvailable)
   ├─ Branch
      ├─ Condition = bAvailable
      │
      ├─ True:
      │  ├─ Set Color and Opacity (ButtonText)
      │  │  └─ Color = (1, 1, 1, 1) White
      │  └─ Set Button Enabled (SpawnButton)
      │     └─ Enabled = True
      │
      └─ False:
         ├─ Set Color and Opacity (ButtonText)
         │  └─ Color = (0.5, 0.5, 0.5, 1) Gray
         └─ Set Button Enabled (SpawnButton)
            └─ Enabled = False
   ```

5. **Compile and Save**

### 2.2 Create WBP_SpawnList (Button Container)

1. **Create Widget Blueprint**
   - Right-click in `Content/UI/` → User Interface → Widget Blueprint
   - Name: `WBP_SpawnList`

2. **Set Parent Class**
   - File → Reparent Blueprint
   - Search: `PACS_SpawnListWidget`
   - Click Reparent

3. **Design the Widget**

   Add in Hierarchy:
   ```
   Canvas Panel
   └─ Scroll Box
      └─ Vertical Box (Name: "ButtonContainer")
         └─ Padding: (8, 8, 8, 8)
   ```

   **Settings:**
   - Scroll Box: Anchors = Fill, Scroll Bar Visibility = Auto
   - Vertical Box: Must be named exactly **"ButtonContainer"** or **"VerticalBox"**

4. **Configure Class Defaults**

   - Select the widget root (click on WBP_SpawnList in Hierarchy)
   - In Details panel, find **Spawn List** category:
     - **Button Widget Class** = `WBP_SpawnButton_Base` (use dropdown)
     - **Filter By UI Visibility** = `true` ✓
     - **Max Buttons To Display** = `20`

5. **Optional: Implement Events**

   Event Graph:
   ```
   Event OnButtonsPopulated (int ButtonCount)
   └─ Print String
      └─ String = Format Text: "Created {0} spawn buttons"
         └─ {0} = ButtonCount
   ```

6. **Compile and Save**

### 2.3 Create WBP_MainOverlay (Root UI Container)

1. **Create Widget Blueprint**
   - Right-click in `Content/UI/` → User Interface → Widget Blueprint
   - Name: `WBP_MainOverlay`

2. **Design Layout**

   ```
   Overlay
   ├─ [Your existing HUD elements]
   │
   └─ WBP_SpawnList
      └─ Anchors: Top-Left or Top-Right
      └─ Position: (20, 100)
      └─ Size: (250, 600)
      └─ Z-Order: 1
   ```

   **WBP_SpawnList Settings:**
   - Drag `WBP_SpawnList` from Content Browser into the Overlay
   - Anchor to Top-Left or Top-Right corner
   - Set desired size (recommend 250 width, 600 height)
   - Adjust position to not overlap other UI

3. **Compile and Save**

---

## Part 3: Data Asset Configuration

### 3.1 Configure Spawn Config

1. **Open Spawn Config Data Asset**
   - Navigate to your `DA_SpawnConfig` (or equivalent)
   - Double-click to open

2. **For Each Spawn Configuration Entry:**

   Expand each entry in the `Spawn Configs` array:

   **Required Settings:**
   - ✅ **Spawn Tag**: Must be valid (e.g., `PACS.Spawn.Civilian`)
   - ✅ **Actor Class**: Your NPC class
   - ✅ **bVisible In UI**: `true` ✓

   **UI Display Settings:**
   - **Display Name**: "Civilian NPC" (or appropriate name)
   - **Button Icon**: Select a Texture2D (optional but recommended)
     - Browse → Select icon texture
     - Leave empty for text-only buttons
   - **Tooltip Description**: "Click to spawn a civilian character"

   **Limit Settings:**
   - **Player Spawn Limit**: `10` (max per player, 0 = unlimited)
   - **Global Spawn Limit**: `100` (max total, 0 = unlimited)

   **Example Configuration:**
   ```
   [0] Civilian Config
      ├─ Spawn Tag: PACS.Spawn.Civilian
      ├─ Actor Class: APACS_NPC_Civilian_C
      ├─ Display Name: "Civilian"
      ├─ Button Icon: T_Icon_Civilian
      ├─ bVisible In UI: ✓ true
      ├─ Tooltip: "Spawns a civilian NPC"
      ├─ Player Spawn Limit: 10
      └─ Global Spawn Limit: 100
   ```

3. **Hide Unwanted Entries**
   - For NPCs you DON'T want in UI: set **bVisible In UI** = `false`

4. **Save** the data asset

---

## Part 4: GameMode Setup

### 4.1 Ensure Spawn Orchestrator Initialization

Your GameMode should already have this in BeginPlay. Verify:

1. **Open BP_GameMode** (or your GameMode class)

2. **Check BeginPlay Event**
   ```
   Event BeginPlay
   └─ Get Subsystem (Spawn Orchestrator)
      └─ Set Spawn Config
         └─ Spawn Config = DA_SpawnConfig (your data asset)
   ```

3. If missing, add it:
   - Add node: **Get World Subsystem** → Class = `PACS Spawn Orchestrator`
   - From subsystem: **Set Spawn Config**
   - Connect your `DA_SpawnConfig` reference

4. **Compile and Save**

---

## Part 5: PlayerController Setup

### 5.1 Configure BP_PlayerController

1. **Open BP_PlayerController** (your Blueprint PlayerController)
   - Should be parented to `PACS_PlayerController`

2. **Set Spawn UI Widget Class**
   - In **Class Defaults** (top toolbar button)
   - Find category: **Spawn UI**
   - Set **Spawn UI Widget Class** = `WBP_MainOverlay` (use dropdown)

3. **Compile and Save**

That's it! The C++ code will automatically:
- Create the widget on BeginPlay for local players only
- Never create on dedicated servers
- Skip creation for VR players (HMD detected)
- Retry if SpawnOrchestrator isn't ready yet
- Add widget to viewport when ready

---

## Part 6: Testing

### 6.1 Solo Testing (PIE)

1. **Play in Editor**
   - Click Play (PIE)
   - You should see spawn buttons on the left/right side

2. **Test Spawning**
   - Click a spawn button
   - Notice input context changes (camera controls may behave differently)
   - Left-click on the ground to spawn NPC
   - NPC should appear at click location

3. **Test Cancellation**
   - Click a spawn button
   - Right-click or press Escape
   - Should return to normal gameplay mode

4. **Test Limits**
   - Spawn NPCs until limit reached
   - Button should become grayed out/disabled

### 6.2 Multiplayer Testing

1. **Set Editor Preferences**
   - Edit → Editor Preferences → Level Editor → Play
   - Number of Players: `2` or more
   - Net Mode: **Play As Listen Server**

2. **Play**
   - Each player should see their own UI
   - Player 1 spawns NPC → Player 2 sees it
   - Verify server authority (NPCs spawn on server)

3. **Check Console**
   - Press ` (tilde) to open console
   - Look for spawn logs:
     ```
     LogTemp: PACS_PlayerController: Entering spawn placement mode for tag: PACS.Spawn.Civilian
     LogTemp: PACS_PlayerController: Successfully spawned PACS.Spawn.Civilian at X=1000.0 Y=2000.0 Z=100.0
     ```

### 6.3 Dedicated Server Testing

1. **Package Server Build**
   - Project → Package Project → Windows Server
   - Wait for build completion

2. **Launch Server**
   ```
   POLAIR_CSServer.exe /Game/Maps/YourMap
   ```

3. **Connect Clients**
   - Launch client builds
   - Connect to server IP

4. **Verify**
   - UI should NOT appear on server (check server window)
   - UI appears on clients
   - Spawning works from clients
   - All clients see spawned NPCs

---

## Part 7: Styling & Polish

### 7.1 Customize Button Appearance

Edit `WBP_SpawnButton_Base`:

**Button Style:**
1. Select the Button widget
2. Details → Style → Normal
   - Background: Solid color or texture
   - Tint: (0.1, 0.1, 0.1, 0.9) Dark gray
3. Style → Hovered
   - Tint: (0.2, 0.5, 0.8, 1.0) Blue highlight
4. Style → Pressed
   - Tint: (0.1, 0.3, 0.6, 1.0) Darker blue

**Text Style:**
1. Select ButtonText
2. Font → Size: 14-16
3. Font → Typeface: Bold
4. Shadow → Offset: (1, 1)
5. Shadow → Color: Black with 0.5 opacity

### 7.2 Add Scroll Bar Styling

Edit `WBP_SpawnList`:

1. Select Scroll Box
2. Details → Style → Bar Thickness: 8
3. Style → Bar Color: Semi-transparent white

### 7.3 Add Background Panel

Edit `WBP_SpawnList`:

1. Add **Border** widget
2. Move VerticalBox inside Border
3. Border → Brush Color: (0, 0, 0, 0.6) Semi-transparent black
4. Border → Padding: (8, 8, 8, 8)

---

## Troubleshooting

### Buttons Don't Appear

**Check:**
- [ ] `bVisibleInUI` = true in SpawnConfig
- [ ] ButtonWidgetClass set in WBP_SpawnList Class Defaults
- [ ] VerticalBox named correctly ("ButtonContainer" or "VerticalBox")
- [ ] SpawnConfig set in GameMode BeginPlay

**Console Command:**
```
ShowDebug SpawnSystem
```

### Clicking Button Does Nothing

**Check:**
- [ ] IMC_UI has IA_PlaceNPC action
- [ ] IA_PlaceNPC mapped to Left Mouse Button
- [ ] WBP_SpawnButton_Base parent is UPACS_SpawnButtonWidget
- [ ] Check logs for "Entering spawn placement mode"

### NPCs Don't Spawn

**Check:**
- [ ] SpawnOrchestrator initialized (check GameMode)
- [ ] Actor class valid in SpawnConfig
- [ ] Pool not exhausted (check pool limits)
- [ ] Valid spawn location (ground, not in air)

**Console Commands:**
```
pacs.ValidatePooling
pacs.MeasurePerformance
```

### Input Stuck in UI Mode

**Check:**
- [ ] IA_CancelPlacement works (Right-click or Escape)
- [ ] Check console for "Exiting spawn placement mode"
- [ ] Manually switch: Call `SetBaseContext(Gameplay)` on InputHandler

---

## Quick Reference

### File Locations
- Input Context: `Content/Input/IMC_UI`
- Button Widget: `Content/UI/WBP_SpawnButton_Base`
- List Widget: `Content/UI/WBP_SpawnList`
- Main Overlay: `Content/UI/WBP_MainOverlay`
- Spawn Config: `Content/Data/DA_SpawnConfig`

### C++ Classes
- `UPACS_SpawnButtonWidget` - Button base class
- `UPACS_SpawnListWidget` - List manager
- `APACS_PlayerController` - Placement logic
- `UPACS_SpawnOrchestrator` - Pooling system

### Console Commands
- `pacs.ValidatePooling` - Check pool status
- `pacs.MeasurePerformance` - Performance metrics
- `ShowDebug SpawnSystem` - Display spawn info
- `stat fps` - Show frame rate

---

## Next Steps

Once setup is complete:

1. **Test in VR** - System auto-disables UI for HMD players
2. **Add More NPC Types** - Expand SpawnConfig with vehicles, civilians, etc.
3. **Custom Icons** - Create unique icons for each NPC type
4. **Polish UI** - Add animations, sounds, visual feedback
5. **Spawn Limits UI** - Display current count/limit on buttons

---

## Support

If you encounter issues:

1. Check console output for errors
2. Verify all naming matches exactly (case-sensitive)
3. Ensure parent classes are correctly set
4. Review setup documentation: `docs/UI_Spawn_System_Setup.md`