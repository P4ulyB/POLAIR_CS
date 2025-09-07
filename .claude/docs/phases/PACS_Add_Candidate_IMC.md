# Add “Candidate” Input Mapping Context (IMC) – Global via Data Asset (UE 5.6)

**Audience:** Claude Code (to implement) + Pauly (to verify)  
**Goal:** Add a new Input Mapping Context named **Candidate** that can be toggled on/off for the candidate pawn and is exposed globally via the existing `UPACS_InputMappingConfig` Data Asset. No Shipping impact.

---

## 0) Design Summary (what to build)

- **Data Asset** (`UPACS_InputMappingConfig`): add `UInputMappingContext* CandidateContext;` so the IMC can be authored once and used across maps.
- **Input Handler** (`UPACS_InputHandlerComponent`): add a small API to **enable/disable** the Candidate IMC at runtime. It **adds/removes** the IMC from the `UEnhancedInputLocalPlayerSubsystem` with a fixed **Pawn-level priority** (between Gameplay and UI).
- **Non-breaking**: Candidate is **optional**. `IsValid()` must **not** fail if `CandidateContext` is `nullptr`.
- **No auto-magic detection** of pawn class in this patch. We provide explicit calls that can be made from `Pawn::PossessedBy`, `PlayerController::OnPossess`, or gameplay code when the “candidate mode” is entered/exited.

Priority convention (adjust if you already have different numbers):
```cpp
namespace PACS_InputPriority
{
    constexpr int32 Critical = 1000; // pause, debug, etc.
    constexpr int32 UI       =  400;
    constexpr int32 Pawn     =  300; // NEW: Candidate
    constexpr int32 Gameplay =  200;
}
```

---

## 1) Data Asset: add Candidate IMC property

**File:** `Source/<YourGame>/Public/PACS_InputMappingConfig.h`

```cpp
// Inside UPACS_InputMappingConfig class public UPROPERTY section
/** Optional IMC for candidate pawn controls. If set, it can be enabled at runtime by the handler. */
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PACS|IMC")
class UInputMappingContext* CandidateContext = nullptr;
```

**File:** `Source/<YourGame>/Private/PACS_InputMappingConfig.cpp`

- **No required change** to serialization if you are not doing custom serialize.
- Update `IsValid()` **only if** you currently require specific contexts:
  - Keep existing checks for Gameplay/Menu/UI unchanged.
  - **Do not** require `CandidateContext` to be non-null.
  - If you expose a “strict” validation mode, treat `CandidateContext` as optional.

Example diff inside `IsValid()`:
```cpp
bool UPACS_InputMappingConfig::IsValid() const
{
    const bool bBaseOK = (GameplayContext && MenuContext && UIContext);
    const bool bActionsOK = ActionMappings.Num() > 0 && ActionMappings.Num() <= PACS_InputLimits::MaxActionsPerConfig;
    // CandidateContext is OPTIONAL
    return bBaseOK && bActionsOK;
}
```

---

## 2) Input Handler API to toggle Candidate IMC

**File:** `Source/<YourGame>/Public/PACS_InputHandlerComponent.h`

```cpp
public:

    /** Enable Candidate IMC. If OptionalOverride is null, uses Config->CandidateContext. */
    UFUNCTION(BlueprintCallable, Category = "PACS|Input")
    void EnableCandidateInput(class UInputMappingContext* OptionalOverride = nullptr, int32 Priority = PACS_InputPriority::Pawn);

    /** Disable Candidate IMC previously enabled. */
    UFUNCTION(BlueprintCallable, Category = "PACS|Input")
    void DisableCandidateInput();
```

**File:** `Source/<YourGame>/Private/PACS_InputHandlerComponent.cpp`

Add includes at top if missing:
```cpp
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputLocalPlayerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
```

Add private helper to get local player subsystem (near other helpers):
```cpp
static UEnhancedInputLocalPlayerSubsystem* GetEnhancedSubsystem(APlayerController* PC)
{
    if (!PC) { return nullptr; }
    if (ULocalPlayer* LP = PC->GetLocalPlayer())
    {
        return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    }
    return nullptr;
}
```

Implement the public methods:
```cpp
void UPACS_InputHandlerComponent::EnableCandidateInput(UInputMappingContext* OptionalOverride, int32 Priority)
{
    if (!ensureMsgf(InputConfig, TEXT("EnableCandidateInput: InputConfig is null"))) { return; }

    // Choose IMC
    UInputMappingContext* IMC = OptionalOverride ? OptionalOverride : InputConfig->CandidateContext;
    if (!IMC)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnableCandidateInput: No Candidate IMC (nullptr)."));
        return;
    }

    // Only for local player controller
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !PC->IsLocalController())
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnableCandidateInput: skipped (non-local or missing PC)."));
        return;
    }

    if (UEnhancedInputLocalPlayerSubsystem* Subsys = GetEnhancedSubsystem(PC))
    {
        Subsys->AddMappingContext(IMC, Priority);
        TrackedCandidateIMC = IMC;
    }
}

void UPACS_InputHandlerComponent::DisableCandidateInput()
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !PC->IsLocalController())
    {
        return;
    }

    if (UEnhancedInputLocalPlayerSubsystem* Subsys = GetEnhancedSubsystem(PC))
    {
        UInputMappingContext* IMC = TrackedCandidateIMC ? TrackedCandidateIMC : (InputConfig ? InputConfig->CandidateContext : nullptr);
        if (IMC)
        {
            Subsys->RemoveMappingContext(IMC);
        }
        TrackedCandidateIMC = nullptr;
    }
}
```

Header field for tracking (optional but recommended):

**File:** `Source/<YourGame>/Public/PACS_InputHandlerComponent.h`

```cpp
protected:
    /** IMC we last enabled for Candidate so we can remove the exact instance. */
    UPROPERTY(Transient)
    class UInputMappingContext* TrackedCandidateIMC = nullptr;
```

---

## 3) Hook points (how gameplay turns it on/off)

Choose **one** of these patterns (keep it explicit and server-authoritative as per your policy):

- **On possession** (client-local only adds IMC; authority validated before state changes):
  - In `A<YourCandidatePawn>::PossessedBy(AController* NewController)` (server) → replicate “candidate mode true” to owning client → client `OnRep` calls `EnableCandidateInput()` on the handler.
- **On mode enter/exit**:
  - When the gameplay system sets candidate mode, call `EnableCandidateInput()`; when exiting, call `DisableCandidateInput()`.

> The handler deliberately **does not assume** the pawn class. You keep control in gameplay code and respect authority/validation rules.

---

## 4) Editor wiring: set the Candidate IMC in the DA

1. Create a new **Input Mapping Context** asset in Content: `IMC_Candidate`.  
2. Open your `DA_PACS_InputConfig` (or equivalent) and set **CandidateContext = IMC_Candidate**.  
3. (Optional) Add any `UInputAction` mappings required by the candidate pawn.

---