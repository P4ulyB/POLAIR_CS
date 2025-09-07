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