# Selection Plane System - User Guide

## Overview

The Selection Plane System provides visual feedback for NPC selection in POLAIR_CS, designed specifically for multiplayer dedicated server environments with VR support. This system replaces the previous decal-based approach with a more performant static mesh plane solution using CustomPrimitiveData (CPD).

### Key Features
- **VR-Aware**: Selection visuals automatically hide on VR/HMD clients
- **Performance Optimized**: Uses CPD instead of Material Instance Dynamics
- **Server-Authoritative**: Selection state managed by server, visuals rendered client-side
- **Pooling Compatible**: Fully integrated with the object pooling system
- **Configurable**: Visual profiles can be customized via data assets

## Architecture Components

### 1. Selection Profile (`PACS_SelectionProfile.h`)
Defines the visual configuration for different selection states:
- **Hovered**: When cursor is over an NPC
- **Selected**: When an NPC is actively selected
- **Unavailable**: When an NPC cannot be selected
- **Available**: Default state for selectable NPCs

Each state includes:
- Color (FLinearColor)
- Brightness multiplier (float)

### 2. Selection Plane Component (`PACS_SelectionPlaneComponent.h`)
Core component that manages selection visualization:
- Automatically creates and configures the static mesh plane
- Updates material parameters using CustomPrimitiveData
- Handles VR client detection and visibility
- Integrates with object pooling lifecycle

### 3. Developer Settings (`PACS_NetPerfSettings.h`)
Project-wide configuration accessible via:
**Project Settings → PACS → NetPerf**

Key settings:
- `bDisableSelectionPlanesOnVR`: Enable/disable VR culling
- `SelectionPlaneMaxDistance`: Maximum render distance
- `SelectionPlaneScale`: Global scale multiplier

## Implementation in NPC Classes

### Base Integration
All three NPC base classes now include the SelectionPlaneComponent:

```cpp
// PACS_NPC_Base - Abstract base for all NPCs
// PACS_NPC_Base_Char - Character-based NPCs (inherits from ACharacter)
// PACS_NPC_Base_Veh - Vehicle NPCs (inherits from AWheeledVehiclePawn)
```

Each class:
1. Creates the SelectionPlaneComponent in constructor
2. Initializes it in BeginPlay
3. Updates visibility on selection state changes
4. Properly resets during pooling operations

### Selection Interface
All NPC classes expose these methods:

```cpp
// Set selection state
void SetSelected(bool bNewSelected, APlayerState* Selector = nullptr);

// Query selection state
bool IsSelected() const;
APlayerState* GetCurrentSelector() const;
```

## Material Setup Requirements

### Creating the Selection Material
You must create a material in Unreal Editor named `M_PACS_Selection`:

1. **Create Material**:
   - Right-click in Content Browser
   - Create → Material
   - Name it `M_PACS_Selection`
   - Save to: `/Content/Materials/Selection/`

2. **Material Settings**:
   - Blend Mode: `Translucent`
   - Shading Model: `Unlit`
   - Two Sided: `true`
   - Disable Depth Test: `true` (for visibility through objects)

3. **CustomPrimitiveData Setup**:
   ```hlsl
   // In Material Editor:
   // Add Custom Primitive Data node
   // Index 0-2: Color RGB
   // Index 3: Brightness

   // Example node setup:
   Color = CustomPrimitiveData(float3(0,1,2))
   Brightness = CustomPrimitiveData(3)
   FinalColor = Color * Brightness
   ```

4. **Opacity Configuration**:
   - Connect alpha channel or use constant (e.g., 0.5)
   - Consider using fresnel for edge highlighting

## Usage Workflow

### 1. Selection System Initialization
Happens automatically when NPCs spawn:
```cpp
// In BeginPlay of each NPC
SelectionPlaneComponent->InitializeSelectionPlane();
SelectionPlaneComponent->SetSelectionPlaneVisible(false); // Hidden by default
```

### 2. Runtime Selection
When player selects an NPC:
```cpp
// Server-side selection logic
if (HasAuthority())
{
    TargetNPC->SetSelected(true, PlayerState);
}
```

The component automatically:
- Updates visual state on all clients
- Hides visuals on VR clients
- Applies appropriate color/brightness

### 3. Selection States
Change selection visual state:
```cpp
// Available states from ESelectionVisualState enum
SelectionPlaneComponent->SetSelectionState(ESelectionVisualState::Hovered);
SelectionPlaneComponent->SetSelectionState(ESelectionVisualState::Selected);
SelectionPlaneComponent->SetSelectionState(ESelectionVisualState::Unavailable);
SelectionPlaneComponent->SetSelectionState(ESelectionVisualState::Available);
```

### 4. Pooling Integration
When NPCs return to pool:
```cpp
// Automatically called by pooling system
void OnReturnedToPool_Implementation()
{
    // Selection cleared
    // Visuals hidden
    // State reset
}
```

## Customization

### Creating Custom Selection Profiles

1. **Create Data Asset**:
   - Right-click in Content Browser
   - Create → Data Asset
   - Choose `PACS_SelectionProfileAsset`
   - Configure colors and brightness values

2. **Apply Profile at Runtime**:
```cpp
// In your NPC Blueprint or C++
if (UPACS_SelectionProfileAsset* ProfileAsset = LoadObject<UPACS_SelectionProfileAsset>(...))
{
    SelectionPlaneComponent->SetSelectionProfile(ProfileAsset->GetProfile());
}
```

### Per-NPC Customization
Override visual properties for specific NPCs:
```cpp
// In derived NPC class
void BeginPlay()
{
    Super::BeginPlay();

    // Custom colors for this NPC type
    FSelectionVisualProfile CustomProfile;
    CustomProfile.SelectedColor = FLinearColor::Red;
    CustomProfile.SelectedBrightness = 2.0f;

    SelectionPlaneComponent->SetSelectionProfile(CustomProfile);
}
```

## Performance Considerations

### VR Optimization
- Selection planes automatically hidden on VR clients
- No additional performance impact on Quest 3
- Server handles selection logic, clients only render

### Network Optimization
- Only selection state is replicated (1 byte enum)
- Visual updates use local CustomPrimitiveData
- No Material Instance Dynamics replication

### Rendering Optimization
- Single shared material for all NPCs
- CPD updates are GPU-efficient
- Distance culling via `SelectionPlaneMaxDistance`

## Debugging

### Console Commands
```cpp
// Toggle selection plane visibility
PACS.Selection.ForceVisible [0/1]

// Debug selection states
PACS.Selection.DebugDraw [0/1]

// Override VR detection
PACS.Selection.IgnoreVR [0/1]
```

### Common Issues

**Selection plane not visible:**
1. Check material is created and assigned
2. Verify `SetSelectionPlaneVisible(true)` is called
3. Ensure not running on VR client
4. Check `SelectionPlaneMaxDistance` setting

**Wrong colors displayed:**
1. Verify CPD indices in material match component
2. Check profile values are set correctly
3. Ensure material is using Unlit shading model

**Performance issues:**
1. Reduce `SelectionPlaneMaxDistance`
2. Simplify plane mesh (use simple quad)
3. Check material complexity

## Migration from Decal System

### Code Changes Required

1. **Remove Decal References**:
```cpp
// Old:
UDecalComponent* SelectionDecal;
SetupDefaultDecal();

// New:
UPACS_SelectionPlaneComponent* SelectionPlaneComponent;
// Setup handled automatically
```

2. **Update Selection Logic**:
```cpp
// Old:
SelectionDecal->SetVisibility(bIsSelected);

// New:
SelectionPlaneComponent->SetSelectionState(
    bIsSelected ? ESelectionVisualState::Selected
                : ESelectionVisualState::Available
);
```

3. **Blueprint Updates**:
- Remove DecalComponent references
- Update any BP nodes using old selection system
- Recompile affected Blueprints

### Data Migration
- Old decal materials can be deleted
- Selection colors should be migrated to new profiles
- Update any data tables referencing selection visuals

## Future Integration Points

### Hover Probe System
The selection plane system is designed to integrate with future hover probe implementation:

```cpp
// Future hover detection (not yet implemented)
void OnHoverProbeHit(APACS_NPC_Base* HitNPC)
{
    if (HitNPC && HitNPC->SelectionPlaneComponent)
    {
        HitNPC->SelectionPlaneComponent->SetSelectionState(
            ESelectionVisualState::Hovered
        );
    }
}
```

### Dynamic State Transitions
System supports smooth transitions between states:
```cpp
// Future enhancement for animated transitions
SelectionPlaneComponent->TransitionToState(
    ESelectionVisualState::Selected,
    TransitionDuration
);
```

## Best Practices

1. **Always Check Authority**: Selection state changes should only happen on server
2. **Use Pooling Callbacks**: Properly reset state in `OnReturnedToPool`
3. **Profile Performance**: Monitor draw calls with stat commands
4. **Test VR Early**: Verify selection planes don't appear in VR builds
5. **Customize Wisely**: Use data assets for reusable profiles

## Support and Troubleshooting

For issues or questions:
1. Check error logs for "PACS_SelectionPlane" entries
2. Verify project settings in PACS → NetPerf
3. Ensure material setup matches CPD indices
4. Test in both PIE and packaged builds
5. Verify VR detection with `stat stereo` command

## Version History

- **v1.0** - Initial implementation replacing decal system
  - Added SelectionPlaneComponent
  - Integrated with NPC base classes
  - VR client exclusion
  - CustomPrimitiveData support
  - Developer settings configuration