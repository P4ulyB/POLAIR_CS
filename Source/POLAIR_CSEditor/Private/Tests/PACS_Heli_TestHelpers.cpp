#include "Tests/PACS_Heli_TestHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Actors/Pawn/PACS_CandidateHelicopterCharacter.h"
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