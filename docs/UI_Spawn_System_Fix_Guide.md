# UI Spawn System - Client-Side Fix Guide

## Issue Diagnosis

Based on the logs, the spawn UI wasn't appearing because:

1. **Wrong widget class set**: You had `Test_C` instead of `WBP_MainOverlay`
2. **SpawnOrchestrator is server-only**: Client was trying to access a server-only subsystem
3. **SpawnListWidget needed direct config**: Was trying to get config from server subsystem

---

## Fixed Architecture

The spawn UI system now works as follows:

1. **Client-side UI creation**: PlayerController creates UI widget locally
2. **Direct config reference**: SpawnListWidget uses a data asset reference (not server subsystem)
3. **Server RPCs for spawning**: When user clicks, client sends RPC to server

---

## Setup Instructions

### 1. Fix PlayerController Widget Class

**In BP_PlayerController:**
1. Open `BP_PlayerController` in the editor
2. Go to **Class Defaults** (top toolbar)
3. Find category **PACS|Spawn UI**
4. Set **Spawn UI Widget Class** to `WBP_MainOverlay` (not Test_C)
5. Compile and Save

### 2. Configure WBP_SpawnList

**In WBP_SpawnList Blueprint:**
1. Open `WBP_SpawnList` in the editor
2. Go to **Class Defaults**
3. Find category **Spawn List**
4. Set **Button Widget Class** to `WBP_SpawnButton_Base`
5. **NEW**: Set **Spawn Config Asset** to `DA_SpawnConfigd_Main` (or your spawn config)
6. Set **Filter By UI Visibility** to `true`
7. Set **Max Buttons To Display** to `20`
8. Compile and Save

### 3. Ensure WBP_MainOverlay Contains SpawnList

**In WBP_MainOverlay:**
1. Open `WBP_MainOverlay` in the editor
2. Make sure it contains `WBP_SpawnList` widget
3. Position the spawn list appropriately (e.g., top-left corner)
4. Compile and Save

### 4. Verify Spawn Config

**In DA_SpawnConfigd_Main (or your config):**
1. Open the spawn config data asset
2. For each spawn configuration:
   - Set **bVisible In UI** to `true` for NPCs you want buttons for
   - Set **Display Name** (e.g., "Civilian")
   - Set **Button Icon** (optional)
   - Set **Tooltip Description** (optional)
3. Save the asset

---

## Testing

### Console Commands

When you play in editor, check the console for these messages:

```
PACS_PlayerController::BeginPlay - Scheduling CreateSpawnUI for next tick (IsLocalController=true)
PACS_PlayerController::CreateSpawnUI - Starting UI creation process
PACS_PlayerController::CreateSpawnUI - Widget class is set: WBP_MainOverlay
PACS_PlayerController::CreateSpawnUI - Proceeding to create widget (client-side UI)
PACS_PlayerController::CreateSpawnUI - Creating widget from class: WBP_MainOverlay
PACS_PlayerController::CreateSpawnUI - SUCCESS! Spawn UI widget created and added to viewport
```

### What to Look For

1. **Widget appears**: You should see spawn buttons on screen
2. **Buttons populated**: Buttons should match your spawn config entries
3. **Click to place**: Click button → cursor changes → click ground to spawn

### Common Issues

**No widget appears:**
- Check console for error about SpawnUIWidgetClass not set
- Verify BP_PlayerController has WBP_MainOverlay set

**Widget appears but no buttons:**
- Check WBP_SpawnList has SpawnConfigAsset set
- Verify spawn config has entries with bVisibleInUI = true

**Buttons don't spawn NPCs:**
- Check server is running (not standalone mode)
- Verify spawn config actor classes are valid
- Check pool limits haven't been reached

---

## Technical Changes Made

### Removed Server Dependency
- `CreateSpawnUI()` no longer checks for SpawnOrchestrator (server-only)
- `UPACS_SpawnListWidget` now uses direct `SpawnConfigAsset` property
- Removed `GetSpawnOrchestrator()` method

### Added Client Configuration
- New `SpawnConfigAsset` property on SpawnListWidget
- Direct data asset reference instead of server subsystem lookup
- Icons and button data come from local config copy

### Enhanced Logging
- Detailed logging at each step of UI creation
- Clear error messages for missing configuration
- Success confirmation when UI is created

---

## Architecture Notes

The spawn system follows a client-server split:

**Client Side:**
- UI creation and display
- Button interaction handling
- Spawn placement mode
- Visual feedback

**Server Side:**
- SpawnOrchestrator subsystem
- Actual NPC spawning
- Pool management
- Replication to all clients

**Communication:**
- Client sends `ServerRequestSpawnNPC` RPC
- Server validates and spawns
- Server sends `ClientNotifySpawnResult` back
- NPCs replicate to all clients automatically

---

## Next Steps

1. Test single player PIE
2. Test multiplayer (2+ players)
3. Verify dedicated server spawning
4. Add spawn limit UI feedback (optional)
5. Add icon loading for buttons (optional)