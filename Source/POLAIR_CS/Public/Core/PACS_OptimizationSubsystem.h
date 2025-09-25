#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PACS_OptimizationSubsystem.generated.h"

class IAnimationBudgetAllocator;

/**
 * PACS Optimization Subsystem
 * Manages client-side performance optimizations including Animation Budget Allocator
 * Works in conjunction with Significance Manager for NPC visual quality
 */
UCLASS()
class POLAIR_CS_API UPACS_OptimizationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // UGameInstanceSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Enable Animation Budget Allocator for the world
    UFUNCTION(BlueprintCallable, Category = "Optimization")
    void EnableAnimationBudgetAllocator(UWorld* World);

    // Register an NPC for optimized animation budgeting
    UFUNCTION(BlueprintCallable, Category = "Optimization")
    void RegisterNPCForOptimization(class APACS_NPCCharacter* NPC);

    // Configure skeletal mesh optimization settings
    UFUNCTION(BlueprintCallable, Category = "Optimization")
    void ConfigureSkeletalMeshOptimizations(USkeletalMeshComponent* MeshComp);

protected:
    // Animation budget allocator interface
    IAnimationBudgetAllocator* AnimBudgetAllocator;

    // Budget parameters
    float AnimationBudgetMs = 10.0f; // 10ms budget for animations per frame
    int32 MaxTickedPoseComponents = 30; // Max skeletal meshes to tick per frame
};