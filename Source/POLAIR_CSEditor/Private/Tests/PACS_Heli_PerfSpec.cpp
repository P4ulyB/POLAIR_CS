#include "Tests/PACS_Heli_PerfSpec.h"
#include "Tests/PACS_Heli_TestHelpers.h"
#include "Actors/Pawn/PACS_CandidateHelicopterCharacter.h"
#include "Components/PACS_HeliMovementComponent.h"
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