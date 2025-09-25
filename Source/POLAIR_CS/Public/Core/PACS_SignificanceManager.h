#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PACS_SignificanceManager.generated.h"

class USignificanceManager;
class APACS_NPCCharacter;

/**
 * Significance buckets for NPCs
 * Controls visual fidelity based on importance
 */
UENUM(BlueprintType)
enum class ESignificanceBucket : uint8
{
    Critical = 0,  // Very close/important - full detail
    High = 1,      // Close - reduced detail
    Medium = 2,    // Medium distance - further reduced
    Low = 3,       // Far - minimal detail
    Culled = 4     // Very far - no updates
};

/**
 * PACS Significance Manager
 * Client-side system for managing visual quality based on distance and importance
 * Works with Animation Budget Allocator for performance optimization
 */
UCLASS()
class POLAIR_CS_API UPACS_SignificanceManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // UWorldSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Register an NPC with the significance manager
    UFUNCTION(BlueprintCallable, Category = "Significance")
    void RegisterNPC(APACS_NPCCharacter* NPC);

    // Unregister an NPC
    UFUNCTION(BlueprintCallable, Category = "Significance")
    void UnregisterNPC(APACS_NPCCharacter* NPC);

    // Get current significance bucket for an NPC
    UFUNCTION(BlueprintCallable, Category = "Significance")
    ESignificanceBucket GetNPCSignificance(const APACS_NPCCharacter* NPC) const;

protected:
    // Calculate significance score for an NPC
    float CalculateSignificance(const APACS_NPCCharacter* NPC) const;

    // Apply visual settings based on significance bucket
    void ApplySignificanceBucket(APACS_NPCCharacter* NPC, ESignificanceBucket Bucket);

    // Called when significance changes for an object
    void OnSignificanceChanged(UObject* Object, float OldSignificance, float NewSignificance);

private:
    // Core significance manager from engine
    UPROPERTY()
    TObjectPtr<USignificanceManager> SignificanceManager;

    // Track registered NPCs and their current buckets
    UPROPERTY()
    TMap<TObjectPtr<APACS_NPCCharacter>, ESignificanceBucket> RegisteredNPCs;

    // Distance thresholds for buckets (in units)
    float CriticalDistance = 1000.0f;  // 10 meters
    float HighDistance = 3000.0f;      // 30 meters
    float MediumDistance = 5000.0f;    // 50 meters
    float LowDistance = 10000.0f;      // 100 meters

    // Convert significance score to bucket
    ESignificanceBucket ScoreToBucket(float Score) const;
};