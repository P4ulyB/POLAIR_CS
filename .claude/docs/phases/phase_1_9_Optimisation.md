1) Kill per‑actor distance logic on the server

You call GetWorld()->GetFirstPlayerController() to compute distance. On a dedicated server this is the wrong notion of “viewer” (and can be nullptr early). Distance‑based throttling should be client‑only visuals.

Server‑side relevancy is already covered by net cull distance, dormancy, priority and the RepGraph. Don’t fight it per Tick.

Change

Wrap distance/mesh/visual tick control with #if !UE_SERVER (or runtime IsRunningDedicatedServer() guard) and don’t touch NetUpdateFrequency every 0.1 s.

If you need server throttling, do it event‑driven with hysteresis (see §3) or move to RepGraph.

2) Prefer engine systems to manual ticking rules

Use these, in order:

Significance Manager (GameplayTags/Significance) to drive pose ticking/LOD/material intensity.

Animation Budget Allocator for skeletal mesh tick budgeting.

NetCullDistanceSquared (per component / per actor) and RepGraph to control replication scope.

Dormancy for idle NPCs (DORM_Initial while idle, FlushNetDormancy() on state change like selection/move).

Your current SetNetUpdateFrequency() thrashes every distance band change and can increase replication work. Let RepGraph + dormancy do the heavy lifting.

3) If you keep banding, add hysteresis & avoid constant writes

Right now, UpdateDistanceBasedOptimizations() can flip bands each tick near thresholds and spam:

SetActorTickInterval

SetNetUpdateFrequency

component tick enables

Do

Cache the current band; only apply when the band actually changes.

Add hysteresis (e.g., enter “Medium” at 2300, leave at 1900) to prevent flap.

Rate‑limit network frequency changes (e.g., reevaluate every 2–3 s tops).

4) Don’t create client‑only components on a server build

You create UDecalComponent and selection materials in the constructor unconditionally.

Do

#if !UE_SERVER
CollisionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("CollisionDecal"));
// ... decal setup
#endif


Also skip the async asset loads and material creation entirely on server (WITH_EDITOR is not the right gate; use !UE_SERVER or IsRunningDedicatedServer()).

5) Movement tick enable/disable: finish the loop

You enable CMC ticking before SimpleMoveToLocation but never disable it when the move completes.

Do

Ensure the pawn has an AIController (AAIController) and bind:

if (AAIController* AI = Cast<AAIController>(GetController()))
{
    FPathFollowingRequestResult Res = AI->MoveTo(FAIMoveRequest(TargetLocation));
    AI->ReceiveMoveCompleted.AddUniqueDynamic(this, &ThisClass::OnAIMoveCompleted);
}


In OnAIMoveCompleted, disable CMC tick again and, if idling, enter dormancy.

Also consider MoveTo over SimpleMoveToLocation for better control (acceptance radius, partial paths, filters).

6) Selection & replication hygiene

CurrentSelector being an APlayerState* is fine, but keep the property ReplicatedUsing=OnRep_CurrentSelector. When clearing on server, prefer dormancy wake + single net update rather than calling ForceNetUpdate() often. If selectors don’t change frequently, dormancy saves you real bandwidth.

VisualConfig looks like a struct of soft paths + params. If it only changes at spawn/pooled‑reuse, COND_InitialOnly + DORM_Initial + FlushNetDormancy() on reuse is cleaner than always‑replicating. Your comment about “dormancy race” is usually solved by setting dormancy after initial replication (post‑spawn) or by using RepGraph’s just‑spawned node. Right now you’re paying steady replication tax.

7) Async load handle lifecycle

You request async loads, but I don’t see a release:

Store TSharedPtr<FStreamableHandle> AssetLoadHandle;

On EndPlay, if AssetLoadHandle.IsValid(), AssetLoadHandle->ReleaseHandle(); AssetLoadHandle.Reset();

If pooled, ensure you cancel in‑flight loads on return to pool to avoid stray callbacks.

8) Collision strategy

For selection, you add a UBoxComponent attached to the mesh. You already have a capsule. One more primitive per NPC × 15–50 NPCs adds cost (broadphase, overlaps).

If the box is only for UI hover on clients, create it client‑only and set a dedicated collision channel/profile that never interacts with physics or nav.

Server doesn’t need it for gameplay? Compile it out on server (#if !UE_SERVER). Nav/pathing uses the capsule anyway.

9) Decal/material parameter churn

You cache a UMaterialInstanceDynamic and tweak params on hover/selection. Good.

Ensure you don’t drive those parameters from server state per frame. Only change on state transitions (hover on/off, selection change).

Clear CachedDecalMaterial in EndPlay to avoid dangling references in pooled reuse.

10) Tick rate

Global PrimaryActorTick.TickInterval = 0.1f (10 Hz) is okay for client visuals; on server, for AI you might run behaviour at 5–10 Hz but leave movement component to its own sub‑stepping as needed. If an NPC is completely idle + dormant, consider disabling actor tick altogether and re‑enabling on stimuli (perception, command, proximity via significance).

11) Authority checks

In ServerMoveToLocation_Implementation, you recheck HasAuthority(). It’s redundant (it runs on server). Keep it if you like belt‑and‑braces, but it’s not required.

Keep the distance sanity check; add a nav query validity check:

if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
{
    FNavLocation Projected;
    if (!NavSys->ProjectPointToNavigation(TargetLocation, Projected))
    {
        UE_LOG(LogTemp, Warning, TEXT("Move target not on navmesh")); 
        return;
    }
}

12) Net settings quick wins

Set sensible defaults once (not in Tick):

MinNetUpdateFrequency = 2.f; NetUpdateFrequency = 10.f;

Per‑NPC NetCullDistanceSquared based on gameplay need.

bOnlyRelevantToOwner = false, bAlwaysRelevant = false (let relevance decide).

Use RepGraph (Project Settings → Networking → Enable Replication Graph) and put NPCs in spatial nodes; that alone usually outperforms ad‑hoc per‑actor throttling.

13) Client‑only selection colouring

Your UpdateVisualState() is clean. Make sure the priority logic runs only on clients (and isn’t called on dedicated server). You’ve guarded OnRep_CurrentSelector; also guard any external callers.

14) Pooling readiness

You reference bIsPooledCharacter. Ensure you:

Reset bVisualsApplied, CachedDecalMaterial, collision sizes, and CMC tick state on return to pool.

Re‑enter DORM_Initial on return, FlushNetDormancy() on assign to a player/command.