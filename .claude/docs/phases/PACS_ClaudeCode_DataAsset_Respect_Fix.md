# PACS — Claude Code Instructions: Make Data Asset Drive Spawn and Runtime, Without Stomping CMC Data (UE 5.5/5.6)

Follow these steps **exactly**. No deviation. No scope creep. Implement only what is specified below.

Objective:
1) Ensure the **movement component (CMC)** uses a valid **UPACS_CandidateHelicopterData** at all times.  
2) Prevent overwriting a valid CMC Data pointer with `nullptr` in the character.  
3) Seed using an **effective** data asset (character Data or CMC Data).  
4) Make clients **snap** their CMC working state on `OnRep_OrbitTargets`.  
5) Keep movement in **MOVE_Custom / CMOVE_HeliOrbit** on server and clients.

Preconditions:
- GameMode already calls `ApplyOffsetsThenSeed(...)` after server spawn/possess.
- `UPACS_HeliMovementComponent` no longer shadows `ClientPredictionData` in the header.

---

## Files to modify

```
Source/<GameModule>/PACS/Heli/PACS_CandidateHelicopterCharacter.h
Source/<GameModule>/PACS/Heli/PACS_CandidateHelicopterCharacter.cpp
Source/<GameModule>/PACS/Heli/PACS_HeliMovementComponent.cpp  (optional diagnostics only)
```

Do not rename or move files.

---

## 1) Character: assert custom mode and **prefer-existing** Data wiring

### 1.1 Header — `PACS_CandidateHelicopterCharacter.h`

Ensure these overrides are declared (add if missing; keep visibility consistent with your codebase):
```cpp
virtual void BeginPlay() override;
virtual void PossessedBy(AController* NewController) override;
virtual void OnRep_Controller() override;
virtual void OnRep_OrbitTargets() override; // if already declared, keep it
```

### 1.2 Source — `PACS_CandidateHelicopterCharacter.cpp`

Add required includes near the top:
```cpp
#include "PACS_HeliMovementComponent.h"
#include "PACS_CandidateHelicopterData.h"
```

Replace or implement the following methods **exactly** (only the shown logic; do not add unrelated work):

```cpp
void APACS_CandidateHelicopterCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        // Ensure custom movement
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
        CMC->bConstrainToPlane = true;

        // Prefer-existing: do NOT stomp a valid CMC->Data with nullptr.
        if (!CMC->Data && Data)      { CMC->Data = Data; }
        else if (CMC->Data && !Data) { Data = CMC->Data; }

        // Optional diagnostic (remove after validation)
        // ensureMsgf(CMC->Data != nullptr, TEXT("PACS: CMC->Data is NULL. Set the data asset on either the Character or the CMC in BP."));
    }
}

void APACS_CandidateHelicopterCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
    }
}

void APACS_CandidateHelicopterCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();
    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
    }
}
```

---

## 2) Character: clients **snap** working state on `OnRep_OrbitTargets`

Implement or replace the body of `OnRep_OrbitTargets()`:

```cpp
void APACS_CandidateHelicopterCharacter::OnRep_OrbitTargets()
{
    if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        // Snap the CMC working state to replicated targets when they arrive
        CMC->CenterCm   = OrbitTargets.CenterCm;
        CMC->AltitudeCm = OrbitTargets.AltitudeCm;
        CMC->RadiusCm   = OrbitTargets.RadiusCm;
        CMC->SpeedCms   = OrbitTargets.SpeedCms;

        // Re-assert custom movement on clients
        CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
        CMC->bConstrainToPlane = true;
    }
}
```

Do not add additional logic in this function.

---

## 3) Character: seed with an **effective** data asset

Open `APACS_CandidateHelicopterCharacter::ApplyOffsetsThenSeed(...)`. At the start of the function, compute an effective data pointer:

```cpp
const UPACS_CandidateHelicopterData* EffData = Data;
if (!EffData)
{
    if (const UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        EffData = CMC->Data;
    }
}
```

Use `EffData` for defaults and clamps, replacing any `Data ? X : fallback` that currently ignores CMC->Data:

```cpp
float Alt = EffData ? EffData->DefaultAltitudeCm : 20000.f;
float Rad = EffData ? EffData->DefaultRadiusCm   : 15000.f;
float Spd = EffData ? EffData->DefaultSpeedCms   : 2222.22f;
const float MaxSpd = EffData ? EffData->MaxSpeedCms : 6000.f;

// apply optional save offsets to Alt/Rad/Spd here (your existing logic)

// clamp using MaxSpd
OrbitTargets.CenterCm   = FVector(GetActorLocation().X, GetActorLocation().Y, 0.f);
OrbitTargets.AltitudeCm = FMath::Max(Alt, 100.f);
OrbitTargets.RadiusCm   = FMath::Max(Rad, 100.f);
OrbitTargets.SpeedCms   = FMath::Clamp(Spd, 0.f, MaxSpd);

// durations remain as you currently set them (0 for instant seed is fine)
```

At the **end** of `ApplyOffsetsThenSeed(...)`, re-assert custom movement to guarantee `PhysCustom` next tick:

```cpp
if (UPACS_HeliMovementComponent* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
{
    CMC->SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
}
```

Do not change any other logic in this function.

---

## 4) Optional diagnostics (temporary)

In `UPACS_HeliMovementComponent::Eval_Server()` add a one-time assertion near the top to catch missing data during bring-up; remove after validation:

```cpp
// Optional: remove after validation
// ensureMsgf(Data != nullptr, TEXT("PACS: UPACS_HeliMovementComponent::Data is NULL — wire Character Data -> CMC or set CMC Data in BP."));
```

No functional changes in the movement component are required for this task.

---

## 5) Data Asset sanity (content, not code)

Open your `UPACS_CandidateHelicopterData` asset instance and verify:
- `DefaultAltitudeCm > 0`
- `DefaultRadiusCm > 0`
- `DefaultSpeedCms > 0`
- `MaxSpeedCms >= DefaultSpeedCms`

If `MaxSpeedCms` is `0`, speed clamps to `0` during seed/eval and the pawn will not orbit.

---

## 6) Expected results

- On server spawn, seed values reflect the Data Asset defaults (or defaults + save offsets).  
- Clients snap their CMC working state to those replicated targets as soon as `OnRep_OrbitTargets` fires.  
- Pawn remains in **custom** movement and starts orbiting immediately.  
- Changing Data Asset defaults affects starting altitude/radius/speed on the next run, on server and clients.

End of instructions.
