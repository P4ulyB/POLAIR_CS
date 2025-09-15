
# Phase 1.4 — IMPLEMENTATION BRIEF — `APACS_AssessorPawn` (UE 5.5, PACS Input)

**Date:** 15/09/2025  
**Author:** Pauly + ChatGPT  
**Audience:** Claude Code (**follow these instructions exactly**).  
**Goal:** Implement a **minimal, non‑replicated Assessor Pawn** that provides client‑side navigation only: planar WASD movement, mouse‑wheel zoom, and a Spring Arm with slight camera lag. **No rotation, no selection, no commands** yet. Selection/commands will live on PlayerController components later.

---

## 0) Ground Rules (read first)
- **Do not add features** beyond what is explicitly listed here.
- **Adhere to Unreal Engine 5.5 coding style** (minimal includes, forward declarations, UE naming).
- **Client‑only camera/navigation**. Do **not** replicate actor or movement.
- Input is provided via Pauly’s **PACS** input router — this pawn implements `IPACS_InputReceiver` and consumes only three actions (`Assessor.MoveForward`, `Assessor.MoveRight`, `Assessor.Zoom`).
- Keep the pawn a **navigation vessel**. It must not reference selection, UI, RPCs, or server code.
- All code runs on the **game thread** only; no async work is required.

---

## 1) File Layout (create exactly these files)

```
Source/YourGame/Data/Configs/AssessorPawnConfig.h
Source/YourGame/Data/Configs/AssessorPawnConfig.cpp

Source/YourGame/Pawns/Assessor/PACS_AssessorPawn.h
Source/YourGame/Pawns/Assessor/PACS_AssessorPawn.cpp
```

> Replace `YourGame` with the primary C++ module name if different. Keep folder names as shown to preserve project organisation.

---

## 2) Data Asset — `UAssessorPawnConfig`

**Purpose:** Data‑drive camera tilt, zoom bounds/steps, movement speed, and spring arm lag — one cohesive asset (avoid asset sprawl).  
**Design Note:** We treat “zoom” as **Spring Arm length** changes. This gives stable feel and avoids moving the pawn in Z.

**`AssessorPawnConfig.h`**
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssessorPawnConfig.generated.h"

UCLASS(BlueprintType)
class YOURGAME_API UAssessorPawnConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    // Downward camera tilt applied to the SpringArm (negative pitch).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Camera")
    float CameraTiltDegrees = 30.f;

    // Zoom represented as SpringArm length.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Zoom")
    float StartingArmLength = 1500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Zoom")
    float MinArmLength = 400.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Zoom")
    float MaxArmLength = 4000.f;

    // Planar move speed on AxisBasis X/Y (cm/s).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Movement")
    float MoveSpeed = 2400.f;

    // Discrete zoom step per wheel tick.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Zoom")
    float ZoomStep = 200.f;

    // SpringArm smoothing.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Lag")
    bool bEnableCameraLag = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="bEnableCameraLag"), Category="Assessor|Lag")
    float CameraLagSpeed = 10.f;  // higher = snappier

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="bEnableCameraLag"), Category="Assessor|Lag")
    float CameraLagMaxDistance = 250.f;
};
```

**`AssessorPawnConfig.cpp`**
```cpp
#include "AssessorPawnConfig.h"
// Data-only asset. No implementation required.
```

---

## 3) Pawn — `APACS_AssessorPawn`

**Responsibilities now:**
- Maintain an `AxisBasis` scene component that defines the local planar movement basis (its X/Y only; Z locked).
- Maintain a `USpringArmComponent` pitched downward by `-CameraTiltDegrees`; drive zoom via `TargetArmLength` with clamps and optional lag (`bEnableCameraLag`, `CameraLagSpeed`, `CameraLagMaxDistance`).
- Maintain a `UCameraComponent` attached to the SpringArm tip.
- Implement `IPACS_InputReceiver` and *only* consume the three actions listed below.
- **Do not replicate** actor or movement.

> Rotation will be added later by yawing **AxisBasis**. Not part of Phase 1.4.

**`PACS_AssessorPawn.h`**
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "PACS_InputTypes.h"                // IPACS_InputReceiver, EPACS_InputHandleResult
#include "AssessorPawnConfig.h"
#include "PACS_AssessorPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UPACS_InputHandlerComponent;
class APACS_PlayerController;

/**
 * Minimal Assessor spectator pawn (client-only navigation):
 * - WASD planar movement using AxisBasis (local X/Y only; Z locked)
 * - Mouse wheel zoom via SpringArm TargetArmLength (clamped)
 * - Camera tilt set by config; optional camera lag
 * - Registers with PACS input handler on possession
 * Threading: game-thread only.
 */
UCLASS()
class YOURGAME_API APACS_AssessorPawn : public ASpectatorPawn, public IPACS_InputReceiver
{
    GENERATED_BODY()

public:
    APACS_AssessorPawn();

    // IPACS_InputReceiver
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override;
    virtual int32 GetInputPriority() const override { return PACS_InputPriority::Gameplay; }

    // Config
    UPROPERTY(EditDefaultsOnly, Category="Assessor|Config")
    TObjectPtr<UAssessorPawnConfig> Config;

    // Narrow navigation API (controller may call later).
    void AddPlanarInput(const FVector2D& Axis01); // Accumulate this frame (X=Right, Y=Forward)
    void AddZoomSteps(float Steps);                // Discrete wheel ticks

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;

private:
    // Components
    UPROPERTY(VisibleAnywhere, Category="Assessor|Components")
    TObjectPtr<USceneComponent> AxisBasis;

    UPROPERTY(VisibleAnywhere, Category="Assessor|Components")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, Category="Assessor|Components")
    TObjectPtr<UCameraComponent> Camera;

    // Input accumulation
    float InputForward = 0.f;
    float InputRight   = 0.f;

    // Target zoom (ArmLength)
    float TargetArmLength = 0.f;

    // Helpers
    void RegisterWithInputHandler(APACS_PlayerController* PC);
    void UnregisterFromInputHandler(APACS_PlayerController* PC);
    void ApplyConfigDefaults();
    void StepZoom(float AxisValue);
};
```

**`PACS_AssessorPawn.cpp`**
```cpp
#include "PACS_AssessorPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"

#include "PACS_PlayerController.h"
#include "PACS_InputHandlerComponent.h"

APACS_AssessorPawn::APACS_AssessorPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    AxisBasis = CreateDefaultSubobject<USceneComponent>(TEXT("AxisBasis"));
    AxisBasis->SetupAttachment(RootComponent);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(AxisBasis);
    SpringArm->bDoCollisionTest = false;  // invisible pawn; avoid camera popping
    SpringArm->TargetArmLength = 1500.f;  // overridden in ApplyConfigDefaults()

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);

    // Non-replicated visuals/transform.
    bReplicates = false;
    SetReplicateMovement(false);
}

void APACS_AssessorPawn::BeginPlay()
{
    Super::BeginPlay();
    ApplyConfigDefaults();
}

void APACS_AssessorPawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
#if !UE_SERVER
    if (APACS_PlayerController* PC = Cast<APACS_PlayerController>(NewController))
    {
        RegisterWithInputHandler(PC);
    }
#endif
}

void APACS_AssessorPawn::UnPossessed()
{
#if !UE_SERVER
    if (APACS_PlayerController* PC = Cast<APACS_PlayerController>(GetController()))
    {
        UnregisterFromInputHandler(PC);
    }
#endif
    Super::UnPossessed();
}

void APACS_AssessorPawn::ApplyConfigDefaults()
{
    const float Tilt  = (Config ? Config->CameraTiltDegrees    : 30.f);
    const float Arm   = (Config ? Config->StartingArmLength    : 1500.f);
    const bool  bLag  = (Config ? Config->bEnableCameraLag     : true);
    const float LagSp = (Config ? Config->CameraLagSpeed       : 10.f);
    const float LagMx = (Config ? Config->CameraLagMaxDistance : 250.f);

    // Tilt the rig downward (negative pitch)
    const FRotator BasisRot(-Tilt, 0.f, 0.f);
    SpringArm->SetRelativeRotation(BasisRot);

    // Spring Arm length & lag
    SpringArm->TargetArmLength = Arm;
    SpringArm->bEnableCameraLag = bLag;
    SpringArm->CameraLagSpeed = LagSp;
    SpringArm->CameraLagMaxDistance = LagMx;

    TargetArmLength = Arm;
}

void APACS_AssessorPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Planar movement in AxisBasis' local frame
    const FVector Fwd   = AxisBasis->GetForwardVector();
    const FVector Right = AxisBasis->GetRightVector();
    const float   Speed = (Config ? Config->MoveSpeed : 2400.f);

    FVector Delta = (Fwd * InputForward + Right * InputRight) * Speed * DeltaSeconds;
    Delta.Z = 0.f;
    AddActorWorldOffset(Delta, false);

    // Drive ArmLength toward Target (instant for now)
    SpringArm->TargetArmLength = TargetArmLength;

    // Consume per-frame inputs
    InputForward = 0.f;
    InputRight   = 0.f;
}

void APACS_AssessorPawn::StepZoom(float AxisValue)
{
    if (FMath::IsNearlyZero(AxisValue)) return;

    const float Step = (Config ? Config->ZoomStep      : 200.f);
    const float MinL = (Config ? Config->MinArmLength  : 400.f);
    const float MaxL = (Config ? Config->MaxArmLength  : 4000.f);

    TargetArmLength = FMath::Clamp(TargetArmLength + AxisValue * Step, MinL, MaxL);
}

// ===== IPACS_InputReceiver =====
EPACS_InputHandleResult APACS_AssessorPawn::HandleInputAction(FName ActionName, const FInputActionValue& Value)
{
#if !UE_SERVER
    if (ActionName == TEXT("Assessor.MoveForward"))
    {
        InputForward += Value.Get<float>();
        return EPACS_InputHandleResult::HandledConsume;
    }
    if (ActionName == TEXT("Assessor.MoveRight"))
    {
        InputRight += Value.Get<float>();
        return EPACS_InputHandleResult::HandledConsume;
    }
    if (ActionName == TEXT("Assessor.Zoom"))
    {
        StepZoom(Value.Get<float>());
        return EPACS_InputHandleResult::HandledConsume;
    }
#endif
    return EPACS_InputHandleResult::NotHandled;
}

// ===== Narrow API (controller may call these directly later) =====
void APACS_AssessorPawn::AddPlanarInput(const FVector2D& Axis01)
{
    InputForward += Axis01.Y;
    InputRight   += Axis01.X;
}

void APACS_AssessorPawn::AddZoomSteps(float Steps)
{
    StepZoom(Steps);
}

void APACS_AssessorPawn::RegisterWithInputHandler(APACS_PlayerController* PC)
{
    if (!PC) return;
    if (UPACS_InputHandlerComponent* Handler = PC->FindComponentByClass<UPACS_InputHandlerComponent>())
    {
        Handler->RegisterReceiver(this, PACS_InputPriority::Gameplay);
    }
}

void APACS_AssessorPawn::UnregisterFromInputHandler(APACS_PlayerController* PC)
{
    if (!PC) return;
    if (UPACS_InputHandlerComponent* Handler = PC->FindComponentByClass<UPACS_InputHandlerComponent>())
    {
        Handler->UnregisterReceiver(this);
    }
}
```

---

## 4) PACS Input Wiring (content work)

Create three **Axis1D** `UInputAction` assets and map them in your `UPACS_InputMappingConfig` under the **Gameplay** context:

| Input Action Asset        | Identifier (FName)        | Bindings                             |
|---------------------------|---------------------------|--------------------------------------|
| `IA_AssessorMoveForward`  | `"Assessor.MoveForward"`  | W = +1, S = −1                       |
| `IA_AssessorMoveRight`    | `"Assessor.MoveRight"`    | D = +1, A = −1                       |
| `IA_AssessorZoom`         | `"Assessor.Zoom"`         | Mouse Wheel Up/Down (+1 / −1 scale)  |

> The pawn registers with the `UPACS_InputHandlerComponent` owned by `APACS_PlayerController` on possession. No extra glue code is required in Phase 1.4.

---

## 5) Editor Setup (do in this order)

1. Create a **Data Asset** of class `UAssessorPawnConfig` (e.g., `/Game/PACS/Data/DA_AssessorPawnConfig`).  
   Suggested defaults:  
   - `CameraTiltDegrees = 30`  
   - `StartingArmLength = 1500`  
   - `MinArmLength = 400`, `MaxArmLength = 4000`  
   - `MoveSpeed = 2400`  
   - `ZoomStep = 200`  
   - `bEnableCameraLag = true`, `CameraLagSpeed = 10`, `CameraLagMaxDistance = 250`
2. Create a **Blueprint** of `APACS_AssessorPawn` (optional) and assign `Config` to the asset above.
3. Ensure **GameMode** spawns/possesses this pawn for the Assessor client path (your existing logic is fine).

---

## 6) Completion Definition (must all pass)

- [ ] Compiles with **zero** warnings (Win64 Development Editor).  
- [ ] Actor **does not replicate**; movement replication disabled.  
- [ ] **WASD** moves only on X/Y (no Z drift).  
- [ ] **Mouse wheel** changes `SpringArm.TargetArmLength` with clamps to `[MinArmLength, MaxArmLength]`.  
- [ ] Camera tilt equals `CameraTiltDegrees` via SpringArm rotation.  
- [ ] SpringArm lag reflects config values.  
- [ ] Pawn registers with PACS input on possession and receives all three actions.  
- [ ] No references to selection, UI, RPCs, or server systems.

---

## 7) Sanity Tests (manual)

1. **Planar lock** — Hold W; verify Z of pawn remains constant while moving.  
2. **Zoom clamps** — Scroll to extremes; arm length respects min/max.  
3. **Lag feel** — Move and stop; camera eases. Tweak lag values and confirm change.  
4. **Input routing** — Temporarily log in `HandleInputAction` to confirm action identifiers are firing.

---

## 8) Troubleshooting

- **No input firing**: confirm `APACS_PlayerController` is active and owns a `UPACS_InputHandlerComponent`; confirm Gameplay context contains the three actions with exact `ActionIdentifier` FNames.  
- **Camera collision pops**: ensure `SpringArm->bDoCollisionTest = false`.  
- **Zoom feels inverted**: flip sign on the action’s axis scale in the Input Action mapping (content side).  
- **Too jittery**: increase `CameraLagSpeed` or reduce `MoveSpeed`.  
- **Too floaty**: disable lag or raise `CameraLagSpeed` to 12–16.

---

## 9) Future Hooks (do not implement now)

- **Rotation**: yaw **AxisBasis** only; keep SpringArm pitch for tilt.  
- **Edge scroll**: handled by PlayerController (poll viewport, then call `AddPlanarInput`).  
- **Selection/marquee**: PlayerController components only.  
- **Server commands**: PlayerController/PlayerState handle RPCs and replication.

---

## 10) Notes for Claude Code (compliance reminders)

- Use forward declarations in headers; keep includes to `.cpp` where possible.  
- Respect UE logging and error‑handling patterns; avoid `ensureAlways` unless necessary.  
- Do not add Enhanced Input setup beyond PACS identifiers.  
- Keep public API narrow (`AddPlanarInput`, `AddZoomSteps`).  
- No additional components, no UMG, no RPCs in this phase.
