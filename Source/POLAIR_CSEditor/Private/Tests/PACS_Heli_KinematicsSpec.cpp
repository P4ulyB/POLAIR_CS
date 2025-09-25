#include "Tests/PACS_Heli_KinematicsSpec.h"
#include "Actors/Pawn/PACS_CandidateHelicopterCharacter.h"
#include "Components/PACS_HeliMovementComponent.h"
#include "Engine/World.h"
#include "Tests/PACS_Heli_TestHelpers.h"

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