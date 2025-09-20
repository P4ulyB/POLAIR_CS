# PACS NPC – Initial‑Only Visual Config (Soft IDs) – Implementation Instructions (for Claude Code)

**Target UE version:** 5.5+ (project uses UE 5.5/5.6 source)  
**Namespaces:** Use the existing `PACS_` prefixes and module API macro (`POLAIR_CS_API`).  
**Goal:** Minimal, server‑authoritative NPC baseline that replicates a **single initial‑only container** of visual data (soft IDs), applies visuals **client‑side on OnRep**, then enters **dormancy**. Scales to 50+ NPCs with smooth client visuals and low bandwidth.

---

## 0) NON‑NEGOTIABLE CONSTRAINTS

1. **Do not** add RPCs (no Multicast) for visuals. Use property replication + `OnRep` only.  
2. The **server** sets config and **must not** load meshes/anim assets on a dedicated server.  
3. Replicate **one container** (`FPACS_NPCVisualConfig`) marked **`COND_InitialOnly`** with **soft IDs** (not hard object pointers).  
4. Clients **async‑load** assets in `OnRep_VisualConfig`, then set Mesh → AnimClass in that order.  
5. After initial replication, the actor must call **`SetNetDormancy(DORM_Initial)`**.  
6. Late joiners must get correct visuals via the **initial bunch**; no extra code paths to "backfill".  
7. Keep code **simple** and **production‑ready** (authority checks, validation, errors logged once).

---

## 1) DATA ASSET: `UPACS_NPCConfig`

**Task:** Extend/implement a Data Asset that *authoring‑time designers* will set on each NPC blueprint. This asset converts to a small visual config of **soft IDs**.

### 1.1 Class contract
- File: `Source/POLAIR_CS/Public/Data/Configs/PACS_NPCConfig.h` and `Private/Data/Configs/PACS_NPCConfig.cpp`
- Class: `UCLASS(BlueprintType)` **`class POLAIR_CS_API UPACS_NPCConfig : public UDataAsset`**
- Expose editor fields:
  - `FPrimaryAssetId SkeletalMeshId` (or `FSoftObjectPath SkeletalMeshPath` – choose one soft reference style and keep it consistent)
  - `FPrimaryAssetId AnimBPId` (or `FSoftObjectPath AnimBPPath`), expected to resolve to a class derived from `UAnimInstance` (the generated class of your Anim BP)
- Add a **pure data → runtime blob** conversion function:
  - `void ToVisualConfig(struct FPACS_NPCVisualConfig& Out) const;`

### 1.2 Validation
- Implement `bool IsDataValid(FDataValidationContext& Context) const override;` to ensure both IDs/paths are set and resolvable in editor (best‑effort).  
- If AnimBP is a `UBlueprint` asset, verify it compiles to a valid `UAnimInstance` class (editor‑only check).

---

## 2) VISUAL CONTAINER: `FPACS_NPCVisualConfig` (soft IDs, initial‑only)

**Task:** Define a small replicated struct that carries only what the client needs to resolve visuals.

### 2.1 Struct
- File: `Source/POLAIR_CS/Public/Data/PACS_NPCVisualConfig.h`
- `USTRUCT()` **`struct POLAIR_CS_API FPACS_NPCVisualConfig`**
  - Members: `FPrimaryAssetId MeshId; FPrimaryAssetId AnimBPId; uint8 FieldsMask;`
  - Implement `bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)` to only send present fields (use `FieldsMask` bit 0 = Mesh, bit 1 = Anim).
  - Add `WithNetSerializer = true` in `TStructOpsTypeTraits` and `WithIdenticalViaEquality = true`

### 2.2 Equality
- Implement `bool operator==(const FPACS_NPCVisualConfig& Rhs) const` for `REPNOTIFY_OnChanged` support.

---

## 3) NPC CLASS: `APACS_NPCCharacter` (Character‑based, lean)

**Task:** Wire the minimal replication and client apply path.

### 3.1 Header
- File: `Source/POLAIR_CS/Public/Pawns/NPC/PACS_NPCCharacter.h`
- Class: **`class POLAIR_CS_API APACS_NPCCharacter : public ACharacter`**
- Members:
  - `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC") TObjectPtr<UPACS_NPCConfig> NPCConfigAsset;`
    - **Editor‑only authoring reference**; **not replicated**.
  - `UPROPERTY(ReplicatedUsing=OnRep_VisualConfig, meta=(RepNotifyCondition="REPNOTIFY_OnChanged")) FPACS_NPCVisualConfig VisualConfig;`
- Functions:
  - `virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;`
  - `virtual void BeginPlay() override;`
  - `UFUNCTION() void OnRep_VisualConfig();`
  - `void ApplyVisuals_Client(); // client‑only: resolve soft IDs async, then set mesh -> anim`
  - `void BuildVisualConfigFromAsset_Server(); // server‑only: NPCConfigAsset -> VisualConfig`

### 3.2 CPP (behavioural requirements)
- File: `Source/POLAIR_CS/Private/Pawns/NPC/PACS_NPCCharacter.cpp`
- **Constructor:**
  - `bReplicates = true;`
  - Keep default `NetUpdateFrequency` modest (e.g., 10 Hz). We rely on dormancy, not tick spam.
  - If you won't use CharacterMovement yet, you may `GetCharacterMovement()->SetComponentTickEnabled(false);` (keep movement for future AI).
- **GetLifetimeReplicatedProps:**
  - `DOREPLIFETIME_CONDITION(APACS_NPCCharacter, VisualConfig, COND_InitialOnly);`
- **BeginPlay:**
  - `if (HasAuthority()) {`
    - `checkf(NPCConfigAsset, TEXT("NPCConfigAsset must be set on server before BeginPlay"));`
    - `BuildVisualConfigFromAsset_Server();`
    - `SetNetDormancy(DORM_Initial); // after assigning VisualConfig`
  - `} else { // client`
    - If `VisualConfig` already valid (channel open delivered initial bunch early), call `ApplyVisuals_Client()` to fast‑path first frame.
  - **Do not** assign meshes/anim on a dedicated server.
- **OnRep_VisualConfig (client only):**
  - Start **async loads** for Mesh and Anim class via `UAssetManager`/`FStreamableManager`.  
  - When both complete, set:
    - `GetMesh()->SetSkeletalMesh(LoadedSkeletalMesh, /*bReinitPose*/ true);`
    - `GetMesh()->SetAnimInstanceClass(LoadedAnimClass);`
  - Then set client perf flags:
    - `GetMesh()->VisibilityBasedAnimTickOption = OnlyTickPoseWhenRendered;`
    - `GetMesh()->bEnableUpdateRateOptimizations = true;`
    - If using **Animation Budget Allocator**, register the skeletal mesh component.
- **BuildVisualConfigFromAsset_Server:**
  - Copy soft IDs from `NPCConfigAsset` into `VisualConfig`, set `FieldsMask` bits accordingly.
  - **Do not** load assets on server; do not touch `GetMesh()` on dedicated server.

---

## 4) ASSET STREAMING (CLIENT) – RESOLUTION DETAILS

**Task:** Implement a small helper to resolve `FPrimaryAssetId` (or `FSoftObjectPath`) to the runtime types.

- Use `UAssetManager::GetStreamableManager().RequestAsyncLoad(...)` with an **array** of soft references.  
- Compute Anim **class**: if ID resolves to a `UBlueprint` (AnimBP), get `GeneratedClass` and ensure it is a `UAnimInstance` subclass.  
- Handle failures with `ensureMsgf` once (avoid log spam). Keep the NPC visible with a fallback (no mesh/anim), but log clearly.

---

## 5) AUTHORING & COOK RULES

**Task:** Guarantee assets are available to clients in a packaged build.

- Prefer **Primary Asset** workflow for meshes/anim BPs referenced by `UPACS_NPCConfig`.  
- Or, ensure maps reference the assets explicitly to force cook.  
- In `DefaultEngine.ini`, configure Primary Asset rules if needed.  
- Add a simple **Preload Subsystem** (optional): at map load, pre‑request common `FPrimaryAssetId`s you expect across many NPCs to smooth burst spawns.

---

## 6) NETWORK / PERF SETTINGS

### 6.1 Dormancy and relevancy
- After setting `VisualConfig` on the server, call: `SetNetDormancy(DORM_Initial)`.  
- Do **not** use `DORM_DormantAll` (late joiners still need the initial bunch).  
- Configure `NetCullDistanceSquared` in CDO/INI to a sensible range for your game (e.g., ~40–60 m for ambient NPCs).

### 6.2 INI defaults (example – **do not** hardcode per instance)
Add to `DefaultGame.ini` (adjust class path to yours):
```ini
[/Script/POLAIR_CS.PACS_NPCCharacter]
NetCullDistanceSquared=2500000.0 ; 50m
NetUpdateFrequency=10.0
MinNetUpdateFrequency=2.0
```
Use dormancy for bandwidth; do not raise update frequency unless truly needed.

### 6.3 Client animation cost controls
- `SkeletalMeshComponent.VisibilityBasedAnimTickOption = OnlyTickPoseWhenRendered`
- `SkeletalMeshComponent.bEnableUpdateRateOptimizations = true`
- Enable **Animation Budget Allocator** in project settings and ensure meshes register automatically (Engine feature).

---

## 7) SPAWN PATH & BATCHING (50+ NPCs)

**Task:** Avoid spawn spikes and streaming hitches on clients.

- Use `SpawnActorDeferred<APACS_NPCCharacter>` → assign `NPCConfigAsset` → `FinishSpawning`.  
- Batch spawns over multiple frames (e.g., 10–20 per frame) with a short `FTimerManager` loop.  
- For common appearances, pre‑request their soft IDs a few seconds before the spawn wave.

---

## 8) LATE‑JOINER GUARANTEE

**Design guarantee:** Because `VisualConfig` is **COND_InitialOnly**, every client receives it in the **initial bunch** when the actor becomes relevant and the channel opens. `OnRep_VisualConfig` then runs and applies visuals. No RPCs required.

**Rules to preserve this:**
- Do not mutate `VisualConfig` at runtime. If you must change visuals, **wake from dormancy** (`FlushNetDormancy(); SetNetDormancy(DORM_Awake)`) and replicate a *new* config (not `InitialOnly`), then optionally go dormant again.

---

## 9) OPTIONAL: LISTEN SERVER / PIE VISUALS

If you want visuals while developing on a listen server/PIE:
- In `BeginPlay()` after server validation:
  ```cpp
  if (HasAuthority() && !IsRunningDedicatedServer())
  {
    // Apply client visuals locally for testing only
    ApplyVisuals_Client();
  }
  ```
Do **not** load or assign meshes on a dedicated server build.

---

## 10) ACCEPTANCE CHECKLIST (Claude must verify)

1. `UPACS_NPCConfig` exists, has soft IDs, implements `IsDataValid()` and `ToVisualConfig(...)`.  
2. `FPACS_NPCVisualConfig` exists, NetSerializes, equality works, used as **one** replicated field with `COND_InitialOnly` and `REPNOTIFY_OnChanged`.  
3. `APACS_NPCCharacter`:
   - Replicates `VisualConfig` only (no multicast).  
   - `BeginPlay` server: `checkf(NPCConfigAsset)`, build config, `SetNetDormancy(DORM_Initial)`.  
   - `BeginPlay` client: fast‑path apply if config already valid.  
   - `OnRep_VisualConfig`: async‑load both assets, set Mesh (reinit pose) → Anim class; enable URO & visibility tick options.  
4. No server‑side asset loads. Dedicated server path touches no mesh/anim APIs.  
5. INI defaults for relevancy/frequency set; no per‑instance hardcoding.  
6. Spawn batching implemented where bulk spawns occur.  
7. Packaging: assets cook via Primary Asset rules or map refs; no missing asset at runtime.  
8. Late‑join test passes: joining client sees correct visuals without RPCs.

---

## 11) TEST PLAN (quick functional)

1. **Editor‑time validation:** Create a bad `UPACS_NPCConfig` (missing IDs). Ensure **Data Validation** reports errors.  
2. **Initial spawn (two clients):** Host DS + 2 clients. Spawn 50 NPCs batched over frames. Both clients show correct meshes/anim within a few seconds; logs show no asset errors.  
3. **Late join:** Join a third client after NPCs exist. It must receive visuals correctly (no RPCs).  
4. **Dedicated server memory check:** Verify server does not load any skeletal assets (e.g., with `MemReport`, or logs).  
5. **Perf sanity:** With camera away from the crowd, verify anim updates are throttled (URO/budget in effect).

---

## 12) OUT‑OF‑SCOPE (do not implement now)

- AI Controllers, Behaviour Trees, Perception/EQS. (Future phase)  
- Runtime visual changes. (Future phase; if needed, add a separate mutable config blob.)  
- LOD material swaps or material parameter animation. (Future phase)

---

## 13) FILES TO TOUCH (summary)

- `Source/POLAIR_CS/Public/Data/Configs/PACS_NPCConfig.h` + `Private/Data/Configs/PACS_NPCConfig.cpp`  
- `Source/POLAIR_CS/Public/Data/PACS_NPCVisualConfig.h` (struct)  
- `Source/POLAIR_CS/Public/Pawns/NPC/PACS_NPCCharacter.h` + `Private/Pawns/NPC/PACS_NPCCharacter.cpp`  
- `Config/DefaultGame.ini` (class defaults; relevancy/frequency)  
- Optionally: a small preload subsystem if you expect burst spawns of identical appearances.

---

### Notes for Claude Code
- Follow Unreal coding conventions; forward‑declare where possible, minimal includes in headers.  
- All new code must compile without engine source edits.  
- Add comments explaining **why** (authority/dormancy/initial‑only), not just what.  
- Keep the implementation tight: no extra features.
