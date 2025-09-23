# UE5.5 Dedicated‑Server Selection System (MVP) — Step‑by‑Step Guide

**Goal:** A minimal, server‑authoritative selection system where **one replicated pointer** on the NPC grants contextual‑action authority to exactly one player at a time. **Visuals (colour/brightness)** are applied **client‑side** in response to replication.

> This is intentionally simple: no registries, no extra subsystems. You can layer those later if needed.

---

## 0) Assumptions & Constraints

- Unreal Engine **5.5** (source or binary), dedicated server model.
- You already have an NPC actor class with a **decal/material MID setup** and local functions to set colour/brightness (e.g., `ApplyAvailableVisuals_Local()`, `ApplySelectedVisuals_Local()`, `ApplyUnavailableVisuals_Local()`).
- You already have an **Input Handler** that detects left‑clicks and passes along the hit actor.
- Hover remains **local‑only** and is not replicated.

---

## 1) NPC: add a single replicated owner pointer

### 1.1 Header changes (`YourNPC.h`)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "YourNPC.generated.h"

UCLASS()
class AYourNPC : public ACharacter
{
    GENERATED_BODY()

public:
    AYourNPC();

    /** The player who currently owns/has selected this NPC. Server sets it; clients read it. */
    UPROPERTY(ReplicatedUsing=OnRep_CurrentSelector)
    TObjectPtr<APlayerState> CurrentSelector = nullptr;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Called on clients whenever CurrentSelector changes. */
    UFUNCTION()
    void OnRep_CurrentSelector();

    // Optional: if your MID might not be ready when OnRep fires, track pending state
    UPROPERTY(Transient)
    uint8 bHasPendingSelectionState : 1;

    UPROPERTY(Transient)
    uint8 PendingSelectionCase = 0; // 0=Available, 1=Selected, 2=Unavailable

public:
    // These are *local-only* visual hooks you already have, or implement them minimally
    void ApplyAvailableVisuals_Local();
    void ApplySelectedVisuals_Local();
    void ApplyUnavailableVisuals_Local();
};
```

### 1.2 Source changes (`YourNPC.cpp`)

```cpp
#include "YourNPC.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

AYourNPC::AYourNPC()
{
    bReplicates = true;
    // Ensure any visual component MIDs are created client-side when relevant
}

void AYourNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AYourNPC, CurrentSelector);
}

void AYourNPC::OnRep_CurrentSelector()
{
    // Dedicated servers don't paint visuals
    if (IsRunningDedicatedServer())
    {
        return;
    }

    // Resolve the local player's PlayerState (single local player assumed)
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APlayerState* LocalPS = PC ? PC->PlayerState : nullptr;

    // If your MID is not ready yet, defer painting until after it's created
    auto Paint = [&](uint8 Case)
    {
        switch (Case)
        {
        case 1: ApplySelectedVisuals_Local(); break;
        case 2: ApplyUnavailableVisuals_Local(); break;
        default: ApplyAvailableVisuals_Local(); break;
        }
    };

    const bool bMIDReady = true; // flip to a real readiness check if needed

    uint8 Case = 0; // Available
    if (IsValid(CurrentSelector))
    {
        Case = (CurrentSelector == LocalPS) ? 1 : 2;
    }

    if (!bMIDReady)
    {
        bHasPendingSelectionState = true;
        PendingSelectionCase = Case;
        return;
    }

    Paint(Case);
}

void AYourNPC::EndPlay(const EEndPlayReason::Type Reason)
{
    // On server: if we’re going away, clear selection to avoid stale refs
    if (HasAuthority() && CurrentSelector)
    {
        CurrentSelector = nullptr;
        ForceNetUpdate(); // push out the clear quickly
    }

    Super::EndPlay(Reason);
}

// Stub out your local paint functions if you don't already have them.
// They should set colour/brightness on your MID only on this client.
void AYourNPC::ApplyAvailableVisuals_Local()   { /* set colour/brightness for available */ }
void AYourNPC::ApplySelectedVisuals_Local()    { /* set colour/brightness for selected  */ }
void AYourNPC::ApplyUnavailableVisuals_Local() { /* set colour/brightness for unavailable */ }
```

> **Tip:** If your MID is created asynchronously, call `OnRep_CurrentSelector()` or a tiny “ApplyPendingVisualsIfAny()” at the end of the MID‑creation path to honour the latest `PendingSelectionCase`.

---

## 2) PlayerState: track your current selection (server‑only)

Keep a **server‑only** pointer so you can quickly release previous selections and clean up on logout. Use `TWeakObjectPtr` to prevent dangling when NPCs are destroyed.

### 2.1 Header (`YourPlayerState.h`)
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "YourPlayerState.generated.h"

class AYourNPC;

UCLASS()
class AYourPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    // Server-only: currently selected NPC for this player (not replicated)
    TWeakObjectPtr<AYourNPC> SelectedNPC_ServerOnly;

    FORCEINLINE AYourNPC* GetSelectedNPC_ServerOnly() const { return SelectedNPC_ServerOnly.Get(); }
    FORCEINLINE void SetSelectedNPC_ServerOnly(AYourNPC* InNPC) { SelectedNPC_ServerOnly = InNPC; }
};
```

### 2.2 Source (`YourPlayerState.cpp`)
```cpp
#include "YourPlayerState.h"
#include "YourNPC.h"
```

---

## 3) PlayerController: RPCs for Select / Deselect

### 3.1 Header (`YourPlayerController.h`)
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "YourPlayerController.generated.h"

UCLASS()
class AYourPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UFUNCTION(Server, Reliable)
    void ServerRequestSelect(AActor* Target);

    UFUNCTION(Server, Reliable)
    void ServerRequestDeselect();
};
```

### 3.2 Source (`YourPlayerController.cpp`)

```cpp
#include "YourPlayerController.h"
#include "YourPlayerState.h"
#include "YourNPC.h"
#include "Engine/World.h"

void AYourPlayerController::ServerRequestSelect_Implementation(AActor* Target)
{
    if (!HasAuthority() || !IsValid(Target)) return;

    AYourPlayerState* PS = GetPlayerState<AYourPlayerState>();
    AYourNPC* TargetNPC  = Cast<AYourNPC>(Target);
    if (!PS || !TargetNPC) return;

    // Optional server verification: (re)trace from server POV to confirm click legit
    // if (!ServerConfirmsHit(TargetNPC)) return;

    // Release my previous selection, if any
    if (AYourNPC* Prev = PS->GetSelectedNPC_ServerOnly())
    {
        Prev->CurrentSelector = nullptr;
        Prev->ForceNetUpdate();
        PS->SetSelectedNPC_ServerOnly(nullptr);
    }

    // If target still free, claim it
    if (TargetNPC->CurrentSelector == nullptr)
    {
        TargetNPC->CurrentSelector = PS;
        TargetNPC->ForceNetUpdate();
        PS->SetSelectedNPC_ServerOnly(TargetNPC);
    }
    // else: already owned by someone else → do nothing (or send client feedback)
}

void AYourPlayerController::ServerRequestDeselect_Implementation()
{
    if (!HasAuthority()) return;

    AYourPlayerState* PS = GetPlayerState<AYourPlayerState>();
    if (!PS) return;

    if (AYourNPC* NPC = PS->GetSelectedNPC_ServerOnly())
    {
        NPC->CurrentSelector = nullptr;
        NPC->ForceNetUpdate();
        PS->SetSelectedNPC_ServerOnly(nullptr);
    }
}
```

---

## 4) GameMode: clean up on player exit

### 4.1 Source (`YourGameMode.cpp`)
```cpp
#include "YourGameMode.h"
#include "YourPlayerState.h"
#include "YourNPC.h"

void AYourGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    AYourPlayerState* PS = Exiting ? Exiting->GetPlayerState<AYourPlayerState>() : nullptr;
    if (!PS) return;

    if (AYourNPC* NPC = PS->GetSelectedNPC_ServerOnly())
    {
        NPC->CurrentSelector = nullptr;
        NPC->ForceNetUpdate();
        PS->SetSelectedNPC_ServerOnly(nullptr);
    }
}
```

---

## 5) Gate all contextual actions (server‑side)

Any server RPC that performs an action on an NPC must verify ownership:

```cpp
// Example server RPC entry somewhere on the server
void AYourNPC::Server_PerformContextAction_Implementation(AYourPlayerController* RequestingPC)
{
    if (!HasAuthority() || !RequestingPC) return;
    if (CurrentSelector != RequestingPC->PlayerState) return; // reject: not the owner

    // ...perform the action...
}
```

> This guarantees only the selecting player can execute contextual actions.


---

## 6) Input: wire up left‑clicks to RPCs

From your Input Handler (client‑side):

- If left‑click hits a valid `AYourNPC` → `PC->ServerRequestSelect(TargetNPC)`
- If left‑click hits **empty world** → `PC->ServerRequestDeselect()`

This keeps hover (local highlight) separate from selection authority.


---

## 7) Testing Checklist (2+ clients in PIE or standalone)

1. **Client A** clicks NPC‑1 → A sees **Selected (Green)**, **Client B** sees **Unavailable (Red)**.
2. **Client B** clicks the same NPC‑1 → nothing changes; B gets feedback (optional) because `CurrentSelector != nullptr`.
3. **Client A** clicks empty world → NPC‑1 becomes **Available** for all.
4. **Client B** now clicks NPC‑1 → B owns it; A sees **Red**.
5. **Client that owns an NPC disconnects** → `Logout` clears pointer; NPC goes **Available** for all.
6. **NPC is destroyed** while owned → `EndPlay` clears `CurrentSelector`; everyone sees **Available**.
7. **Contextual action** attempted by a non‑owner → rejected by server gate.

> Enable logs to verify flow:
> - Add `UE_LOG(LogTemp, Log, TEXT("Select: %s -> %s"), *GetNameSafe(PS), *GetNameSafe(TargetNPC));`
> - Add logs on `OnRep_CurrentSelector` to see client‑side visual decisions.


---

## 8) Common Pitfalls & Fixes

- **“Why aren’t colours replicating?”** They shouldn’t. The **pointer** (`CurrentSelector`) replicates; each client paints locally in `OnRep_CurrentSelector()`.
- **“MID isn’t ready when OnRep fires.”** Use the `bHasPendingSelectionState` approach and apply once your MID is created.
- **“Multiple selections per player?”** The above pattern enforces **one at a time**. If you need multiple, change `YourPlayerState` to track a `TSet<TWeakObjectPtr<AYourNPC>>` and adjust release logic.
- **“I want to validate clicks server‑side.”** Add a cheap line trace in `ServerRequestSelect_Implementation` from a trusted server origin and compare actors.
- **“I need UI labels for owner name.”** Use `Cast<AYourPlayerState>(CurrentSelector)->GetPlayerName()` **only for display** (don’t drive logic with names).


---

## 9) When to Scale Up (Optional Future Work)

- **GameState FastArray mirror** if you need a global UI list of selections, analytics, or plan to manage thousands of items.
- **Cooldowns / LOS / range** checks in `ServerRequestSelect` for game rules.
- **Per‑team availability** (e.g., blue vs red) by extending the client‑side paint decision with team info from `PlayerState`.


---

## 10) Files Touched (summary)

- `YourNPC.h/.cpp` — add `CurrentSelector`, `OnRep_CurrentSelector`, tidy in `EndPlay`, local paint calls.
- `YourPlayerState.h/.cpp` — server‑only `TWeakObjectPtr<AYourNPC>` with getters/setters.
- `YourPlayerController.h/.cpp` — `ServerRequestSelect/ServerRequestDeselect` RPCs.
- `YourGameMode.cpp` — `Logout` cleanup.

That’s it — minimal, authoritative, bandwidth‑light, and easy to maintain.
