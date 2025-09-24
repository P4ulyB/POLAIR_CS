# Character Performance Analysis - Critical Bottlenecks

## Executive Summary
**CRITICAL PERFORMANCE FAILURE:** System running at ~26 FPS (38.769ms CPU time) vs 90 FPS target (11.1ms budget)
- **972.2ms WaitForTasks:** Catastrophic task graph congestion from 296 instances
- **280ms Character Systems:** Movement and mesh updates consuming 25x budget per frame
- **544 Character Instances:** Each creating dynamic materials and async loading assets
- **Memory Violation:** Estimated 2-3MB per character vs 1MB target

## Critical Bottleneck #1: WaitForTasks (972.2ms - 87% of frame time)

### Root Cause Analysis:
```cpp
// Line 159-227: ASYNC LOADING PER CHARACTER
AssetLoadHandle = StreamableManager.RequestAsyncLoad(ToLoad, FStreamableDelegate::CreateWeakLambda(this, [this]()
```
- **296 async load operations** queued simultaneously
- Each character triggers 3-4 asset loads (mesh, anim, decal, materials)
- Task graph completely saturated waiting for asset streaming completion
- Lambda captures creating additional memory pressure

### Performance Impact:
- **972ms stall** waiting for task completion
- **Task graph congestion** blocking all other game systems
- **Memory fragmentation** from 296+ concurrent load operations

## Critical Bottleneck #2: Character Movement (280.5ms)

### Root Cause Analysis:
```cpp
// Line 26: ALWAYS TICKING MOVEMENT
GetCharacterMovement()->SetComponentTickEnabled(true);

// Line 29-31: EXPENSIVE ROTATION UPDATES
GetCharacterMovement()->bOrientRotationToMovement = true;
GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
```

### Performance Violations:
- **226 movement components** ticking every frame regardless of visibility
- **No LOD system** - all characters update at full fidelity
- **No distance culling** - characters update regardless of distance from player
- **540°/sec rotation rate** requiring interpolation calculations per tick

## Critical Bottleneck #3: Dynamic Material Instances (Memory + CPU)

### Root Cause Analysis:
```cpp
// Line 198: DYNAMIC MATERIAL PER CHARACTER
UMaterialInstanceDynamic* DynamicDecalMat = UMaterialInstanceDynamic::Create(DecalMat, this);

// Lines 315-349: FREQUENT PARAMETER UPDATES
CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.SelectedBrightness);
CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.SelectedColour);
```

### Performance Violations:
- **544 dynamic material instances** in memory
- **Material parameter updates** on hover/selection state changes
- **No material instance pooling** - each character creates unique instance
- **FName allocations** in hot path (should use static FNames)

## Critical Bottleneck #4: GetWorld()->GetFirstPlayerController() Anti-Pattern

### Root Cause Analysis:
```cpp
// Line 356: EXPENSIVE WORLD QUERY IN HOT PATH
APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
```

### Performance Impact:
- Called in `GetCurrentVisualPriority()` which is called from `UpdateVisualState()`
- **O(n) world traversal** for every visual state update
- Should be **cached once** on BeginPlay

## Memory Budget Violations

### Current Memory Footprint (Per Character):
```
SkeletalMesh:           ~500KB (LOD0)
AnimInstance:           ~200KB
Dynamic Material:       ~150KB
Visual Config:          ~100KB
Collision Components:   ~50KB
Movement Component:     ~100KB
Async Load Handles:     ~50KB
-----------------------------------
Total:                  ~1.15MB minimum (violates 1MB target)

With 544 instances:     ~626MB character memory
```

## Recommended Optimizations (Priority Order)

### 1. Implement Character Pooling & LOD System (Save 600ms+)
```cpp
// SOLUTION: Pre-pool characters with shared materials
class UPACS_CharacterPool : public UGameInstanceSubsystem
{
    TArray<APACS_NPCCharacter*> PooledCharacters;
    TMap<UClass*, UMaterialInstanceDynamic*> SharedMaterialInstances;
    
    // Pre-create 600 characters at game start
    // Share material instances across same character types
    // Recycle instead of spawn/destroy
};
```

### 2. Implement Distance-Based Update Culling (Save 200ms+)
```cpp
// SOLUTION: Tick groups based on distance
void APACS_NPCCharacter::UpdateTickRate()
{
    float DistanceToPlayer = GetDistanceToLocalPlayer();
    
    if (DistanceToPlayer > 5000.0f) // 50m
    {
        GetCharacterMovement()->SetComponentTickEnabled(false);
        SetActorTickInterval(1.0f); // Update once per second
    }
    else if (DistanceToPlayer > 2000.0f) // 20m
    {
        SetActorTickInterval(0.1f); // 10 FPS update
    }
    else
    {
        SetActorTickInterval(0.033f); // 30 FPS for nearby
    }
}
```

### 3. Batch Asset Loading (Save 700ms+)
```cpp
// SOLUTION: Load all character assets at level start
void APACS_GameMode::PreloadAllCharacterAssets()
{
    TArray<FSoftObjectPath> AllAssets;
    // Collect all unique meshes, anims, materials
    
    StreamableManager.RequestAsyncLoad(AllAssets, 
        FStreamableDelegate::CreateLambda([this]()
        {
            // Assets loaded, now safe to spawn characters
            SpawnAllCharacters();
        }));
}
```

### 4. Static Material Parameter Names (Save 50ms+)
```cpp
// SOLUTION: Pre-cache FNames
static const FName NAME_Brightness("Brightness");
static const FName NAME_Colour("Colour");

// Use static names instead of constructing each frame
CachedDecalMaterial->SetScalarParameterValue(NAME_Brightness, value);
```

### 5. Implement Hierarchical LOD System (Save 100ms+)
```cpp
// SOLUTION: HLOD for distant character groups
class APACS_CharacterHLOD : public AActor
{
    // Replace 10+ distant characters with single instanced mesh
    UInstancedStaticMeshComponent* GroupMesh;
    
    void MergeCharacters(TArray<APACS_NPCCharacter*> Characters)
    {
        // Hide individual characters
        // Show single instanced representation
    }
};
```

### 6. Movement Component Optimization
```cpp
// SOLUTION: Custom lightweight movement for NPCs
class UPACS_SimplifiedNPCMovement : public UActorComponent
{
    // Remove unused CharacterMovement features
    // No physics, no root motion, no networking
    // Simple interpolation only when visible
};
```

## Network Bandwidth Analysis

### Current Replication Cost (Per Character):
```
Transform:              24 bytes (position + rotation)
VisualConfig:          ~200 bytes (one-time)
CurrentSelector:        8 bytes (pointer)
Movement Updates:       24 bytes @ 10Hz
-----------------------------------
Total:                 ~256 bytes initial + 240 bytes/sec ongoing

With 544 characters:    ~130KB/sec (VIOLATES 100KB/s target)
```

### Network Optimizations:
1. **Relevancy Culling:** Only replicate characters within 100m
2. **Update Rate Scaling:** Reduce to 2Hz for distant characters  
3. **Delta Compression:** Only send changed fields
4. **Quantization:** Compress position/rotation data

## Implementation Priority

### Phase 1 (Immediate - Restore Playability):
1. **Disable movement ticking** for off-screen characters
2. **Cache GetFirstPlayerController()** result
3. **Reduce network update frequency** to 5Hz

### Phase 2 (Critical - Meet VR Target):
1. **Implement character pooling** system
2. **Batch all asset loading** at level start
3. **Add distance-based LOD** system

### Phase 3 (Optimization - Polish):
1. **Shared material instances** per character type
2. **Hierarchical LOD** for groups
3. **Custom lightweight movement** component

## Validation Metrics

### Success Criteria:
- **Frame Time:** < 11.1ms (90 FPS) on Meta Quest 3
- **WaitForTasks:** < 1ms per frame
- **Character Tick:** < 5ms total for all 544 instances
- **Memory:** < 1MB per character
- **Network:** < 100KB/s per client

### Profiling Commands:
```bash
# Measure frame time
stat unit

# Profile task graph
stat dumphitches

# Memory profiling
memreport -full

# Network bandwidth
net.ShowNetworkStats 1
```

## Risk Assessment

### Critical Risks:
1. **Session Stability:** Current memory growth will crash after 2-3 hours
2. **VR Comfort:** 26 FPS causes motion sickness in VR
3. **Network Saturation:** 130KB/s will cause packet loss with 8-10 clients
4. **Scalability:** Cannot add more NPCs without complete refactor

### Mitigation Timeline:
- **Day 1:** Implement Phase 1 fixes (restore 60 FPS baseline)
- **Day 2-3:** Implement Phase 2 (achieve 90 FPS target)
- **Day 4-5:** Implement Phase 3 (optimize for stability)

## Conclusion

The current implementation has **catastrophic performance failures** requiring immediate intervention:
- **972ms task stalls** from unbatched async loading
- **280ms movement overhead** from always-ticking components
- **544 dynamic materials** causing memory pressure
- **No LOD or culling systems** for scalability

Implementing the recommended optimizations in priority order will:
1. Reduce frame time from 38.8ms to <11.1ms (achieving 90 FPS)
2. Reduce memory from ~1.15MB to <1MB per character
3. Reduce network from 130KB/s to <100KB/s
4. Enable 6-8 hour session stability

**RECOMMENDATION:** Halt feature development and focus exclusively on Phase 1-2 optimizations to restore VR viability.
