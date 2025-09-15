# PACS Helicopter Candidate — Claude Code Final Implementation Instructions (UE 5.5/5.6)

This document is the **authoritative plan** for implementing the helicopter **Candidate** pawn and related systems. **Follow these steps exactly. No deviation. No scope creep.** Do not rename classes. Do not add features beyond what is specified here.

Non‑negotiables:
1. Use **ACharacter** with a **custom UCharacterMovementComponent** for the Candidate pawn.
2. **VR camera must NOT bank.** Only the helicopter frame mesh banks visually.
3. Client movement must use **UE character movement prediction** (SavedMove + client prediction data). Server remains authoritative.
4. Orbit parameter edits are sent via **one Reliable RPC** with batching, transaction id, and a single re‑anchor policy.
5. Movement uses **swept** movement and slides along boundaries. Orbit center must be validated against blocking geometry.
6. The system must accept **save offsets** (Altitude/Radius/Speed) applied on spawn to the **Data Asset defaults**. The save system remains external.
7. Linear **tangential speed** is invariant when radius changes. Angular rate adapts (`ω = v / r`).

---

## 0) Project and module setup

1) Enable plugins in your project:
- Enhanced Input
- HeadMountedDisplay

2) In your game module `.Build.cs`, add:
```csharp
PublicDependencyModuleNames.AddRange(new[] {
  "Core", "CoreUObject", "Engine", "InputCore",
  "EnhancedInput", "HeadMountedDisplay", "NetCore"
});
```

3) Existing classes must remain and are assumed to exist:
- `APACS_PlayerController`
- `APACSGameMode`
- `APACS_PlayerState`
- `UPACS_InputHandlerComponent`

Do not modify their names.

---

## 1) Files to add (exact paths)

Create these files under your game module. Replace `<GameModule>` with your actual module name.

```
Source/<GameModule>/PACS/Heli/PACS_OrbitMessages.h
Source/<GameModule>/PACS/Heli/PACS_CandidateHelicopterData.h
Source/<GameModule>/PACS/Heli/PACS_CandidateHelicopterData.cpp
Source/<GameModule>/PACS/Heli/PACS_HeliSavedMove.h
Source/<GameModule>/PACS/Heli/PACS_HeliSavedMove.cpp
Source/<GameModule>/PACS/Heli/PACS_HeliMovementComponent.h
Source/<GameModule>/PACS/Heli/PACS_HeliMovementComponent.cpp
Source/<GameModule>/PACS/Heli/PACS_CandidateHelicopterCharacter.h
Source/<GameModule>/PACS/Heli/PACS_CandidateHelicopterCharacter.cpp
Source/<GameModule>/PACS/Heli/PACS_AssessorFollowComponent.h
Source/<GameModule>/PACS/Heli/PACS_AssessorFollowComponent.cpp
Source/<GameModule>/PACS/VR/PACS_VRSeatSave.h
Source/<GameModule>/PACS/VR/PACS_VRSeatSave.cpp
Source/<GameModule>/PACS/VR/PACS_UserSave.h
Source/<GameModule>/PACS/VR/PACS_UserSave.cpp
```

Do not change file names or locations.

---

## 2) Code to place in each file

### 2.1 `PACS_OrbitMessages.h`
```cpp
#pragma once
#include "CoreMinimal.h"
#include "PACS_OrbitMessages.generated.h"

UENUM()
enum class EPACS_AnchorPolicy : uint8
{
    PreserveAngleOnce = 0,
    ResetAngleNow     = 1
};

USTRUCT()
struct FPACS_OrbitEdit
{
    GENERATED_BODY()

    UPROPERTY() bool bHasCenter=false;  UPROPERTY() FVector_NetQuantize100 NewCenterCm = FVector::ZeroVector;
    UPROPERTY() bool bHasAlt=false;     UPROPERTY() float NewAltCm=0.f;
    UPROPERTY() bool bHasRadius=false;  UPROPERTY() float NewRadiusCm=0.f;
    UPROPERTY() bool bHasSpeed=false;   UPROPERTY() float NewSpeedCms=0.f;

    UPROPERTY() float DurCenterS=0.f, DurAltS=0.f, DurRadiusS=0.f, DurSpeedS=0.f;

    UPROPERTY() uint16 TransactionId=0;
    UPROPERTY() EPACS_AnchorPolicy AnchorPolicy = EPACS_AnchorPolicy::PreserveAngleOnce;
};
```

---

### 2.2 `PACS_CandidateHelicopterData.h`
```cpp
#pragma once
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "PACS_CandidateHelicopterData.generated.h"

UCLASS(BlueprintType)
class UPACS_CandidateHelicopterData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) float DefaultAltitudeCm = 20000.f;   // 200 m
    UPROPERTY(EditDefaultsOnly) float DefaultRadiusCm   = 15000.f;   // 150 m
    UPROPERTY(EditDefaultsOnly) float DefaultSpeedCms   = 2222.22f;  // 80 km/h
    UPROPERTY(EditDefaultsOnly) float MaxSpeedCms       = 5555.56f;  // 200 km/h

    UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0", ClampMax="10")) float MaxBankDeg = 10.f;

    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> CenterInterp = nullptr;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> AltInterp    = nullptr;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> RadiusInterp = nullptr;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> SpeedInterp  = nullptr;

    UPROPERTY(EditDefaultsOnly) float CenterDurS = 4.f;
    UPROPERTY(EditDefaultsOnly) float AltDurS    = 3.f;
    UPROPERTY(EditDefaultsOnly) float RadiusDurS = 3.f;
    UPROPERTY(EditDefaultsOnly) float SpeedDurS  = 2.f;

    UPROPERTY(EditDefaultsOnly) float MaxCenterDriftCms = 3000.f; // drift clamp for visuals

    UPROPERTY(EditDefaultsOnly) FVector SeatLocalClamp = FVector(50,50,50);

    UPROPERTY(EditDefaultsOnly) FTransform AssessorFollowView;
};
```

#### `PACS_CandidateHelicopterData.cpp`
```cpp
#include "PACS_CandidateHelicopterData.h"
```

---

### 2.3 `PACS_HeliSavedMove.h`
```cpp
#pragma once
#include "GameFramework/CharacterMovementComponent.h"

struct FSavedMove_HeliOrbit : public FSavedMove_Character
{
    float   SavedAngleRad = 0.f;
    FVector SavedCenterCm = FVector::ZeroVector;
    uint8   SavedOrbitVersion = 0;

    virtual void Clear() override
    {
        FSavedMove_Character::Clear();
        SavedAngleRad = 0.f; SavedCenterCm = FVector::ZeroVector; SavedOrbitVersion = 0;
    }

    virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
                            FNetworkPredictionData_Client_Character& ClientData) override;

    virtual void PrepMoveFor(ACharacter* C) override;

    virtual bool CanCombineWith(const FSavedMove_Character* NewMove, ACharacter* C, float MaxDelta) const override
    {
        const FSavedMove_HeliOrbit* H = static_cast<const FSavedMove_HeliOrbit*>(NewMove);
        const bool SameVersion = (SavedOrbitVersion == H->SavedOrbitVersion);
        const bool SameCentre  = SavedCenterCm.Equals(H->SavedCenterCm, 1.f);
        return SameVersion && SameCentre && FSavedMove_Character::CanCombineWith(NewMove, C, MaxDelta);
    }
};

struct FNetworkPredictionData_Client_HeliOrbit : public FNetworkPredictionData_Client_Character
{
    FNetworkPredictionData_Client_HeliOrbit(const UCharacterMovementComponent& ClientMovement)
    : FNetworkPredictionData_Client_Character(ClientMovement) {}

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FSavedMove_HeliOrbit());
    }
};
```

#### `PACS_HeliSavedMove.cpp`
```cpp
#include "PACS_HeliSavedMove.h"
#include "PACS_CandidateHelicopterCharacter.h"
#include "PACS_HeliMovementComponent.h"

void FSavedMove_HeliOrbit::SetMoveFor(ACharacter* C, float InDeltaTime, const FVector& NewAccel,
                                      FNetworkPredictionData_Client_Character& ClientData)
{
    FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
    if (const APACS_CandidateHelicopterCharacter* H = Cast<APACS_CandidateHelicopterCharacter>(C))
    {
        const UPACS_HeliMovementComponent* MC = CastChecked<UPACS_HeliMovementComponent>(H->GetCharacterMovement());
        SavedAngleRad     = MC->AngleRad;
        SavedCenterCm     = MC->CenterCm;
        SavedOrbitVersion = H->OrbitParamsVersion;
    }
}

void FSavedMove_HeliOrbit::PrepMoveFor(ACharacter* C)
{
    FSavedMove_Character::PrepMoveFor(C);
}
```

---

### 2.4 `PACS_HeliMovementComponent.h`
```cpp
#pragma once
#include "GameFramework/CharacterMovementComponent.h"
#include "PACS_HeliMovementComponent.generated.h"

UENUM()
enum class EPACS_HeliMoveMode : uint8 { CMOVE_HeliOrbit = 0 };

UCLASS()
class UPACS_HeliMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    FVector CenterCm = FVector::ZeroVector;
    float   AltitudeCm = 0.f, RadiusCm = 1.f, SpeedCms = 0.f;
    float   AngleRad = 0.f;
    float   ServerNowS = 0.f;
    float   LastPlaneZ = TNumericLimits<float>::Lowest();

    UPROPERTY(EditDefaultsOnly) TObjectPtr<class UPACS_CandidateHelicopterData> Data;

    virtual void OnRegister() override;
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

    void TickClock_Server();
    void TickClock_Client();
    void TickClock_Proxy();

    void Eval_Server();
    void Eval_Client();
    void Eval_Proxy();

    void UpdateAngle_Server();
    void UpdateAngle_Client();
    void UpdateAngle_Proxy();

    void ApplyAltitudePlane();
    void StepKinematics(float Dt);

private:
    mutable FNetworkPredictionData_Client* ClientPredictionData = nullptr;
};
```

#### `PACS_HeliMovementComponent.cpp`
```cpp
#include "PACS_HeliMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "PACS_CandidateHelicopterCharacter.h"
#include "PACS_CandidateHelicopterData.h"
#include "PACS_HeliSavedMove.h"
#include "Curves/CurveFloat.h"

static float Eval01(float StartS, float DurS, UCurveFloat* Curve, float NowS)
{
    if (DurS <= 0.f) return 1.f;
    const float T = FMath::Clamp((NowS - StartS)/DurS, 0.f, 1.f);
    return Curve ? Curve->GetFloatValue(T) : T;
}

void UPACS_HeliMovementComponent::OnRegister()
{
    Super::OnRegister();
    SetMovementMode(MOVE_Custom, (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit);
    bConstrainToPlane = true;
}

void UPACS_HeliMovementComponent::OnMovementModeChanged(EMovementMode Prev, uint8 PrevCustom)
{
    Super::OnMovementModeChanged(Prev, PrevCustom);
    if (MovementMode == MOVE_Custom && CustomMovementMode == (uint8)EPACS_HeliMoveMode::CMOVE_HeliOrbit)
    {
        LastPlaneZ = TNumericLimits<float>::Lowest();
    }
}

FNetworkPredictionData_Client* UPACS_HeliMovementComponent::GetPredictionData_Client() const
{
    check(PawnOwner);
    if (!ClientPredictionData)
    {
        UPACS_HeliMovementComponent* Mutable = const_cast<UPACS_HeliMovementComponent*>(this);
        Mutable->ClientPredictionData = new FNetworkPredictionData_Client_HeliOrbit(*this);
        Mutable->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
        Mutable->ClientPredictionData->NoSmoothNetUpdateDist  = 140.f;
    }
    return ClientPredictionData;
}

void UPACS_HeliMovementComponent::PhysCustom(float Dt, int32 Iter)
{
    const ENetRole Role = CharacterOwner ? CharacterOwner->GetLocalRole() : ROLE_None;

    if (Role == ROLE_Authority)
    {
        TickClock_Server();   Eval_Server();   ApplyAltitudePlane();   UpdateAngle_Server();   StepKinematics(Dt);
        return;
    }
    if (Role == ROLE_AutonomousProxy)
    {
        TickClock_Client();   Eval_Client();   ApplyAltitudePlane();   UpdateAngle_Client();   StepKinematics(Dt);
        return;
    }

    TickClock_Proxy();        Eval_Proxy();    ApplyAltitudePlane();   UpdateAngle_Proxy();    StepKinematics(Dt);
}

void UPACS_HeliMovementComponent::TickClock_Server()
{
    if (const AGameStateBase* GS = GetWorld()->GetGameState())
        ServerNowS = GS->GetServerWorldTimeSeconds();
    else
        ServerNowS = GetWorld()->GetTimeSeconds();
}

void UPACS_HeliMovementComponent::TickClock_Client() { TickClock_Server(); }
void UPACS_HeliMovementComponent::TickClock_Proxy()  { TickClock_Server(); }

void UPACS_HeliMovementComponent::Eval_Server()
{
    const APACS_CandidateHelicopterCharacter* C = Cast<APACS_CandidateHelicopterCharacter>(CharacterOwner);
    if (!C || !Data) return;

    const float aC = Eval01(C->OrbitAnchors.CenterStartS, (C->OrbitTargets.CenterDurS>0?C->OrbitTargets.CenterDurS:Data->CenterDurS), Data->CenterInterp, ServerNowS);
    const float aA = Eval01(C->OrbitAnchors.AltStartS,    (C->OrbitTargets.AltDurS>0?C->OrbitTargets.AltDurS:Data->AltDurS),          Data->AltInterp,    ServerNowS);
    const float aR = Eval01(C->OrbitAnchors.RadiusStartS, (C->OrbitTargets.RadiusDurS>0?C->OrbitTargets.RadiusDurS:Data->RadiusDurS), Data->RadiusInterp, ServerNowS);
    const float aS = Eval01(C->OrbitAnchors.SpeedStartS,  (C->OrbitTargets.SpeedDurS>0?C->OrbitTargets.SpeedDurS:Data->SpeedDurS),    Data->SpeedInterp,  ServerNowS);

    const FVector DesiredC = FMath::Lerp(CenterCm, C->OrbitTargets.CenterCm, aC);
    const float   MaxStep  = (Data->MaxCenterDriftCms) * FApp::GetDeltaTime();
    CenterCm += (DesiredC - CenterCm).GetClampedToMaxSize(MaxStep);

    AltitudeCm = FMath::Lerp(AltitudeCm, C->OrbitTargets.AltitudeCm, aA);
    RadiusCm   = FMath::Max(FMath::Lerp(RadiusCm,   C->OrbitTargets.RadiusCm,   aR), 1.f);
    SpeedCms   = FMath::Clamp(FMath::Lerp(SpeedCms, C->OrbitTargets.SpeedCms,   aS), 0.f, Data->MaxSpeedCms);
}

void UPACS_HeliMovementComponent::Eval_Client() { Eval_Server(); }
void UPACS_HeliMovementComponent::Eval_Proxy()  { Eval_Server(); }

void UPACS_HeliMovementComponent::UpdateAngle_Server()
{
    const APACS_CandidateHelicopterCharacter* C = Cast<APACS_CandidateHelicopterCharacter>(CharacterOwner);
    if (!C || SpeedCms <= KINDA_SMALL_NUMBER || RadiusCm <= 1.f) return;
    const float Omega = SpeedCms / RadiusCm;
    AngleRad = FMath::UnwindRadians(C->OrbitAnchors.AngleAtStart + Omega * (ServerNowS - C->OrbitAnchors.OrbitStartS));
}

void UPACS_HeliMovementComponent::UpdateAngle_Client() { UpdateAngle_Server(); }
void UPACS_HeliMovementComponent::UpdateAngle_Proxy()  { UpdateAngle_Server(); }

void UPACS_HeliMovementComponent::ApplyAltitudePlane()
{
    if (!FMath::IsNearlyEqual(LastPlaneZ, AltitudeCm, 0.1f))
    {
        SetPlaneConstraintEnabled(true);
        PlaneConstraintNormal = FVector::UpVector;
        PlaneConstraintOrigin = FVector(0,0,AltitudeCm);
        LastPlaneZ = AltitudeCm;
    }
}

void UPACS_HeliMovementComponent::StepKinematics(float Dt)
{
    const float s = FMath::Sin(AngleRad), c = FMath::Cos(AngleRad);
    const FVector Tangent(-s, c, 0.f);
    Velocity = Tangent * SpeedCms;
    const FRotator Yaw = Tangent.ToOrientationRotator();

    FHitResult Hit;
    SafeMoveUpdatedComponent(Velocity * Dt, Yaw, /*bSweep=*/true, Hit);
    if (Hit.IsValidBlockingHit())
    {
        SlideAlongSurface(Velocity * Dt, 1.f - Hit.Time, Hit.Normal, Hit, true);
    }
}
```

---

### 2.5 `PACS_CandidateHelicopterCharacter.h`
```cpp
#pragma once
#include "GameFramework/Character.h"
#include "PACS_CandidateHelicopterCharacter.generated.h"

class UPACS_HeliMovementComponent;
class UPACS_CandidateHelicopterData;
class UCameraComponent;

USTRUCT()
struct FPACS_OrbitTargets
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantize100 CenterCm = FVector::ZeroVector;
    UPROPERTY() float AltitudeCm = 0.f;
    UPROPERTY() float RadiusCm   = 1.f;
    UPROPERTY() float SpeedCms   = 0.f;

    UPROPERTY() float CenterDurS = 0.f;
    UPROPERTY() float AltDurS    = 0.f;
    UPROPERTY() float RadiusDurS = 0.f;
    UPROPERTY() float SpeedDurS  = 0.f;
};

USTRUCT()
struct FPACS_OrbitAnchors
{
    GENERATED_BODY()
    UPROPERTY() float CenterStartS = 0.f, AltStartS = 0.f, RadiusStartS = 0.f, SpeedStartS = 0.f;
    UPROPERTY() float OrbitStartS  = 0.f;
    UPROPERTY() float AngleAtStart = 0.f;
};

USTRUCT(BlueprintType)
struct FPACS_OrbitOffsets
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasAltOffset=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasRadiusOffset=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasSpeedOffset=false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AltitudeDeltaCm=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RadiusDeltaCm=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpeedDeltaCms=0.f;
};

UCLASS()
class APACS_CandidateHelicopterCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    APACS_CandidateHelicopterCharacter(const FObjectInitializer& OI);

    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* HelicopterFrame;
    UPROPERTY(VisibleAnywhere) USceneComponent*      CockpitRoot;
    UPROPERTY(VisibleAnywhere) USceneComponent*      SeatOriginRef;
    UPROPERTY(VisibleAnywhere) USceneComponent*      SeatOffsetRoot;
    UPROPERTY(VisibleAnywhere) UCameraComponent*     VRCamera;

    UPROPERTY() UPACS_HeliMovementComponent* HeliCMC;
    UPROPERTY(EditDefaultsOnly) UPACS_CandidateHelicopterData* Data;

    UPROPERTY(ReplicatedUsing=OnRep_OrbitTargets) FPACS_OrbitTargets OrbitTargets;
    UPROPERTY(ReplicatedUsing=OnRep_OrbitAnchors) FPACS_OrbitAnchors OrbitAnchors;
    UPROPERTY(ReplicatedUsing=OnRep_SelectedBy)   APlayerState* SelectedBy = nullptr;

    UPROPERTY(Replicated) uint8  OrbitParamsVersion = 0;
    UPROPERTY()          uint16 LastAppliedTxnId    = 0;

    UPROPERTY(Transient) FVector SeatLocalOffsetCm = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly) float BankInterpSpeed = 6.f;
    UPROPERTY(Transient) float CurrentBankDeg = 0.f;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable) void CenterSeatedPose(bool bSnapYawToVehicleForward = true);
    void NudgeSeatX(float Sign); void NudgeSeatY(float Sign); void NudgeSeatZ(float Sign);
    void ApplySeatOffset(); void ZeroSeatChain();

    UFUNCTION(Server, Reliable) void Server_ApplyOrbitParams(const struct FPACS_OrbitEdit& Edit);

    UFUNCTION(Server, Reliable) void Server_RequestSelect(APlayerState* Requestor);
    UFUNCTION(Server, Reliable) void Server_ReleaseSelect(APlayerState* Requestor);

    void ApplyOffsetsThenSeed(const FPACS_OrbitOffsets* Off);

protected:
    UFUNCTION() void OnRep_OrbitTargets();
    UFUNCTION() void OnRep_OrbitAnchors();
    UFUNCTION() void OnRep_SelectedBy();

    bool ValidateOrbitCenter(const FVector& Proposed) const;
    void UpdateBankVisual(float DeltaSeconds);
};
```

#### `PACS_CandidateHelicopterCharacter.cpp`
```cpp
#include "PACS_CandidateHelicopterCharacter.h"
#include "PACS_HeliMovementComponent.h"
#include "PACS_CandidateHelicopterData.h"
#include "PACS_OrbitMessages.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

APACS_CandidateHelicopterCharacter::APACS_CandidateHelicopterCharacter(const FObjectInitializer& OI)
: Super(OI.SetDefaultSubobjectClass<UPACS_HeliMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    HelicopterFrame = CreateDefaultSubobject<UStaticMeshComponent>("HelicopterFrame");
    HelicopterFrame->SetupAttachment(RootComponent);

    CockpitRoot   = CreateDefaultSubobject<USceneComponent>("CockpitRoot");
    CockpitRoot->SetupAttachment(RootComponent);

    SeatOriginRef = CreateDefaultSubobject<USceneComponent>("SeatOriginRef");
    SeatOriginRef->SetupAttachment(CockpitRoot);

    SeatOffsetRoot = CreateDefaultSubobject<USceneComponent>("SeatOffsetRoot");
    SeatOffsetRoot->SetupAttachment(SeatOriginRef);

    VRCamera = CreateDefaultSubobject<UCameraComponent>("VRCamera");
    VRCamera->SetupAttachment(SeatOffsetRoot);
    VRCamera->bLockToHmd = true;

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 180.f, 0);
}

void APACS_CandidateHelicopterCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void APACS_CandidateHelicopterCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateBankVisual(DeltaSeconds);
}

void APACS_CandidateHelicopterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APACS_CandidateHelicopterCharacter, OrbitTargets);
    DOREPLIFETIME(APACS_CandidateHelicopterCharacter, OrbitAnchors);
    DOREPLIFETIME(APACS_CandidateHelicopterCharacter, SelectedBy);
    DOREPLIFETIME(APACS_CandidateHelicopterCharacter, OrbitParamsVersion);
}

static float NowS(const UWorld* W)
{
    if (const AGameStateBase* GS = W->GetGameState()) return GS->GetServerWorldTimeSeconds();
    return W->GetTimeSeconds();
}

// ----- VR Seat -----
void APACS_CandidateHelicopterCharacter::CenterSeatedPose(bool bSnapYawToVehicleForward)
{
    ZeroSeatChain();
    UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(
        bSnapYawToVehicleForward ? GetActorRotation().Yaw : 0.f,
        EOrientPositionSelector::OrientationAndPosition);
    ApplySeatOffset();
}
void APACS_CandidateHelicopterCharacter::ZeroSeatChain()
{
    SeatOriginRef->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    SeatOffsetRoot->SetRelativeLocationAndRotation(SeatLocalOffsetCm, FRotator::ZeroRotator);
}
void APACS_CandidateHelicopterCharacter::ApplySeatOffset()
{
    if (const UPACS_CandidateHelicopterData* D = Data)
    {
        SeatLocalOffsetCm.X = FMath::Clamp(SeatLocalOffsetCm.X, -D->SeatLocalClamp.X, D->SeatLocalClamp.X);
        SeatLocalOffsetCm.Y = FMath::Clamp(SeatLocalOffsetCm.Y, -D->SeatLocalClamp.Y, D->SeatLocalClamp.Y);
        SeatLocalOffsetCm.Z = FMath::Clamp(SeatLocalOffsetCm.Z, -D->SeatLocalClamp.Z, D->SeatLocalClamp.Z);
    }
    SeatOffsetRoot->SetRelativeLocation(SeatLocalOffsetCm);
}
void APACS_CandidateHelicopterCharacter::NudgeSeatX(float S){ SeatLocalOffsetCm.X += S; ApplySeatOffset(); }
void APACS_CandidateHelicopterCharacter::NudgeSeatY(float S){ SeatLocalOffsetCm.Y += S; ApplySeatOffset(); }
void APACS_CandidateHelicopterCharacter::NudgeSeatZ(float S){ SeatLocalOffsetCm.Z += S; ApplySeatOffset(); }

// ----- Banking (visual only) -----
void APACS_CandidateHelicopterCharacter::UpdateBankVisual(float Dt)
{
    const auto* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement());
    if (!CMC || !Data || !HelicopterFrame) return;
    const float Target = -(CMC->SpeedCms / FMath::Max(Data->MaxSpeedCms,1.f)) * Data->MaxBankDeg;
    CurrentBankDeg = FMath::FInterpTo(CurrentBankDeg, Target, Dt, BankInterpSpeed);
    HelicopterFrame->SetRelativeRotation(FRotator(0, 0, CurrentBankDeg));
}

// ----- Param Validation -----
bool APACS_CandidateHelicopterCharacter::ValidateOrbitCenter(const FVector& Proposed) const
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(OrbitCenter), false, this);
    const FCollisionShape Probe = FCollisionShape::MakeSphere(50.f);
    return !GetWorld()->OverlapAnyTestByChannel(Proposed, FQuat::Identity, ECC_WorldStatic, Probe, Q);
}

// ----- Reliable batched edits -----
void APACS_CandidateHelicopterCharacter::Server_ApplyOrbitParams(const FPACS_OrbitEdit& E)
{
    if (!HasAuthority() || (SelectedBy == nullptr)) return;
    if (E.TransactionId <= LastAppliedTxnId) return;
    LastAppliedTxnId = E.TransactionId;

    if (E.bHasCenter && !ValidateOrbitCenter(E.NewCenterCm)) return;
    if (E.bHasCenter) OrbitTargets.CenterCm = E.NewCenterCm;
    if (E.bHasAlt)    OrbitTargets.AltitudeCm = FMath::Clamp(E.NewAltCm, 100.f, 1'000'000.f);
    if (E.bHasRadius) OrbitTargets.RadiusCm   = FMath::Clamp(E.NewRadiusCm, 100.f, 1'000'000.f);
    if (E.bHasSpeed)  OrbitTargets.SpeedCms   = FMath::Clamp(E.NewSpeedCms, 0.f, Data ? Data->MaxSpeedCms : 6000.f);

    OrbitTargets.CenterDurS = E.DurCenterS > 0 ? E.DurCenterS : (Data ? Data->CenterDurS : 0.f);
    OrbitTargets.AltDurS    = E.DurAltS    > 0 ? E.DurAltS    : (Data ? Data->AltDurS    : 0.f);
    OrbitTargets.RadiusDurS = E.DurRadiusS > 0 ? E.DurRadiusS : (Data ? Data->RadiusDurS : 0.f);
    OrbitTargets.SpeedDurS  = E.DurSpeedS  > 0 ? E.DurSpeedS  : (Data ? Data->SpeedDurS  : 0.f);

    const float S = NowS(GetWorld());
    if (E.bHasCenter) OrbitAnchors.CenterStartS = S;
    if (E.bHasAlt)    OrbitAnchors.AltStartS    = S;
    if (E.bHasRadius) OrbitAnchors.RadiusStartS = S;
    if (E.bHasSpeed)  OrbitAnchors.SpeedStartS  = S;

    if (E.AnchorPolicy == EPACS_AnchorPolicy::PreserveAngleOnce)
    {
        const auto* CMC = CastChecked<UPACS_HeliMovementComponent>(GetCharacterMovement());
        OrbitAnchors.AngleAtStart = FMath::UnwindRadians(CMC->AngleRad);
        OrbitAnchors.OrbitStartS  = S;
    }
    else
    {
        OrbitAnchors.AngleAtStart = 0.f;
        OrbitAnchors.OrbitStartS  = S;
    }

    ++OrbitParamsVersion;
    ForceNetUpdate();
}

void APACS_CandidateHelicopterCharacter::Server_RequestSelect(APlayerState* Requestor)
{
    if (!HasAuthority()) return;
    if (SelectedBy == nullptr) SelectedBy = Requestor;
}

void APACS_CandidateHelicopterCharacter::Server_ReleaseSelect(APlayerState* Requestor)
{
    if (!HasAuthority()) return;
    if (SelectedBy == Requestor) SelectedBy = nullptr;
}

void APACS_CandidateHelicopterCharacter::ApplyOffsetsThenSeed(const FPACS_OrbitOffsets* Off)
{
    const float S = NowS(GetWorld());

    float Alt = Data ? Data->DefaultAltitudeCm : 20000.f;
    float Rad = Data ? Data->DefaultRadiusCm   : 15000.f;
    float Spd = Data ? Data->DefaultSpeedCms   : 2222.22f;

    if (Off)
    {
        if (Off->bHasAltOffset)    Alt += Off->AltitudeDeltaCm;
        if (Off->bHasRadiusOffset) Rad += Off->RadiusDeltaCm;
        if (Off->bHasSpeedOffset)  Spd += Off->SpeedDeltaCms;
    }

    OrbitTargets.CenterCm   = FVector(GetActorLocation().X, GetActorLocation().Y, 0.f);
    OrbitTargets.AltitudeCm = FMath::Max(Alt, 100.f);
    OrbitTargets.RadiusCm   = FMath::Max(Rad, 100.f);
    OrbitTargets.SpeedCms   = FMath::Clamp(Spd, 0.f, Data ? Data->MaxSpeedCms : 6000.f);
    OrbitTargets.CenterDurS = OrbitTargets.AltDurS = OrbitTargets.RadiusDurS = OrbitTargets.SpeedDurS = 0.f;

    OrbitAnchors.CenterStartS = OrbitAnchors.AltStartS = OrbitAnchors.RadiusStartS =
    OrbitAnchors.SpeedStartS  = OrbitAnchors.OrbitStartS = S;
    OrbitAnchors.AngleAtStart = 0.f;

    const FVector StartPos = FVector(OrbitTargets.CenterCm.X,
                                     OrbitTargets.CenterCm.Y + OrbitTargets.RadiusCm,
                                     OrbitTargets.AltitudeCm);
    SetActorLocation(StartPos, /*bSweep=*/false);

    if (auto* CMC = Cast<UPACS_HeliMovementComponent>(GetCharacterMovement()))
    {
        CMC->CenterCm   = OrbitTargets.CenterCm;
        CMC->AltitudeCm = OrbitTargets.AltitudeCm;
        CMC->RadiusCm   = OrbitTargets.RadiusCm;
        CMC->SpeedCms   = OrbitTargets.SpeedCms;
        CMC->AngleRad   = 0.f;
    }
}

void APACS_CandidateHelicopterCharacter::OnRep_OrbitTargets(){}
void APACS_CandidateHelicopterCharacter::OnRep_OrbitAnchors(){}
void APACS_CandidateHelicopterCharacter::OnRep_SelectedBy(){}
```

---

### 2.6 `PACS_AssessorFollowComponent.h`
```cpp
#pragma once
#include "Components/ActorComponent.h"
#include "PACS_AssessorFollowComponent.generated.h"

UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class UPACS_AssessorFollowComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) float FollowInterpSpeed = 4.f;
    UPROPERTY(EditAnywhere) FVector WorldOffset = FVector(-2000, 1200, 800);

    UPROPERTY() TWeakObjectPtr<class APACS_CandidateHelicopterCharacter> Target;

    virtual void BeginPlay() override;
    virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(Client, Reliable) void ClientBeginFollow(APACS_CandidateHelicopterCharacter* InTarget);
    UFUNCTION(Client, Reliable) void ClientEndFollow();
};
```

#### `PACS_AssessorFollowComponent.cpp`
```cpp
#include "PACS_AssessorFollowComponent.h"
#include "PACS_CandidateHelicopterCharacter.h"

void UPACS_AssessorFollowComponent::BeginPlay()
{
    Super::BeginPlay();
    SetComponentTickEnabled(false);
}

void UPACS_AssessorFollowComponent::ClientBeginFollow(APACS_CandidateHelicopterCharacter* InTarget)
{
    Target = InTarget;
    SetComponentTickEnabled(true);
}

void UPACS_AssessorFollowComponent::ClientEndFollow()
{
    Target.Reset();
    SetComponentTickEnabled(false);
}

void UPACS_AssessorFollowComponent::TickComponent(float Dt, ELevelTick, FActorComponentTickFunction*)
{
    if (!Target.IsValid()) return;
    const FVector Desired = Target->GetActorTransform().TransformPositionNoScale(WorldOffset);
    const FVector NewLoc = FMath::VInterpTo(GetOwner()->GetActorLocation(), Desired, Dt, FollowInterpSpeed);
    const FRotator LookAt = (Target->GetActorLocation() - GetOwner()->GetActorLocation()).Rotation();
    const FRotator NewRot = FMath::RInterpTo(GetOwner()->GetActorRotation(), LookAt, Dt, FollowInterpSpeed);
    GetOwner()->SetActorLocationAndRotation(NewLoc, NewRot, false);
}
```

---

### 2.7 `PACS_VRSeatSave.h`
```cpp
#pragma once
#include "PACS_VRSeatSave.generated.h"

USTRUCT(BlueprintType)
struct FPACS_VRSeatSave
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantize10 SeatLocalOffsetCm = FVector::ZeroVector;
};
```

#### `PACS_VRSeatSave.cpp`
```cpp
#include "PACS_VRSeatSave.h"
```

### 2.8 `PACS_UserSave.h`
```cpp
#pragma once
#include "GameFramework/SaveGame.h"
#include "PACS_VRSeatSave.h"
#include "PACS_UserSave.generated.h"

UCLASS(BlueprintType)
class UPACS_UserSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() TMap<FString, FPACS_VRSeatSave> SeatByPlayer;
};
```

#### `PACS_UserSave.cpp`
```cpp
#include "PACS_UserSave.h"
```

---

## 3) Edits to existing files (concise and exact)

### 3.1 `APACS_PlayerController`

Add members:
```cpp
FDelegateHandle OnPutOnHandle, OnRemovedHandle, OnRecenterHandle;
void HandleHMDPutOn();
void HandleHMDRemoved();
void HandleHMDRecenter();
```

In `BeginPlay()` for **local** controllers only:
```cpp
if (IsLocalController())
{
    OnPutOnHandle   = FCoreDelegates::VRHeadsetPutOnHead.AddUObject(this, &ThisClass::HandleHMDPutOn);
    OnRemovedHandle = FCoreDelegates::VRHeadsetRemovedFromHead.AddUObject(this, &ThisClass::HandleHMDRemoved);
    OnRecenterHandle= FCoreDelegates::VRHeadsetRecenter.AddUObject(this, &ThisClass::HandleHMDRecenter);
}
```

Handlers:
```cpp
void APACS_PlayerController::HandleHMDPutOn()
{
    if (auto* Heli = Cast<APACS_CandidateHelicopterCharacter>(GetPawn()))
        Heli->CenterSeatedPose(true);
}
void APACS_PlayerController::HandleHMDRecenter(){ HandleHMDPutOn(); }
void APACS_PlayerController::HandleHMDRemoved(){}
```

In `EndPlay()`:
```cpp
FCoreDelegates::VRHeadsetPutOnHead.Remove(OnPutOnHandle);
FCoreDelegates::VRHeadsetRemovedFromHead.Remove(OnRemovedHandle);
FCoreDelegates::VRHeadsetRecenter.Remove(OnRecenterHandle);
```

### 3.2 Enhanced Input

Create actions:
```
IA_VRSeatCenter, IA_VRSeatXPlus, IA_VRSeatXMinus,
IA_VRSeatYPlus, IA_VRSeatYMinus, IA_VRSeatZPlus, IA_VRSeatZMinus
```
Create mapping context `IMC_VRSeat`. On possess, add the context via `UEnhancedInputLocalPlayerSubsystem`. Route actions through `UPACS_InputHandlerComponent` to call:
```
CenterSeatedPose()
NudgeSeatX(+/-1), NudgeSeatY(+/-1), NudgeSeatZ(+/-1)
```

---

## 4) Acceptance criteria

1) VR camera does not bank; only `HelicopterFrame` banks (visual only).  
2) Orbit follows anti‑clockwise tangent; bank in [0, MaxBankDeg] and smoothly interpolates.  
3) Linear speed remains constant when radius changes; angular rate adapts.  
4) Curve‑based transitions use **server time**.  
5) No visible jitter for VR owner; proxies smooth linearly.  
6) Single reliable batched edit RPC with transaction id and anchor policy; no parameter packet is lost.  
7) Movement is **swept** and slides along boundaries; orbit centre is validated.  
8) Angle is unwound regularly to avoid precision loss.  
9) Late joiner snaps once if initial error > tolerance; otherwise smooths.  
10) Save offsets can be applied on spawn via `ApplyOffsetsThenSeed`. The pawn contains no save IO logic.

Do not add features beyond the above.

---

## 5) Minimal test checklist

- Spawn candidate with **no** save offsets → values equal Data Asset defaults.  
- Apply offsets (+5000 cm radius, +500 cm altitude, +500 cm/s speed) → starting state equals defaults plus offsets.  
- Change radius only → linear speed stays fixed; lap time changes; no velocity spikes.  
- Boundary volume present → pawn slides, not tunnels.  
- Put on HMD or recenter → seated pose recenters; camera remains level.  
- Rapid UI edits (batched) → single reliable RPC observed; no double re‑anchor jumps.  
- Late joiner connects → one snap at most, then smooth.

---

# Setup instructions for designers

Follow these steps in order. These are asset and Blueprint configuration steps only.

A) Data Asset  
1. Create **PACS_CandidateHelicopterData** via Content Browser.  
2. Set: `DefaultAltitudeCm`, `DefaultRadiusCm`, `DefaultSpeedCms`, `MaxBankDeg`.  
3. Optionally set easing curves and durations.  
4. Keep `SeatLocalClamp` at ±50 cm unless cockpit requires otherwise.

B) Candidate Character Blueprint (based on `APACS_CandidateHelicopterCharacter`)  
1. Add **Static Mesh** or **Skeletal Mesh** named `HelicopterFrame`; attach to **RootComponent**.  
2. Verify hierarchy:
```
RootComponent (Capsule)
├─ HelicopterFrame            ← this banks visually
├─ CockpitRoot
   └─ SeatOriginRef
      └─ SeatOffsetRoot
         └─ VRCamera (bLockToHmd=true)  ← stays level
```
3. Assign your data asset to `Data`.  
4. Character Movement: `Orient Rotation to Movement = true`, `Network Smoothing Mode = Linear`.  
5. Ensure collision profiles block your boundary volumes.

C) Assessor Follow (optional)  
- Add `UPACS_AssessorFollowComponent` to the assessor pawn. No further setup required.

D) Enhanced Input (VR comfort)  
- Ensure `IMC_VRSeat` mapping context is pushed on possess.  
- Bind actions to seat recenter and micro‑nudges.

E) Tuning  
- Start with `MaxBankDeg = 6–8`.  
- Use ease‑in/out curves for Radius/Speed; avoid very short durations (<0.5 s).

F) Save Offsets  
- Your save system constructs `FPACS_OrbitOffsets` (any of Alt/Radius/Speed deltas).  
- After the server spawns the candidate pawn, call `ApplyOffsetsThenSeed(&Offsets)` (or `nullptr` for none).  
- Do not implement save IO inside the pawn.

End of document.


##Trouble Shooting

If the heli fails to orbit on beginplay ...

on a dedicated server you seed immediately after the pawn is spawned. In your APACSGameMode, do it right after the Super::HandleStartingNewPlayer_Implementation(...) call (and also in the timeout path), so the server sets Centre/Alt/Radius/Speed and stamps anchors on tick 1.

Drop-in snippet (minimal; no scope-creep):

// APACSGameMode.cpp
#include "PACS_CandidateHelicopterCharacter.h"  // for APACS_CandidateHelicopterCharacter
// #include "PACS_OrbitMessages.h"              // if you pass FPACS_OrbitOffsets (optional)

// ... inside APACSGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(NewPlayer))
{
    if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PACSPC->PlayerState))
    {
        if (PACSPS->HMDState != EHMDState::Unknown)
        {
            GetWorld()->GetTimerManager().ClearTimer(PACSPC->HMDWaitHandle);

            // Spawn now (server-side)
            Super::HandleStartingNewPlayer_Implementation(NewPlayer);

            // Seed orbit immediately after spawn (server authority)
            if (APACS_CandidateHelicopterCharacter* Candidate =
                    Cast<APACS_CandidateHelicopterCharacter>(NewPlayer->GetPawn()))
            {
                // If you have save offsets, pass a pointer; else pass nullptr.
                // const FPACS_OrbitOffsets* Offsets = SaveSubsystem ? &SaveSubsystem->GetOffsets() : nullptr;
                Candidate->ApplyOffsetsThenSeed(/*Offsets*/ nullptr);
            }
            return;
        }
        // else: timer path below…
    }

    // existing timer setup (unchanged) …
}


Also ensure the timeout path seeds after the forced spawn:

// APACSGameMode::OnHMDTimeout(...)
Super::HandleStartingNewPlayer_Implementation(PlayerController);

// Seed after forced spawn
if (APACS_CandidateHelicopterCharacter* Candidate =
        Cast<APACS_CandidateHelicopterCharacter>(PlayerController ? PlayerController->GetPawn() : nullptr))
{
    Candidate->ApplyOffsetsThenSeed(/*Offsets*/ nullptr);
}


That’s it. With this in place the pawn starts orbiting immediately after spawn/seed, using your Data Asset defaults (plus any save offsets if you wire them). It matches your existing GameMode flow that waits for HMD state, spawns, then proceeds.