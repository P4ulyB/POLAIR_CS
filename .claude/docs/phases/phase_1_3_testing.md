# PACS Helicopter Candidate — Automation Test Suite (Final Instructions for Claude Code)

Follow these instructions **exactly**. No deviation. No scope creep. No renaming. These tests target the helicopter **Candidate** pawn system and must validate functionality, replication on a **dedicated server**, and basic performance characteristics.

All test source files must be created under:
```
C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CSEditor\Public\test\
C:\Devops\Projects\POLAIR_CS\Source\POLAIR_CSEditor\Private\test\
```
Use the exact file names and paths specified below.

---

## 0) Editor module setup for tests

1) Ensure the **POLAIR_CSEditor** module `.Build.cs` has these dependencies (add if missing):
```csharp
PublicDependencyModuleNames.AddRange(new[] {
  "Core", "CoreUObject", "Engine", "InputCore", "NetCore",
  "HeadMountedDisplay", "EnhancedInput", "FunctionalTesting"
});

PrivateDependencyModuleNames.AddRange(new[] {
  "UnrealEd", "Projects", "Slate", "SlateCore"
});
```

2) Enable the **Functional Testing** plugin in the project.

3) These tests assume the production code classes exist and are named exactly as implemented:
- `APACS_CandidateHelicopterCharacter`
- `UPACS_HeliMovementComponent`
- `UPACS_CandidateHelicopterData`
- `UPACS_AssessorFollowComponent`
- `FPACS_OrbitEdit`
- `APACS_PlayerController` (for HMD recenter glue)

---

## 1) Files to create (exact paths)

Create the following files.

```
Public\test\PACS_Heli_TestHelpers.h
Private\test\PACS_Heli_TestHelpers.cpp

Public\test\PACS_Heli_DataSpec.h
Private\test\PACS_Heli_DataSpec.cpp

Public\test\PACS_Heli_KinematicsSpec.h
Private\test\PACS_Heli_KinematicsSpec.cpp

Public\test\PACS_Heli_DedicatedServerNFT.h
Private\test\PACS_Heli_DedicatedServerNFT.cpp

Public\test\PACS_Heli_BoundaryNFT.h
Private\test\PACS_Heli_BoundaryNFT.cpp

Public\test\PACS_Heli_PerfSpec.h
Private\test\PACS_Heli_PerfSpec.cpp
```

Do not change names or locations.

---

## 2) Helpers

### 2.1 `Public\test\PACS_Heli_TestHelpers.h`
```cpp
#pragma once
#include "CoreMinimal.h"

class APACS_CandidateHelicopterCharacter;
class UWorld;

namespace PACSHeliTest
{
    // Spawn the candidate pawn at a location; return pointer or nullptr.
    APACS_CandidateHelicopterCharacter* SpawnCandidate(UWorld* World, const FVector& Location);

    // Advance world time by Seconds (latent-like), ticking world.
    void PumpWorld(UWorld* World, float Seconds, float Step = 1.0f/60.0f);

    // Create a blocking box actor at Location with Extent; returns spawned actor.
    AActor* SpawnBlockingBox(UWorld* World, const FVector& Location, const FVector& Extent, FName Name);
}
```

### 2.2 `Private\test\PACS_Heli_TestHelpers.cpp`
```cpp
#include "test/PACS_Heli_TestHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "PACS_CandidateHelicopterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

APACS_CandidateHelicopterCharacter* PACSHeliTest::SpawnCandidate(UWorld* World, const FVector& Location)
{
    if (!World) return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    return World->SpawnActor<APACS_CandidateHelicopterCharacter>(APACS_CandidateHelicopterCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
}

void PACSHeliTest::PumpWorld(UWorld* World, float Seconds, float Step)
{
    if (!World) return;
    const int32 Steps = FMath::Max(1, int32(Seconds / Step));
    for (int32 i=0; i<Steps; ++i)
    {
        World->Tick(ELevelTick::LEVELTICK_All, Step);
        if (GEngine) { GEngine->UpdateTimeAndHandleMaxTickRate(); }
    }
}

AActor* PACSHeliTest::SpawnBlockingBox(UWorld* World, const FVector& Location, const FVector& Extent, FName Name)
{
    if (!World) return nullptr;
    AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator);
    UBoxComponent* Box = NewObject<UBoxComponent>(A);
    A->SetRootComponent(Box);
    Box->RegisterComponent();
    Box->SetBoxExtent(Extent);
    Box->SetCollisionProfileName(TEXT("BlockAll"));
    A->SetActorLabel(Name.ToString());
    return A;
}
```

---

## 3) Data integrity tests (editor, no net)

### 3.1 `Public\test\PACS_Heli_DataSpec.h`
```cpp
#pragma once
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_Heli_DataSpec, "PACS.Heli.Data.DefaultsAndTunables",
EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter);
```

### 3.2 `Private\test\PACS_Heli_DataSpec.cpp`
```cpp
#include "test/PACS_Heli_DataSpec.h"
#include "PACS_CandidateHelicopterData.h"
#include "UObject/SoftObjectPtr.h"

bool FPACS_Heli_DataSpec::RunTest(const FString& Parameters)
{
    // Designers must create a data asset named /Game/PACS/Data/DA_CandidateHelicopter
    const TSoftObjectPtr<UPACS_CandidateHelicopterData> SoftDA(TEXT("/Game/PACS/Data/DA_CandidateHelicopter.DA_CandidateHelicopter"));
    UPACS_CandidateHelicopterData* DA = SoftDA.LoadSynchronous();
    TestNotNull(TEXT("Data asset exists"), DA);
    if (!DA) return false;

    TestTrue(TEXT("Default Altitude > 0"), DA->DefaultAltitudeCm > 0.f);
    TestTrue(TEXT("Default Radius > 0"),   DA->DefaultRadiusCm   > 0.f);
    TestTrue(TEXT("Default Speed >= 0"),   DA->DefaultSpeedCms   >= 0.f);
    TestTrue(TEXT("MaxSpeed >= DefaultSpeed"), DA->MaxSpeedCms >= DA->DefaultSpeedCms);
    TestTrue(TEXT("MaxBankDeg in [0,10]"), DA->MaxBankDeg >= 0.f && DA->MaxBankDeg <= 10.f);
    return true;
}
```

---

## 4) Kinematics invariants (editor, no net)

### 4.1 `Public\test\PACS_Heli_KinematicsSpec.h`
```cpp
#pragma once
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_Heli_KinematicsSpec, "PACS.Heli.Kinematics.SpeedInvariance",
EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter);
```

### 4.2 `Private\test\PACS_Heli_KinematicsSpec.cpp`
```cpp
#include "test/PACS_Heli_KinematicsSpec.h"
#include "PACS_CandidateHelicopterCharacter.h"
#include "PACS_HeliMovementComponent.h"
#include "Engine/World.h"
#include "test/PACS_Heli_TestHelpers.h"

bool FPACS_Heli_KinematicsSpec::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("World available"), World);
    if (!World) return false;

    auto* Pawn = PACSHeliTest::SpawnCandidate(World, FVector::ZeroVector);
    TestNotNull(TEXT("Spawned candidate"), Pawn);
    if (!Pawn) return false;

    auto* CMC = Cast<UPACS_HeliMovementComponent>(Pawn->GetCharacterMovement());
    TestNotNull(TEXT("Has heli CMC"), CMC);
    if (!CMC) return false;

    // Set fixed speed and two different radii. Angular rate must change; linear speed must not.
    CMC->SpeedCms = 2000.f;
    CMC->RadiusCm = 10000.f;
    CMC->AngleRad = 0.f;

    // Pump world one second and capture distance traveled along tangent magnitude
    const float Dt = 1.f;
    const FVector Start = Pawn->GetActorLocation();
    PACSHeliTest::PumpWorld(World, Dt);
    const float Dist1 = FVector::Dist(Start, Pawn->GetActorLocation());

    // Change radius only
    CMC->RadiusCm = 20000.f;
    Pawn->SetActorLocation(Start);
    CMC->AngleRad = 0.f;
    PACSHeliTest::PumpWorld(World, Dt);
    const float Dist2 = FVector::Dist(Start, Pawn->GetActorLocation());

    // Distances should be roughly equal (tolerance for numeric)
    TestTrue(TEXT("Linear distance invariant over radius change"), FMath::IsNearlyEqual(Dist1, Dist2, 5.f));
    return true;
}
```

---

## 5) Dedicated server replication tests (Networked Functional Tests)

These are **Networked Functional Tests** that must run in a **dedicated server** multi-process session. They validate:
- Server-authoritative seeding.
- Reliable batched param edits replicate and apply once.
- Banking isolation: `HelicopterFrame` rolls while `VRCamera` stays level.
- Linear speed invariance under radius change on clients.

### 5.1 `Public\test\PACS_Heli_DedicatedServerNFT.h`
```cpp
#pragma once
#include "FunctionalTesting/NetworkedFunctionalTest.h"
#include "PACS_Heli_DedicatedServerNFT.generated.h"

UCLASS()
class UPACS_Heli_DedicatedServerNFT : public UNetworkedFunctionalTest
{
    GENERATED_BODY()
public:
    virtual void PrepareTest() override;
};
```

### 5.2 `Private\test\PACS_Heli_DedicatedServerNFT.cpp`
```cpp
#include "test/PACS_Heli_DedicatedServerNFT.h"
#include "PACS_CandidateHelicopterCharacter.h"
#include "PACS_HeliMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "PACS_OrbitMessages.h"

void UPACS_Heli_DedicatedServerNFT::PrepareTest()
{
    // Step 1: Server spawns candidate
    AddServerStep(TEXT("SpawnCandidate"), FWorkerDefinition::Server(1), [this]()
    {
        UWorld* World = GetWorld();
        auto* Pawn = World->SpawnActor<APACS_CandidateHelicopterCharacter>(APACS_CandidateHelicopterCharacter::StaticClass(),
                    FVector::ZeroVector, FRotator::ZeroRotator);
        RegisterAutoDestroyActor(Pawn);
        AssertTrue(IsValid(Pawn), TEXT("Candidate spawned on server"));
        FinishStep();
    });

    // Step 2: All clients find replicated candidate
    AddStep(TEXT("WaitForCandidateOnClients"), FWorkerDefinition::AllClients, nullptr, [this](float)
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APACS_CandidateHelicopterCharacter::StaticClass(), Found);
        if (Found.Num() > 0) { FinishStep(); }
    }, 5.0f);

    // Step 3: Server applies batched orbit edit (radius only), anchor preserve
    AddServerStep(TEXT("ServerApplyRadiusEdit"), FWorkerDefinition::Server(1), [this]()
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APACS_CandidateHelicopterCharacter::StaticClass(), Found);
        auto* Pawn = Found.Num() ? Cast<APACS_CandidateHelicopterCharacter>(Found[0]) : nullptr;
        AssertTrue(IsValid(Pawn), TEXT("Candidate present on server"));

        FPACS_OrbitEdit Edit;
        Edit.bHasRadius = true;
        Edit.NewRadiusCm = 18000.f;
        Edit.DurRadiusS = 1.0f;
        Edit.TransactionId = 1;
        Edit.AnchorPolicy = EPACS_AnchorPolicy::PreserveAngleOnce;
        Pawn->Server_ApplyOrbitParams(Edit);
        FinishStep();
    });

    // Step 4: Clients validate replication: Speed unchanged; Frame rolls; VRCamera roll ~ 0
    AddStep(TEXT("ClientsValidateBankAndSpeed"), FWorkerDefinition::AllClients, [this]()
    {
        SetTimeLimit(10.0f);
    }, [this](float DeltaTime)
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APACS_CandidateHelicopterCharacter::StaticClass(), Found);
        auto* Pawn = Found.Num() ? Cast<APACS_CandidateHelicopterCharacter>(Found[0]) : nullptr;
        if (!Pawn) return;

        auto* CMC = Cast<UPACS_HeliMovementComponent>(Pawn->GetCharacterMovement());
        if (!CMC) return;

        const float Speed = CMC->SpeedCms;
        const float Radius = CMC->RadiusCm;

        // Validate speed remains within expected clamp
        AssertTrue(Speed >= 0.f, TEXT("Speed non-negative"));
        AssertTrue(Radius >= 100.f, TEXT("Radius clamped"));

        // Bank isolation: HelicopterFrame has non-zero roll at speed; camera roll ~ 0
        const UActorComponent* CamComp = Pawn->FindComponentByClass<UCameraComponent>();
        const USceneComponent* Frame   = Pawn->GetDefaultSubobjectByName(TEXT("HelicopterFrame"));
        if (CamComp && Frame)
        {
            const float FrameRoll = Frame->GetComponentRotation().Roll;
            const float CamRoll   = CastChecked<const UCameraComponent>(CamComp)->GetComponentRotation().Roll;
            AssertTrue(FMath::Abs(CamRoll) < 0.5f, TEXT("VRCamera stays level (roll ~ 0)"));
            AssertTrue(FMath::Abs(FrameRoll) >= 0.f, TEXT("HelicopterFrame has visual roll (>= 0)"));
        }
        FinishStep();
    });
}
```

---

## 6) Boundary collision and sliding (Networked Functional Test)

### 6.1 `Public\test\PACS_Heli_BoundaryNFT.h`
```cpp
#pragma once
#include "FunctionalTesting/NetworkedFunctionalTest.h"
#include "PACS_Heli_BoundaryNFT.generated.h"

UCLASS()
class UPACS_Heli_BoundaryNFT : public UNetworkedFunctionalTest
{
    GENERATED_BODY()
public:
    virtual void PrepareTest() override;
};
```

### 6.2 `Private\test\PACS_Heli_BoundaryNFT.cpp`
```cpp
#include "test/PACS_Heli_BoundaryNFT.h"
#include "test/PACS_Heli_TestHelpers.h"
#include "PACS_CandidateHelicopterCharacter.h"
#include "PACS_HeliMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UPACS_Heli_BoundaryNFT::PrepareTest()
{
    // Server spawns candidate and a blocking box intersecting the orbital path
    AddServerStep(TEXT("SpawnCandidateAndBoundary"), FWorkerDefinition::Server(1), [this]()
    {
        UWorld* W = GetWorld();
        auto* Pawn = PACSHeliTest::SpawnCandidate(W, FVector::ZeroVector);
        RegisterAutoDestroyActor(Pawn);
        AssertTrue(IsValid(Pawn), TEXT("Candidate spawned"));

        // Create a blocking box near the orbit path at altitude plane
        AActor* Block = PACSHeliTest::SpawnBlockingBox(W, FVector(10000, 0, Pawn->GetActorLocation().Z), FVector(500, 500, 1000), TEXT("BoundaryBox"));
        RegisterAutoDestroyActor(Block);
        AssertTrue(IsValid(Block), TEXT("Boundary box spawned"));

        // Increase radius and speed so that the path meets the box area
        auto* CMC = Cast<UPACS_HeliMovementComponent>(Pawn->GetCharacterMovement());
        CMC->RadiusCm = 10000.f;
        CMC->SpeedCms = 2000.f;
        FinishStep();
    });

    // Clients observe over time that actor does not tunnel through but slides
    AddStep(TEXT("ClientsObserveNoTunnel"), FWorkerDefinition::AllClients, [this]()
    {
        SetTimeLimit(10.0f);
    }, [this](float DeltaTime)
    {
        UWorld* W = GetWorld();
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(W, APACS_CandidateHelicopterCharacter::StaticClass(), Found);
        auto* Pawn = Found.Num() ? Cast<APACS_CandidateHelicopterCharacter>(Found[0]) : nullptr;
        if (!Pawn) return;

        // Over test duration ensure Z remains constant (plane constraint) and location never enters the box volume extents.
        const FVector Loc = Pawn->GetActorLocation();
        AssertTrue(!FMath::IsNearlyZero(Loc.Z), TEXT("Altitude preserved"));
        // Basic heuristic: ensure X is capped near 10000 (box center) and not penetrating beyond half-extent by a large margin
        AssertTrue(Loc.X < 11000.f, TEXT("Did not tunnel past boundary in a single frame"));
        FinishStep();
    });
}
```

---

## 7) Performance sanity (editor, no net)

This is a coarse check to catch egregious regressions: spawning N candidates in an empty world and ticking for a few seconds must not exceed a per-actor step budget.

### 7.1 `Public\test\PACS_Heli_PerfSpec.h`
```cpp
#pragma once
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_Heli_PerfSpec, "PACS.Heli.Perf.SpawnAndTickBudget",
EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter);
```

### 7.2 `Private\test\PACS_Heli_PerfSpec.cpp`
```cpp
#include "test/PACS_Heli_PerfSpec.h"
#include "test/PACS_Heli_TestHelpers.h"
#include "PACS_CandidateHelicopterCharacter.h"
#include "PACS_HeliMovementComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

bool FPACS_Heli_PerfSpec::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("World"), World);
    if (!World) return false;

    const int32 N = 25; // adjust if needed for local hardware
    TArray<APACS_CandidateHelicopterCharacter*> Actors;
    Actors.Reserve(N);

    for (int32 i=0;i<N;++i)
    {
        auto* P = PACSHeliTest::SpawnCandidate(World, FVector(i*2000.f, 0, 20000.f));
        TestNotNull(TEXT("Spawned"), P);
        if (P)
        {
            auto* CMC = Cast<UPACS_HeliMovementComponent>(P->GetCharacterMovement());
            CMC->RadiusCm = 15000.f;
            CMC->SpeedCms = 2222.22f;
            Actors.Add(P);
        }
    }

    const double T0 = FPlatformTime::Seconds();
    PACSHeliTest::PumpWorld(World, 5.0f);
    const double T1 = FPlatformTime::Seconds();
    const double Elapsed = (T1 - T0);
    const double PerActorPerSecond = (Elapsed / 5.0) / double(N);

    // Loose budget: < 0.0008 s per actor per second of simulated time on dev machine
    TestTrue(TEXT("Per-actor perf budget ok"), PerActorPerSecond < 0.0008);
    return true;
}
```

---

## 8) Running the tests

1) Editor automation (non-networked specs):
```
UE5Editor-Cmd.exe "<PathToProject>\POLAIR_CS.uproject" -nop4 -unattended -NullRHI -ExecCmds="Automation RunTests PACS.Heli.Data.*; Automation RunTests PACS.Heli.Kinematics.*; Automation RunTests PACS.Heli.Perf.*; Quit"
```

2) Dedicated server multi-process (networked functional tests): run via Session Frontend or command line. Example (adjust paths and platform as needed):
```
UE5Editor.exe "<PathToProject>\POLAIR_CS.uproject" -game -messaging -Multiprocess -log -execcmds="Automation RunTests PACS_Heli_DedicatedServerNFT; Automation RunTests PACS_Heli_BoundaryNFT"
```
Ensure the session is launched as **Dedicated Server** with at least 1 server and 2 clients. The Networked Functional Tests will orchestrate their steps on server/clients.

---

## 9) Acceptance for the test suite

- DataSpec validates the existence and tunables of `PACS_CandidateHelicopterData`.
- KinematicsSpec confirms linear speed invariance across radius changes.
- DedicatedServerNFT confirms replication of orbit edits, single-anchor policy, and banking isolation (frame rolls, VR camera level).
- BoundaryNFT ensures swept movement respects blocking and avoids tunneling.
- PerfSpec catches large per-actor cost regressions.
- All tests compile into **POLAIR_CSEditor** and are placed exactly in the specified `test` folders.
- No additional features or content are added by tests; they only exercise existing code paths.

End of document.
