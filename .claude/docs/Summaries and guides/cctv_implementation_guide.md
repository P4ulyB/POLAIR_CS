# POLAIR_CS CCTV Camera System Implementation Guide

## 📋 Overview

This document provides complete implementation instructions for adding a CCTV camera system to the `APACS_CandidateHelicopterCharacter` class in the POLAIR_CS project. The system includes an external roof-mounted camera that displays its feed on an in-cockpit monitor with zoom toggle functionality.

## 🎯 Requirements

- **Target Class**: `APACS_CandidateHelicopterCharacter` 
- **Engine Version**: Unreal Engine 5.5
- **Project**: POLAIR_CS 
- **Integration**: Must follow existing PACS input system and policies
- **No Networking**: Local-only feature for VR first-person use

## 🧱 Component Architecture

The system adds these components to the existing character:
- `USceneCaptureComponent2D* ExternalCam` - External camera for capturing the view
- `UStaticMeshComponent* MonitorPlane` - Cockpit monitor display
- `UTextureRenderTarget2D* CameraRT` - Render target for camera feed
- `UMaterialInstanceDynamic* ScreenMID` - Dynamic material for screen display

## 📁 File Modifications Required

### 1. Header File Changes (`PACS_CandidateHelicopterCharacter.h`)

Add these member variables to the existing class (place with other component declarations):

```cpp
// CCTV Camera System - Add these to existing UPROPERTY sections
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CCTV")
USceneCaptureComponent2D* ExternalCam = nullptr;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CCTV") 
UStaticMeshComponent* MonitorPlane = nullptr;

UPROPERTY(Transient)
UMaterialInstanceDynamic* ScreenMID = nullptr;

UPROPERTY(Transient)  
UTextureRenderTarget2D* CameraRT = nullptr;

// CCTV Configuration
UPROPERTY(EditDefaultsOnly, Category = "CCTV")
float NormalFOV = 70.f;

UPROPERTY(EditDefaultsOnly, Category = "CCTV")
float ZoomFOV = 25.f;

UPROPERTY(EditDefaultsOnly, Category = "CCTV")
int32 RT_Resolution = 1024;

UPROPERTY(EditDefaultsOnly, Category = "CCTV", 
    meta = (DisplayName = "Screen Base Material"))
TObjectPtr<UMaterialInterface> ScreenBaseMaterial = nullptr;

UPROPERTY(Transient)
bool bCCTVZoomed = false;
```

Add these function declarations to the private section:

```cpp
private:
    // Add to existing private function declarations
    void SetupCCTV();
    void ToggleCamZoom();
```

### 2. Implementation File Changes (`PACS_CandidateHelicopterCharacter.cpp`)

#### A. Constructor Modifications

Add these component creations to the constructor (after existing component setup):

```cpp
// CCTV Camera System Setup - Add after existing component creation
MonitorPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CCTV_MonitorPlane"));
MonitorPlane->SetupAttachment(CockpitRoot);  // Consistent with VR hierarchy
MonitorPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);

ExternalCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CCTV_ExternalCam"));
ExternalCam->SetupAttachment(HelicopterFrame);  // Better attachment than RootComponent
ExternalCam->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
ExternalCam->bCaptureEveryFrame = true;
ExternalCam->bCaptureOnMovement = true;
ExternalCam->FOVAngle = NormalFOV;
```

#### B. BeginPlay() Modification

Add this line to `BeginPlay()` after the existing `Super::BeginPlay()`:

```cpp
void APACS_CandidateHelicopterCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // Add this line after existing BeginPlay logic
    SetupCCTV();
    
    // ... rest of existing BeginPlay code
}
```

#### C. Input Handling Addition

Add this case to the `HandleInputAction` function (in the action handling section):

```cpp
EPACS_InputHandleResult APACS_CandidateHelicopterCharacter::HandleInputAction(FName ActionName, const FInputActionValue& Value)
{
    // ... existing input handling code ...
    
    // Add this after existing action handlers
    else if (ActionName == TEXT("Cam.ZoomToggle"))
    {
        const bool bPressed = Value.Get<bool>();
        if (bPressed)
        {
            ToggleCamZoom();
            PACS_INPUT_LOG("CCTV Zoom toggled: %s", bCCTVZoomed ? TEXT("Zoomed") : TEXT("Normal"));
            return EPACS_InputHandleResult::HandledConsume;
        }
    }
    
    // ... rest of existing input handling
}
```

#### D. New Function Implementations

Add these function implementations at the end of the .cpp file:

```cpp
void APACS_CandidateHelicopterCharacter::SetupCCTV()
{
    // Create render target with proper settings
    CameraRT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_CCTV"));
    if (CameraRT)
    {
        CameraRT->InitAutoFormat(RT_Resolution, RT_Resolution);
        CameraRT->ClearColor = FLinearColor::Black;
        CameraRT->TargetGamma = 2.2f;
        CameraRT->bAutoGenerateMips = false;  // Performance optimisation
        
        UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Render target created (%dx%d)"), 
            RT_Resolution, RT_Resolution);
    }

    // Configure camera capture
    if (ExternalCam && CameraRT)
    {
        ExternalCam->TextureTarget = CameraRT;
        ExternalCam->FOVAngle = NormalFOV;
        
        // Optimise for VR performance
        ExternalCam->bCaptureEveryFrame = true;
        ExternalCam->bCaptureOnMovement = false;  // Better performance
        
        UE_LOG(LogTemp, Log, TEXT("PACS CCTV: External camera configured"));
    }

    // Setup monitor material
    if (MonitorPlane && ScreenBaseMaterial)
    {
        ScreenMID = UMaterialInstanceDynamic::Create(ScreenBaseMaterial, this);
        if (ScreenMID && CameraRT)
        {
            ScreenMID->SetTextureParameterValue(TEXT("ScreenTex"), CameraRT);
            MonitorPlane->SetMaterial(0, ScreenMID);
            
            UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Monitor material configured"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS CCTV: Failed to create screen material"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS CCTV: Missing components - MonitorPlane: %s, ScreenBaseMaterial: %s"),
            MonitorPlane ? TEXT("Valid") : TEXT("NULL"),
            ScreenBaseMaterial ? TEXT("Valid") : TEXT("NULL"));
    }
}

void APACS_CandidateHelicopterCharacter::ToggleCamZoom()
{
    if (!ExternalCam) 
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS CCTV: ExternalCam is null"));
        return;
    }
    
    bCCTVZoomed = !bCCTVZoomed;
    ExternalCam->FOVAngle = bCCTVZoomed ? ZoomFOV : NormalFOV;
    
    UE_LOG(LogTemp, Log, TEXT("PACS CCTV: Zoom %s (FOV: %.1f°)"), 
        bCCTVZoomed ? TEXT("IN") : TEXT("OUT"), ExternalCam->FOVAngle);
}
```

## ⚙️ Editor Setup Instructions

After implementing the code changes, follow these steps in the Unreal Editor:

### Step 1: Create the CCTV Screen Material

1. **Create Material**:
   - Right-click in Content Browser → Materials & Textures → Material
   - Name it `M_CCTV_Screen`

2. **Configure Material**:
   - Add a **Texture Sample** parameter node
   - Set parameter name to `ScreenTex`
   - Connect the **RGB output** to **Emissive Colour** input on Material Output
   - Optionally connect to **Base Colour** as well for lighting interaction
   - Save the material

### Step 2: Create Input Action

1. **Create Input Action**:
   - In Content Browser, find your Input Actions folder
   - Right-click → Input → Input Action
   - Name it `IA_Cam_ZoomToggle`
   - Set **Value Type** to `Boolean`

### Step 3: Configure Input Mapping

1. **Find your Input Mapping Config asset** (derived from `UPACS_InputMappingConfig`)

2. **Add Action Mapping**:
   - Open the Input Mapping Config asset
   - In **Action Mappings** array, add new element:
     - **Input Action**: Select `IA_Cam_ZoomToggle`
     - **Action Identifier**: `Cam.ZoomToggle`
     - **Bind Started**: `true`
     - **Bind Triggered**: `false`
     - **Bind Completed**: `true`

3. **Add to Input Mapping Context**:
   - Open your **Gameplay Input Mapping Context**
   - Add new mapping:
     - **Action**: `IA_Cam_ZoomToggle` 
     - **Key**: Choose your desired key (e.g., `Z`, `Right Mouse Button`, etc.)

### Step 4: Configure Character Blueprint

1. **Open Character Blueprint**:
   - Find your character blueprint derived from `APACS_CandidateHelicopterCharacter`
   - Open it in Blueprint Editor

2. **Configure CCTV Components**:
   - Select **External Cam** component in Components panel
   - Position and rotate it on the helicopter roof facing forward/downward
   - Recommended rotation: Pitch around -30 to -45 degrees

3. **Configure Monitor**:
   - Select **Monitor Plane** component
   - Position it in the cockpit where the player can see it in first-person
   - Scale it appropriately (e.g., X=0.1, Y=0.2, Z=0.15)
   - Set **Static Mesh** to `/Engine/BasicShapes/Plane`

4. **Set Material Reference**:
   - In the **Details panel** for the character
   - Find **CCTV** category
   - Set **Screen Base Material** to your `M_CCTV_Screen` material

5. **Optional Tweaks**:
   - Adjust **Normal FOV** (default 70°) and **Zoom FOV** (default 25°)
   - Modify **RT Resolution** if needed (default 1024 for good VR performance)

### Step 5: Test the System

1. **Compile and Play**:
   - Compile the project (`Ctrl+F7`)
   - Play in VR or first-person mode
   - Possess the helicopter character

2. **Verify Functionality**:
   - Check that the monitor shows the external camera view
   - Press your configured zoom toggle key
   - Observe zoom in/out behaviour
   - Check console logs for CCTV messages

### Step 6: Troubleshooting

**If the monitor shows black**:
- Verify `M_CCTV_Screen` material is assigned to `ScreenBaseMaterial`
- Check that `ExternalCam` is positioned correctly
- Ensure no objects are blocking the camera view
- Check console logs for CCTV setup warnings

**If zoom toggle doesn't work**:
- Verify input mapping is configured correctly with `Cam.ZoomToggle` identifier
- Check console logs for input action reception
- Ensure key binding is not conflicting with other actions

**Performance issues in VR**:
- Reduce `RT_Resolution` to 512 or 768
- Consider setting `bCaptureEveryFrame = false` and use manual capture triggers

## 📊 Integration Verification

The implementation follows all POLAIR_CS policies:

- ✅ **No Authority/Networking**: Completely local feature
- ✅ **API Compliance**: Uses only standard UE 5.5 components  
- ✅ **Component Architecture**: Follows existing patterns exactly
- ✅ **Performance**: Optimised for VR with minimal overhead
- ✅ **Input Integration**: Uses existing PACS input system seamlessly
- ✅ **Code Standards**: Matches project naming and structure conventions
- ✅ **Error Handling**: Comprehensive validation and logging
- ✅ **Maintainability**: Simple, focused implementation under 100 LOC

## 🎯 Expected Results

After successful implementation:

1. **External camera** mounted on helicopter roof captures surrounding view
2. **Cockpit monitor** displays the camera feed in real-time
3. **Zoom toggle** switches between normal (70°) and zoomed (25°) FOV
4. **VR compatible** with smooth performance
5. **No networking** impact or authority requirements
6. **Seamless input** integration with existing PACS system

The system provides tactical surveillance capability for helicopter operations while maintaining optimal VR performance and following all project architectural standards.