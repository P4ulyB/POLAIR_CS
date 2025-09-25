#include "Core/PACS_OptimizationSubsystem.h"
#include "Core/PACS_SignificanceManager.h"
#include "Actors/NPC/PACS_NPCCharacter.h"
#include "IAnimationBudgetAllocator.h"
#include "AnimationBudgetAllocatorParameters.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

void UPACS_OptimizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Log, TEXT("PACS_OptimizationSubsystem: Initialized"));
}

void UPACS_OptimizationSubsystem::Deinitialize()
{
    AnimBudgetAllocator = nullptr;
    Super::Deinitialize();
}

void UPACS_OptimizationSubsystem::EnableAnimationBudgetAllocator(UWorld* World)
{
    if (!World)
    {
        return;
    }

    // Skip on dedicated server - this is client-side optimization
    if (World->IsNetMode(NM_DedicatedServer))
    {
        return;
    }

    // Get the Animation Budget Allocator
    AnimBudgetAllocator = IAnimationBudgetAllocator::Get(World);

    if (AnimBudgetAllocator)
    {
        // Enable the allocator
        AnimBudgetAllocator->SetEnabled(true);

        // Configure budget parameters using the proper UE5 API
        FAnimationBudgetAllocatorParameters Params;
        Params.BudgetInMs = AnimationBudgetMs;
        Params.MinQuality = 0.0f; // Allow dropping frames if needed
        Params.MaxTickRate = 10; // Limit tick rate for distant NPCs
        Params.AlwaysTickFalloffAggression = 0.8f;
        Params.InterpolationFalloffAggression = 0.4f;

        AnimBudgetAllocator->SetParameters(Params);

        UE_LOG(LogTemp, Log, TEXT("PACS_OptimizationSubsystem: Animation Budget Allocator enabled with %fms budget"),
            AnimationBudgetMs);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS_OptimizationSubsystem: Failed to get Animation Budget Allocator"));
    }
}

void UPACS_OptimizationSubsystem::RegisterNPCForOptimization(APACS_NPCCharacter* NPC)
{
    if (!NPC)
    {
        return;
    }

    // Configure skeletal mesh optimizations
    if (USkeletalMeshComponent* MeshComp = NPC->GetMesh())
    {
        ConfigureSkeletalMeshOptimizations(MeshComp);
    }

    // Register with Significance Manager
    if (UWorld* World = NPC->GetWorld())
    {
        if (UPACS_SignificanceManager* SigManager = World->GetSubsystem<UPACS_SignificanceManager>())
        {
            SigManager->RegisterNPC(NPC);
        }
    }

    // Epic pattern: Register mesh with Animation Budget Allocator
    // NOTE: AnimationBudgetAllocator requires USkeletalMeshComponentBudgeted
    // NPCs would need to use that component class to work with the budget allocator
    // For now, we skip this registration but keep the optimization settings
    if (AnimBudgetAllocator && NPC->GetMesh())
    {
        // Only register if the component is a budgeted type
        if (USkeletalMeshComponentBudgeted* BudgetedMesh = Cast<USkeletalMeshComponentBudgeted>(NPC->GetMesh()))
        {
            AnimBudgetAllocator->RegisterComponent(BudgetedMesh);
            UE_LOG(LogTemp, Verbose, TEXT("PACS_OptimizationSubsystem: Registered NPC %s for animation budgeting"),
                *NPC->GetName());
        }
        else
        {
            // Standard skeletal mesh - will still benefit from URO and significance settings
            UE_LOG(LogTemp, Verbose, TEXT("PACS_OptimizationSubsystem: NPC %s uses standard SkeletalMeshComponent, skipping budget allocator"),
                *NPC->GetName());
        }
    }
}

void UPACS_OptimizationSubsystem::ConfigureSkeletalMeshOptimizations(USkeletalMeshComponent* MeshComp)
{
    if (!MeshComp)
    {
        return;
    }

    // Epic pattern: Enable all skeletal mesh optimizations

    // Update Rate Optimizations (URO)
    MeshComp->bEnableUpdateRateOptimizations = true;

    // Only tick pose when rendered
    MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

    // Component will be manually registered with budget allocator

    // Reduce bone update frequency when not rendered
    MeshComp->bPauseAnims = false; // Don't fully pause, let URO handle it

    // Enable component use of fixed skel bounds to avoid bounds calculation
    MeshComp->bComponentUseFixedSkelBounds = true;

    // Optimize cloth simulation (disable for NPCs)
    MeshComp->bDisableClothSimulation = true;

    // Configure animation LOD settings
    // Note: Direct AnimUpdateRateParams access removed in UE5
    // These are now handled by AnimInstance and AnimBudgetAllocator
    // Properties like bOverrideMinLOD and bUseBoundsFromMasterPoseComponent
    // have been removed or moved in UE5

    UE_LOG(LogTemp, Verbose, TEXT("PACS_OptimizationSubsystem: Configured skeletal mesh optimizations for %s"),
        *MeshComp->GetName());
}