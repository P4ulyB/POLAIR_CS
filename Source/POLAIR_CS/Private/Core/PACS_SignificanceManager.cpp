#include "Core/PACS_SignificanceManager.h"
#include "Actors/NPC/PACS_NPCCharacter.h"
#include "SignificanceManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Modules/ModuleManager.h"

void UPACS_SignificanceManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Only run on clients
    if (GetWorld()->IsNetMode(NM_DedicatedServer))
    {
        return;
    }

    // Defer the SignificanceManager lookup to next frame
    // This allows the engine's SignificanceManager to be created first
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        SignificanceManager = USignificanceManager::Get(GetWorld());

        if (!SignificanceManager)
        {
            // Try to load the SignificanceManager module to ensure it's initialized
            FModuleManager& ModuleManager = FModuleManager::Get();
            ModuleManager.LoadModuleChecked(TEXT("SignificanceManager"));

            // Try again after loading the module
            SignificanceManager = USignificanceManager::Get(GetWorld());

            if (!SignificanceManager)
            {
                UE_LOG(LogTemp, Warning, TEXT("PACS_SignificanceManager: Failed to get engine SignificanceManager after deferred init"));
                return;
            }
        }

        UE_LOG(LogTemp, Log, TEXT("PACS_SignificanceManager: Successfully initialized with engine SignificanceManager"));

        // Re-register any NPCs that were registered before we had the SignificanceManager
        TArray<APACS_NPCCharacter*> NPCsToReregister;
        for (auto& Pair : RegisteredNPCs)
        {
            NPCsToReregister.Add(Pair.Key);
        }
        RegisteredNPCs.Empty();

        for (APACS_NPCCharacter* NPC : NPCsToReregister)
        {
            RegisterNPC(NPC);
        }
    });

    UE_LOG(LogTemp, Log, TEXT("PACS_SignificanceManager: Deferring initialization to next frame"));
}

void UPACS_SignificanceManager::Deinitialize()
{
    // Clean up all registered NPCs
    for (auto& Pair : RegisteredNPCs)
    {
        if (Pair.Key && SignificanceManager)
        {
            SignificanceManager->UnregisterObject(Pair.Key);
        }
    }
    RegisteredNPCs.Empty();

    Super::Deinitialize();
}

void UPACS_SignificanceManager::RegisterNPC(APACS_NPCCharacter* NPC)
{
    if (!NPC || !SignificanceManager)
    {
        return;
    }

    // Skip on dedicated server
    if (GetWorld()->IsNetMode(NM_DedicatedServer))
    {
        return;
    }

    // Register with engine significance manager
    SignificanceManager->RegisterObject(
        NPC,
        FName("NPC"),
        [this, NPC](USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& ViewTransform)
        {
            return CalculateSignificance(NPC);
        },
        USignificanceManager::EPostSignificanceType::Sequential,
        [this, NPC](USignificanceManager::FManagedObjectInfo* ObjectInfo, float OldSignificance, float NewSignificance, bool bFinal)
        {
            if (bFinal)
            {
                OnSignificanceChanged(NPC, OldSignificance, NewSignificance);
            }
        }
    );

    // Track in our map with initial bucket
    RegisteredNPCs.Add(NPC, ESignificanceBucket::Medium);

    UE_LOG(LogTemp, Verbose, TEXT("PACS_SignificanceManager: Registered NPC %s"), *NPC->GetName());
}

void UPACS_SignificanceManager::UnregisterNPC(APACS_NPCCharacter* NPC)
{
    if (!NPC || !SignificanceManager)
    {
        return;
    }

    // Unregister from engine
    SignificanceManager->UnregisterObject(NPC);

    // Remove from tracking
    RegisteredNPCs.Remove(NPC);

    UE_LOG(LogTemp, Verbose, TEXT("PACS_SignificanceManager: Unregistered NPC %s"), *NPC->GetName());
}

float UPACS_SignificanceManager::CalculateSignificance(const APACS_NPCCharacter* NPC) const
{
    if (!NPC)
    {
        return 0.0f;
    }

    // Get local player location
    APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!LocalPC || !LocalPC->GetPawn())
    {
        return 0.0f;
    }

    // Calculate distance-based significance
    float Distance = FVector::Dist(NPC->GetActorLocation(), LocalPC->GetPawn()->GetActorLocation());

    // Higher score = more significant
    // Use inverse distance with a minimum to avoid divide by zero
    float Significance = 1.0f / FMath::Max(100.0f, Distance);

    // Boost significance if NPC is selected by local player
    if (NPC->IsSelectedBy(LocalPC->PlayerState))
    {
        Significance *= 10.0f;
    }

    // Boost if NPC is moving (more important to see smooth movement)
    if (NPC->GetVelocity().SizeSquared() > 100.0f)
    {
        Significance *= 2.0f;
    }

    return Significance;
}

ESignificanceBucket UPACS_SignificanceManager::ScoreToBucket(float Score) const
{
    // Convert significance score to bucket based on thresholds
    // Higher scores = closer/more important

    if (Score > 1.0f / CriticalDistance)
    {
        return ESignificanceBucket::Critical;
    }
    else if (Score > 1.0f / HighDistance)
    {
        return ESignificanceBucket::High;
    }
    else if (Score > 1.0f / MediumDistance)
    {
        return ESignificanceBucket::Medium;
    }
    else if (Score > 1.0f / LowDistance)
    {
        return ESignificanceBucket::Low;
    }
    else
    {
        return ESignificanceBucket::Culled;
    }
}

void UPACS_SignificanceManager::OnSignificanceChanged(UObject* Object, float OldSignificance, float NewSignificance)
{
    APACS_NPCCharacter* NPC = Cast<APACS_NPCCharacter>(Object);
    if (!NPC)
    {
        return;
    }

    ESignificanceBucket OldBucket = ScoreToBucket(OldSignificance);
    ESignificanceBucket NewBucket = ScoreToBucket(NewSignificance);

    // Only apply changes if bucket actually changed
    if (OldBucket != NewBucket)
    {
        ApplySignificanceBucket(NPC, NewBucket);
        RegisteredNPCs[NPC] = NewBucket;

        UE_LOG(LogTemp, Verbose, TEXT("PACS_SignificanceManager: %s changed from bucket %d to %d"),
            *NPC->GetName(), (int32)OldBucket, (int32)NewBucket);
    }
}

void UPACS_SignificanceManager::ApplySignificanceBucket(APACS_NPCCharacter* NPC, ESignificanceBucket Bucket)
{
    if (!NPC)
    {
        return;
    }

    USkeletalMeshComponent* MeshComp = NPC->GetMesh();
    if (!MeshComp)
    {
        return;
    }

    // Epic pattern: Apply visual settings based on significance
    switch (Bucket)
    {
    case ESignificanceBucket::Critical:
        // Full quality - everything enabled
        MeshComp->SetComponentTickEnabled(true);
        MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        MeshComp->bEnableUpdateRateOptimizations = false;
        // Animation rate handled by AnimBudgetAllocator
        NPC->SetActorTickInterval(0.016f); // 60 FPS
        break;

    case ESignificanceBucket::High:
        // Slightly reduced - URO enabled
        MeshComp->SetComponentTickEnabled(true);
        MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
        MeshComp->bEnableUpdateRateOptimizations = true;
        // Animation rate handled by AnimBudgetAllocator
        NPC->SetActorTickInterval(0.033f); // 30 FPS
        break;

    case ESignificanceBucket::Medium:
        // Reduced updates
        MeshComp->SetComponentTickEnabled(true);
        MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        MeshComp->bEnableUpdateRateOptimizations = true;
        // Animation rate handled by AnimBudgetAllocator
        NPC->SetActorTickInterval(0.1f); // 10 FPS
        break;

    case ESignificanceBucket::Low:
        // Minimal updates
        MeshComp->SetComponentTickEnabled(true);
        MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        MeshComp->bEnableUpdateRateOptimizations = true;
        MeshComp->bNoSkeletonUpdate = true;
        // Animation rate handled by AnimBudgetAllocator
        NPC->SetActorTickInterval(0.5f); // 2 FPS
        break;

    case ESignificanceBucket::Culled:
        // No updates at all
        MeshComp->SetComponentTickEnabled(false);
        MeshComp->bNoSkeletonUpdate = true;
        NPC->SetActorTickInterval(1.0f); // 1 FPS
        break;
    }
}

ESignificanceBucket UPACS_SignificanceManager::GetNPCSignificance(const APACS_NPCCharacter* NPC) const
{
    const ESignificanceBucket* Bucket = RegisteredNPCs.Find(NPC);
    return Bucket ? *Bucket : ESignificanceBucket::Culled;
}