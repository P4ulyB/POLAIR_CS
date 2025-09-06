# PACS Debug Info Component

## Overview
The `UPACS_DebugInfoComponent` is a simple actor component that displays persistent debug information on screen using UE5's built-in debug text rendering. No widgets or complex UI systems - just clean, efficient onscreen text overlay.

## Features
- **Persistent Display**: Shows debug text overlay in top-left corner of viewport
- **Auto-Update**: Refreshes every 0.5 seconds with latest values
- **Toggle Visibility**: Press F1 to show/hide the debug display
- **Performance Optimized**: Only updates when values change, minimal overhead
- **Colour Coded**: Visual feedback for connection status and ping quality

## Information Displayed
1. **Server Connection**: IP address and port from command line or current connection
2. **Username**: PlayFab display name from authenticated player state
3. **Ping**: Current latency in milliseconds with colour coding:
   - Green: < 80ms (Good)
   - Yellow: 80-150ms (Fair)
   - Red: > 150ms (Poor)
4. **Connection Status**: Real-time network state (Connected, Authenticated, etc.)

## Integration

### Automatic Setup (Recommended)
The component is automatically added to `APACS_AuthenticatedPlayerController` for local players:

```cpp
// In PACS_AuthenticatedPlayerController::BeginPlay()
if (IsLocalController())
{
    UPACS_DebugInfoComponent* DebugComponent = NewObject<UPACS_DebugInfoComponent>(this, TEXT("DebugInfoComponent"));
    if (DebugComponent)
    {
        DebugComponent->RegisterComponent();
    }
}
```

### Manual Setup
To add to any PlayerController:

```cpp
// In your PlayerController's BeginPlay
UPACS_DebugInfoComponent* DebugComponent = NewObject<UPACS_DebugInfoComponent>(this);
DebugComponent->RegisterComponent();

// Optional: Start with debug display visible
DebugComponent->SetDebugDisplayVisible(true);
```

### Blueprint Setup
1. Add `PACS_DebugInfoComponent` to your PlayerController Blueprint
2. Configure properties in Details panel:
   - Update Interval (default: 0.5 seconds)
   - Display Offset (default: 50, 50)
   - Text Scale (default: 1.0)
   - Colour settings for different states

## Usage

### Controls
- **F1 Key**: Toggle debug display on/off

### Blueprint Functions
```cpp
// Toggle visibility
ToggleDebugDisplay();

// Set visibility explicitly
SetDebugDisplayVisible(bool bNewVisible);

// Check if currently visible
bool IsDebugDisplayVisible();

// Force immediate update
UpdateDebugDisplay();
```

## Data Sources
The component automatically pulls data from:
- `UPACS_CommandLineParserSubsystem` - Server IP and port
- `APACS_AuthenticatedPlayerState` - Username and authentication status
- `APlayerState` - Ping/latency information
- `UNetDriver` - Connection status

## Performance Considerations
- Component only ticks when visible
- Caches values to avoid redundant updates
- Uses lightweight debug text rendering (no widgets)
- Timer-based updates reduce per-frame overhead

## Customization

### Changing Update Rate
```cpp
// In constructor or Blueprint
UpdateInterval = 1.0f; // Update every second instead of 0.5
```

### Adjusting Display Position
```cpp
// In constructor or Blueprint
DisplayOffset = FVector2D(100.0f, 100.0f); // Move further from corner
```

### Custom Colours
```cpp
// In constructor or Blueprint
NormalTextColour = FColor::Cyan;
WarningTextColour = FColor::Orange;
ErrorTextColour = FColor::Magenta;
```

## Troubleshooting

### Debug Info Not Showing
1. Press F1 to toggle visibility
2. Ensure component is attached to local PlayerController
3. Check logs for "Debug info component added" message

### Values Show "N/A" or Empty
- **Server IP**: Requires connection or command line args
- **Username**: Requires authenticated player state
- **Ping**: Requires active network connection

### Input Not Working
- Ensure PlayerController has input enabled
- Check that F1 key isn't bound elsewhere
- Verify component owner is PlayerController

## Example Output
```
=== PACS Debug Info ===
Server: 192.168.1.100:7777
User: PlayerName123
Ping: 45 ms
Status: Connected (Authenticated)
```

## Notes
- Australian English spelling used throughout (colour, behaviour, centre)
- Component follows PACS naming conventions
- Integrates seamlessly with existing PACS authentication system
- No external dependencies beyond core UE5 modules