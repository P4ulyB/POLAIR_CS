# PACS HMD-Based Pawn Spawn System - Phase 1 & 2 Instructions

## Project Configuration

**Project Path**: `C:\Devops\Projects\PACS`  
**Class Prefix**: `PACS_`  
**Plugin Required**: XRBase (enable in Plugins UI)  
**Module Required**: XRBase (add to Build.cs)

## Build.cs Configuration

Update `C:\Devops\Projects\PACS\Source\PACS\PACS.Build.cs`:

```cs
PublicDependencyModuleNames.AddRange(new string[] 
{
    "Core", 
    "CoreUObject", 
    "Engine"
});

PrivateDependencyModuleNames.AddRange(new string[] 
{
    "XRBase" // Required for UHeadMountedDisplayFunctionLibrary
});
```

## File Implementation

### 1. PACS_PlayerState.h

**Location**: `C:\Devops\Projects\PACS\Source\PACS\Public\PACS_PlayerState.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PACS_PlayerState.generated.h"

UENUM(BlueprintType)
enum class EHMDState : uint8
{
    Unknown = 0,    // HMD state not yet detected
    NoHMD = 1,      // No HMD connected/enabled
    HasHMD = 2      // HMD connected and enabled
};

UCLASS()
class PACS_API APACS_PlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    APACS_PlayerState();

    // HMD detection state - replicated to all clients for system-wide access
    UPROPERTY(ReplicatedUsing=OnRep_HMDState, BlueprintReadOnly, Category = "PACS")
    EHMDState HMDState = EHMDState::Unknown;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // RepNotify for HMD state changes
    UFUNCTION()
    void OnRep_HMDState();
};
```

### 2. PACS_PlayerState.cpp

**Location**: `C:\Devops\Projects\PACS\Source\PACS\Private\PACS_PlayerState.cpp`

```cpp
#include "PACS_PlayerState.h"
#include "Net/UnrealNetwork.h"

APACS_PlayerState::APACS_PlayerState()
{
    HMDState = EHMDState::Unknown;
}

void APACS_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate HMD state to all clients
    DOREPLIFETIME(APACS_PlayerState, HMDState);
}

void APACS_PlayerState::OnRep_HMDState()
{
    // Handle HMD state changes - update UI, notify systems, etc.
    // Called on clients when HMD state replicates
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: HMD state changed to %d"), static_cast<int32>(HMDState));
}
```

### 3. PACS_PlayerController.h

**Location**: `C:\Devops\Projects\PACS\Source\PACS\Public\PACS_PlayerController.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PACS_PlayerState.h"
#include "Engine/TimerHandle.h"
#include "PACS_PlayerController.generated.h"

UCLASS()
class PACS_API APACS_PlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APACS_PlayerController();

    // Per-player timer handle for HMD detection timeout
    FTimerHandle HMDWaitHandle;

protected:
    // Pending HMD state for PlayerState null scenarios
    EHMDState PendingHMDState = EHMDState::Unknown;
    bool bHasPendingHMDState = false;

    // Client RPC for zero-swap handshake
    UFUNCTION(Client, Reliable)
    void ClientRequestHMDState();
    void ClientRequestHMDState_Implementation();

    // Server RPC response for zero-swap handshake
    UFUNCTION(Server, Reliable)
    void ServerReportHMDState(EHMDState DetectedState);
    void ServerReportHMDState_Implementation(EHMDState DetectedState);

    // Override for server-side PlayerState initialization
    virtual void InitPlayerState() override;
};
```

### 4. PACS_PlayerController.cpp

**Location**: `C:\Devops\Projects\PACS\Source\PACS\Private\PACS_PlayerController.cpp`

```cpp
#include "PACS_PlayerController.h"
#include "PACS_PlayerState.h"
#include "PACS_GameMode.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"

APACS_PlayerController::APACS_PlayerController()
{
    // Constructor implementation
}

void APACS_PlayerController::ClientRequestHMDState_Implementation()
{
    EHMDState DetectedState = EHMDState::NoHMD;
    
    #if !UE_SERVER
    // Use XRBase API for robust HMD detection - check both connected and enabled
    if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayConnected() && 
        UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
    {
        DetectedState = EHMDState::HasHMD;
        UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: HMD detected and enabled"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS PlayerController: HMD not detected or not enabled"));
    }
    #else
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Server build - defaulting to NoHMD"));
    #endif
    
    ServerReportHMDState(DetectedState);
}

void APACS_PlayerController::ServerReportHMDState_Implementation(EHMDState DetectedState)
{
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Server received HMD state %d"), static_cast<int32>(DetectedState));
    
    // Guard PlayerState access with null check
    if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PlayerState))
    {
        // Store previous state to detect transitions
        EHMDState PreviousState = PACSPS->HMDState;
        PACSPS->HMDState = DetectedState;
        
        // Only trigger spawn if state transitioned from Unknown and player has no pawn
        if (PreviousState == EHMDState::Unknown && GetPawn() == nullptr)
        {
            UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Triggering spawn for player with HMD state %d"), static_cast<int32>(DetectedState));
            if (APACS_GameMode* GM = GetWorld()->GetAuthGameMode<APACS_GameMode>())
            {
                GM->HandleStartingNewPlayer(this);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS PlayerController: Spawn not triggered - PreviousState: %d, HasPawn: %s"), 
                static_cast<int32>(PreviousState), GetPawn() ? TEXT("true") : TEXT("false"));
        }
    }
    else
    {
        // PlayerState null - queue HMD state for when it becomes available
        UE_LOG(LogTemp, Warning, TEXT("PACS PlayerController: PlayerState null - queueing HMD state"));
        PendingHMDState = DetectedState;
        bHasPendingHMDState = true;
    }
}

void APACS_PlayerController::InitPlayerState()
{
    Super::InitPlayerState();
    
    // Server-side PlayerState initialization - apply any pending HMD state
    if (bHasPendingHMDState)
    {
        UE_LOG(LogTemp, Log, TEXT("PACS PlayerController: Applying pending HMD state %d"), static_cast<int32>(PendingHMDState));
        
        if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PlayerState))
        {
            PACSPS->HMDState = PendingHMDState;
            bHasPendingHMDState = false;
            
            // Only trigger spawn if player has no pawn
            if (GetPawn() == nullptr)
            {
                if (APACS_GameMode* GM = GetWorld()->GetAuthGameMode<APACS_GameMode>())
                {
                    GM->HandleStartingNewPlayer(this);
                }
            }
        }
    }
}
```

### 5. PACS_GameMode.h

**Location**: `C:\Devops\Projects\PACS\Source\PACS\Public\PACS_GameMode.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PACS_PlayerState.h"
#include "PACS_GameMode.generated.h"

UCLASS()
class PACS_API APACS_GameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APACS_GameMode();

    // Override for player connection handling
    virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
    // Pawn classes for different user types
    UPROPERTY(EditDefaultsOnly, Category = "PACS|Spawning")
    TSubclassOf<APawn> CandidatePawnClass;

    UPROPERTY(EditDefaultsOnly, Category = "PACS|Spawning")
    TSubclassOf<APawn> AssessorPawnClass;

    // Override to provide custom pawn selection logic
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

    // Zero-swap spawn implementation - BlueprintNativeEvent override
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    
    // Timeout handler for zero-swap pattern
    UFUNCTION()
    void OnHMDTimeout(APlayerController* PlayerController);
};
```

### 6. PACS_GameMode.cpp

**Location**: `C:\Devops\Projects\PACS\Source\PACS\Private\PACS_GameMode.cpp`

```cpp
#include "PACS_GameMode.h"
#include "PACS_PlayerController.h"
#include "PACS_PlayerState.h"

APACS_GameMode::APACS_GameMode()
{
    // Set default PlayerState class
    PlayerStateClass = APACS_PlayerState::StaticClass();
    
    // Set pawn classes - configure in Blueprint or here
    // CandidatePawnClass = APACS_CandidatePawn::StaticClass();
    // AssessorPawnClass = APACS_AssessorPawn::StaticClass();
}

void APACS_GameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    
    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: PostLogin called for player"));
    
    // Zero-swap handshake: request HMD state immediately
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(NewPlayer))
    {
        // GameMode only exists on server, safe to call Client RPC
        UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Requesting HMD state from client"));
        PACSPC->ClientRequestHMDState();
    }
}

UClass* APACS_GameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    // GameMode only exists on server - no HasAuthority() check needed
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(InController))
    {
        // Guard PlayerState access with null check
        if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PACSPC->PlayerState))
        {
            // Return appropriate pawn class based on HMD detection
            switch (PACSPS->HMDState)
            {
                case EHMDState::HasHMD:
                    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Selecting CandidatePawn for HMD user"));
                    return CandidatePawnClass;
                case EHMDState::NoHMD:
                    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Selecting AssessorPawn for non-HMD user"));
                    return AssessorPawnClass;
                case EHMDState::Unknown:
                default:
                    UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: HMD state unknown - falling back to AssessorPawn"));
                    break;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: PlayerState null - falling back to AssessorPawn"));
        }
    }
    
    // Fail-safe: Default to AssessorPawn if PlayerState unavailable or HMD state unknown
    return AssessorPawnClass ? AssessorPawnClass : Super::GetDefaultPawnClassForController_Implementation(InController);
}

void APACS_GameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // Idempotence guard: prevent double spawn
    if (NewPlayer && NewPlayer->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: Player already has pawn - clearing timer and returning"));
        if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(NewPlayer))
        {
            GetWorld()->GetTimerManager().ClearTimer(PACSPC->HMDWaitHandle);
        }
        return;
    }

    // Zero-swap pattern: Only spawn once HMD state is known
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(NewPlayer))
    {
        // Guard PlayerState access with null check
        if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PACSPC->PlayerState))
        {
            if (PACSPS->HMDState != EHMDState::Unknown)
            {
                // HMD state known, clear any pending timer and proceed with spawn
                UE_LOG(LogTemp, Log, TEXT("PACS GameMode: HMD state known (%d) - proceeding with spawn"), static_cast<int32>(PACSPS->HMDState));
                GetWorld()->GetTimerManager().ClearTimer(PACSPC->HMDWaitHandle);
                Super::HandleStartingNewPlayer_Implementation(NewPlayer);
                return;
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("PACS GameMode: HMD state unknown - setting timeout"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: PlayerState null - setting timeout"));
        }
        
        // Set per-player timeout to prevent infinite wait
        GetWorld()->GetTimerManager().SetTimer(
            PACSPC->HMDWaitHandle,  // Per-player timer handle - prevents race conditions
            FTimerDelegate::CreateUObject(this, &APACS_GameMode::OnHMDTimeout, PACSPC),
            3.0f, false);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Non-PACS PlayerController - using default spawn"));
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void APACS_GameMode::OnHMDTimeout(APlayerController* PlayerController)
{
    UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: HMD detection timeout reached"));
    
    // Clear timer first to prevent race conditions
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(PlayerController))
    {
        GetWorld()->GetTimerManager().ClearTimer(PACSPC->HMDWaitHandle);
    }
    
    // Idempotence guard: prevent double spawn
    if (PlayerController && PlayerController->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: Player already has pawn during timeout - returning"));
        return;
    }

    // Timeout reached - force spawn with NoHMD default
    if (IsValid(PlayerController))
    {
        if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(PlayerController))
        {
            // Guard PlayerState access with null check
            if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PACSPC->PlayerState))
            {
                if (PACSPS->HMDState == EHMDState::Unknown)
                {
                    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Setting HMD state to NoHMD due to timeout"));
                    PACSPS->HMDState = EHMDState::NoHMD;
                }
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Forcing spawn after timeout"));
        Super::HandleStartingNewPlayer_Implementation(PlayerController);
    }
}
```

## Implementation Flow

### Zero-Swap Handshake Pattern
1. **PostLogin**: Server calls `ClientRequestHMDState()` on new PACS_PlayerController
2. **Client Detection**: Client detects HMD using robust detection (`IsHeadMountedDisplayConnected() && IsHeadMountedDisplayEnabled()`)
3. **Server Storage**: Server stores `EHMDState` in replicated PACS_PlayerState  
4. **PlayerState Null Handling**: If PlayerState null, queue state in PACS_PlayerController for `InitPlayerState()`
5. **Idempotent Spawning**: Guards prevent double spawns from late packets or race conditions
6. **Delayed Spawn**: `HandleStartingNewPlayer_Implementation` waits for HMD state before spawning
7. **Per-Player Timeout**: 3-second timeout using `PACS_PlayerController::HMDWaitHandle`
8. **Pawn Selection**: `GetDefaultPawnClassForController_Implementation` selects CandidatePawnClass or AssessorPawnClass

## Required Project Settings

1. **Enable XRBase Plugin**: Edit → Plugins → Search "XRBase" → Enable
2. **Set GameMode**: Project Settings → Maps & Modes → Default GameMode = PACS_GameMode
3. **Configure Pawn Classes**: Set CandidatePawnClass and AssessorPawnClass in PACS_GameMode Blueprint

## Critical Implementation Notes

- **Robust HMD Detection**: Uses both `IsHeadMountedDisplayConnected()` and `IsHeadMountedDisplayEnabled()` for reliability
- **XRBase API**: Includes `IXRTrackingSystem.h` for complete XR support
- **Tri-State Enum**: `EHMDState {Unknown, NoHMD, HasHMD}` with `ReplicatedUsing=OnRep_HMDState`
- **Per-Player Timers**: `FTimerHandle HMDWaitHandle` on each PACS_PlayerController prevents race conditions
- **Server-Side PlayerState Handling**: Uses `InitPlayerState()` override for pending state application
- **Zero Pawn Swapping**: Single spawn path via `HandleStartingNewPlayer_Implementation` override
- **BlueprintNativeEvent Pattern**: Correct `_Implementation` overrides for Epic's framework
- **No UnrealNetwork Module**: Uses Engine module only, includes `Net/UnrealNetwork.h` in .cpp files
- **Idempotent Spawning**: Guards against double spawns from late HMD reports or race conditions
- **Comprehensive Logging**: UE_LOG statements for debugging HMD detection and spawn flow

## Execution Result

- **HMD Users**: Spawn as CandidatePawn (VR character) when both connected and enabled
- **Non-HMD Users**: Spawn as AssessorPawn (spectator/desktop)
- **System-Wide Access**: HMD state available via replicated PACS_PlayerState
- **No Visible Swapping**: Clean single spawn with correct pawn type
- **Race Condition Free**: Per-player timer isolation with proper cleanup
- **Network Robust**: Server-side PlayerState null handling with `InitPlayerState()` callback
- **Double-Spawn Safe**: Prevents multiple spawns from late packets or timeout races
- **Production Ready**: Comprehensive logging for debugging and monitoring