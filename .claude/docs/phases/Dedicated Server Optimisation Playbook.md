Below is a tight, practical playbook for RepGraph + Dormancy + Significance/Anim Budget + event-driven CMC ticking in a dedicated-server game. This keeps authority on the server while pushing visual cost client-side.

1) Replication Graph (server: who gets what, when)

What it does: Replaces per-actor relevancy with a graph that builds per-connection actor lists (spatial cells, “always relevant”, teams, etc.). It’s the correct lever for keeping bandwidth/CPU sane as actor counts grow. 
Epic Games Developers
+1

Setup (minimal):

Enable RepGraph in Project Settings → Networking; set your custom class in DefaultEngine.ini:

[/Script/Engine.Engine]
NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[/Script/OnlineSubsystemUtils.IpNetDriver]
NetConnectionClassName="/Script/Engine.NetConnection"


In C++, derive UReplicationGraph, add a spatial node (grid/quad) and route your NPC class into it (e.g., by tag or class). Use per-class NetCullDistanceSquared to match gameplay needs.

Keep actors out of Tick-time “relevancy hacks”; let RepGraph own it.

Rules of thumb:

Idle/background NPCs → spatial node only.

Rarely important (match start, bosses) → “global/always” node.

Wake actors on state change; don’t constantly tweak NetUpdateFrequency.

2) Dormancy (server: stop replicating when idle)

What it does: Lets an actor go to sleep network-wise. Use DORM_Initial (placed/spawned), wake on changes, sleep again when stable. This cuts replication work drastically for idle NPCs. 
Epic Games Developers
+1

Pattern:

// After initial replication (PostInitializeComponents / shortly after spawn)
SetNetDormancy(DORM_Initial);

// When issuing an order / selection changed (before mutating replicated state)
FlushNetDormancy(); // wakes, sends deltas

// When movement completes / NPC returns to idle
SetNetDormancy(DORM_Initial);


Pitfalls: Don’t mutate replicated vars while dormant without waking first; prefer FlushNetDormancy() to one-off ForceNetUpdate() for clean cycles. 
Epic Games Developers

3) Significance Manager (client: decide what deserves budget)

What it does: Central service where you register objects and compute a score (distance, on-screen, importance). You then throttle LODs, FX, tick rates, etc., based on buckets. Great for per-client decision-making. 
Epic Games Developers
+1

Pattern (client-side):

auto* Sig = USignificanceManager::Get(GetWorld());
Sig->RegisterObject(this, FName("NPC"),
    [](USignificanceManager::FManagedObjectInfo* Info, const FTransform& ViewXf)
    {
        const float Dist = FVector::Dist(Info->GetTransform().GetLocation(), ViewXf.GetLocation());
        return 1.0f / FMath::Max(100.f, Dist); // higher = more significant
    },
    /*onSignificanceChanged:*/ [this](USignificanceManager::FManagedObjectInfo* Info, float Old, float New)
    {
        // Move between buckets: Full → Reduced → Culled visuals
        ApplyVisualBucket(New);
    });


Register/unregister on visibility/spawn/pooled-reuse. Drive pose tick frequency, LOD choice, FX, audio from buckets, not from per-actor Tick. 
Epic Games Developers

4) Animation Budget Allocator (client: cap anim CPU)

What it does: Enforces a global anim CPU budget by dynamically throttling skeletal mesh ticks across the world. It integrates well with significance. 
Epic Games Developers

Setup (both sides but impacts clients):

Enable Animation Budget Allocator plugin.

In code at world start (GameMode/GameInstance):

IAnimationBudgetAllocator::Get(GetWorld())->SetEnabled(true);


Ensure AnimBP “Enable Animation Budgeting” node (or equivalent API) is used on meshes you want budgeted; toggle with a.Budget.Enabled=1 for runtime testing. 
Epic Games Developers

Also turn on per-mesh optimisations:

USkeletalMeshComponent* Skel = GetMesh();
Skel->bEnableUpdateRateOptimizations = true;                    // URO
Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;


URO reduces pose ticks; visibility option skips pose when off-screen. (Watch fast curves—URO may cause laggy curves; profile before enabling on critical meshes.) 
Epic Games Developers
+2
Epic Games Developers
+2

5) Event-driven enabling of CMC tick (server: move only costs when moving)

Goal: Avoid paying UCharacterMovementComponent cost while idle.

Pattern:

void ANPC::ServerMoveToLocation_Implementation(const FVector Target)
{
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        if (UCharacterMovementComponent* CMC = GetCharacterMovement())
            CMC->SetComponentTickEnabled(true);

        const FAIRequestID Req = AI->MoveTo(FAIMoveRequest(Target).SetAcceptanceRadius(75.f));
        AI->ReceiveMoveCompleted.AddUniqueDynamic(this, &ThisClass::OnMoveComplete);
        FlushNetDormancy(); // wake for the state change
    }
}

void ANPC::OnMoveComplete(FAIRequestID, const FPathFollowingResult& Res)
{
    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
        CMC->SetComponentTickEnabled(false);

    SetNetDormancy(DORM_Initial); // back to sleep
}


This keeps movement accurate when needed, and zeroes movement cost when not.

How they fit together (one flow)

RepGraph decides which clients see an NPC at all (server-side bandwidth/CPU). 
Epic Games Developers

Dormancy makes idle NPCs silent on the wire until an order hits. 
Epic Games Developers

Significance ranks visible NPCs per client; Anim Budget enforces the frame’s anim CPU. 
Epic Games Developers
+1

CMC tick only runs while moving (server), disabled on completion.

Practical defaults (drop-in)

Server

Enable RepGraph; put NPCs in spatial nodes with sensible NetCullDistanceSquared.

Set NPCs to DORM_Initial after initial replication; FlushNetDormancy() on orders and selection changes; return to DORM_Initial when idle. 
Epic Games Developers

Never tweak NetUpdateFrequency every frame; change rarely if at all (RepGraph owns cadence).

Client

Enable Animation Budget Allocator (IAnimationBudgetAllocator::Get(World)->SetEnabled(true), a.Budget.Enabled=1) and Significance buckets to drive URO/LOD/FX. 
Epic Games Developers

On all NPC meshes: OnlyTickPoseWhenRendered + bEnableUpdateRateOptimizations=true. 
Epic Games Developers
+1

Both

Use Unreal Insights to verify: reduced ReplicateActor time (server) and lowered SkeletalMeshComponent::TickPose time (client).

Why this beats per-tick distance logic

RepGraph eliminates O(N×Clients) relevancy checks from actors themselves. 
Epic Games Developers

Dormancy stops property churn and prioritisation work for idle actors. 
Epic Games Developers

Significance/Anim Budget shift cost to where it matters visually and keep you within a predictable budget. 
Epic Games Developers
+1


pre-loaded during level load or use async loading