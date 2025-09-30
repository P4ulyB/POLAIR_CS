# POLAIR_CS Training Simulation - Claude Code Instructions

<context>
You are assisting with development of POLAIR_CS, a VR training simulation built in Unreal Engine 5.5 for multiplayer dedicated server environments via PlayFab.

## Project Overview
- **Engine:** Unreal Engine 5.5 (Source Build)
- **Platform:** PC + Meta Quest 3 VR
- **Architecture:** Dedicated server with PlayFab hosting
- **Session:** 6-8 hours, 8-10 assessors + 1 VR candidate
- **Environment:** WSL (Claude Code) + Windows (UE5)

## Core Architecture
This is a **server-authoritative multiplayer game** where:
- All state changes must validate `HasAuthority()` first
- Clients send requests via Server RPCs
- Server broadcasts state via RepNotify properties
- VR and non-VR players coexist in the same session
- Object pooling handles NPC spawning for performance
</context>

---

<instructions>

## Development Policies

### 1. Server Authority (CRITICAL)
All mutating methods must start with:
```cpp
if (!HasAuthority())
{
    return;
}
```
Never modify gameplay state on clients. Use Server RPCs for client requests.

### 2. Naming Conventions
- **Classes:** `APACS_ClassName` (Actors), `UPACS_ClassName` (Objects)
- **Structs:** `FPACS_StructName`
- **Enums:** `EPACS_EnumName`
- **Interfaces:** `IPACS_InterfaceName`
- **Consistency:** Follow existing patterns in the codebase

### 3. Performance Targets (Non-Negotiable)
- **VR Frame Rate:** 90 FPS (11.1ms frame budget)
- **Network Bandwidth:** <100KB/s per client
- **Memory per Actor:** <1MB
- **Session Stability:** 6-8 hours continuous operation

Monitor these via:
- `UPACS_MemoryTracker` subsystem
- `UPACS_NetworkMonitor` component
- Console commands: `pacs.ValidatePooling`, `pacs.MeasurePerformance`

### 4. Epic Source Pattern Verification
Before implementing UE5 systems:
1. Use `research_agent` to query Epic source via MCP tools
2. Follow Epic patterns exactly (especially for Replication Graph, Subsystems)
3. Epic source overrides online documentation when conflicts exist
4. Cache research results in `.claude/research_cache/` for reuse

### 5. Code Organization
```
Source/POLAIR_CS/
├── Public/
│   ├── Actors/           # APawn, ACharacter, AActor subclasses
│   ├── Components/       # UActorComponent subclasses
│   ├── Core/             # GameMode, PlayerController, PlayerState, GameInstance
│   ├── Data/             # UDataAsset, config classes
│   ├── Interfaces/       # IInterface definitions
│   ├── Settings/         # UDeveloperSettings
│   └── Subsystems/       # UWorldSubsystem, UGameInstanceSubsystem
└── Private/              # Implementation files (.cpp)
```
Maximum 750 lines per file. Split larger files by responsibility.

### 6. File Operation Guidelines
- **ALWAYS** prefer editing existing files over creating new ones
- **NEVER** proactively create documentation files unless explicitly requested
- **NEVER** use emojis in code or documentation

</instructions>

---

<current_systems>

## Implemented Systems

### Multiplayer Networking
- **PACS_ReplicationGraph:** Spatial grid replication (10km cells, 15km cull distance)
- **Network Monitor:** Bandwidth tracking, spawn batching (<100KB/s enforcement)
- **Authority Checks:** 34 occurrences across codebase
- **Replication:** Compressed vectors (`FVector_NetQuantize`), byte-packed states

### Object Pooling
- **PACS_SpawnOrchestrator:** World subsystem managing all pools (server-only)
- **IPACS_Poolable Interface:** `OnAcquiredFromPool()`, `OnReturnedToPool()`
- **PACS_SpawnConfig:** Data asset defining pool configurations per GameplayTag
- **PACS_SpawnPoint:** Placeable level actors with spawn patterns
- **Status:** Production-ready with async asset loading

### NPC System
- **APACS_NPC_Base:** Abstract base with selection system
- **APACS_NPC_Base_Char:** Character movement NPCs with skeletal mesh
- **APACS_NPC_Base_LW:** Lightweight NPCs (<1MB, FloatingPawnMovement, flat terrain)
- **APACS_NPC_Base_Veh:** Vehicle NPCs (Chaos Vehicles)

### Player Framework
- **APACSGameMode:** Zero-swap pawn spawning, HMD detection, spawn orchestrator init
- **APACS_PlayerController:** VR/HMD management, input system, selection RPCs
- **APACS_PlayerState:** HMD state replication, selection tracking
- **UPACSGameInstance:** Session management, PlayFab integration

### Pawn Types
- **APACS_AssessorPawn:** Top-down spectator with WASD movement, zoom, discrete rotation
- **APACS_CandidateHelicopterCharacter:** VR helicopter with orbit controls, CCTV cameras

### Selection System
- **PACS_SelectionPlaneComponent:** Server state + client visuals
- **VR-Aware:** Automatically excludes visuals on HMD clients
- **Material:** `M_PACS_Selection` with CustomPrimitiveData (CPD indices 0-3)
- **States:** Hovered, Selected, Unavailable, Available, Hidden

### Input System
- **PACS_InputHandlerComponent:** Centralized Enhanced Input management
- **IPACS_InputReceiver Interface:** Priority-based input routing
- **Components:** EdgeScroll, HoverProbe for camera/selection

### Performance Subsystems
- **UPACS_MemoryTracker:** Per-actor profiling, budget compliance (100MB default)
- **UPACS_CustomSignificanceManager:** Distance-based LOD
- **PACSServerKeepaliveSubsystem:** PlayFab heartbeat
- **PACSLaunchArgSubsystem:** Command-line parsing

### VR Features
- **HMD Detection:** Zero-swap pattern with timeout handling
- **HMD State Replication:** `EHMDState` enum replicated via PlayerState
- **VR Optimizations:** Cull distance (5000), reduced tick rates, LOD clamping

</current_systems>

---

<build_configuration>

## Build & Development

### Modules
- **POLAIR_CS:** Main runtime module
- **POLAIR_CSEditor:** Editor-only tools

### Build Targets
- **Client:** `POLAIR_CS.Target.cs` (Type: Game)
- **Server:** `POLAIR_CSServer.Target.cs` (Type: Server)
- **Editor:** `POLAIR_CSEditor.Target.cs`

### Key Dependencies
**Public:** Core, CoreUObject, Engine, InputCore, EnhancedInput, HeadMountedDisplay, UMG, PlayFabGSDK, NetCore, DeveloperSettings, AIModule, NavigationSystem, ReplicationGraph, SignificanceManager, GameplayTags, ChaosVehicles, Niagara

**Plugins:** PlayFab, PlayFabGSDK, OpenXR, OculusXR, ReplicationGraph, SignificanceManager, AnimationBudgetAllocator, ChaosVehiclesPlugin

### Build Commands (WSL)
```bash
# Client build (auto-triggered by hooks after Edit/Write)
cmd.exe /c "C:\Devops\UESource\Engine\Build\BatchFiles\Build.bat" POLAIR_CS Win64 Development

# Server build (manual)
cmd.exe /c "C:\Devops\UESource\Engine\Build\BatchFiles\Build.bat" POLAIR_CSServer Win64 Development
```

### Compilation Hooks
- **Auto-compile** triggers after any `Edit` or `Write` operation
- Build results captured to `/tmp/build_result.log`
- Errors automatically surface with validation suggestions

</build_configuration>

---

<mcp_integration>

## MCP Server for Epic Source Research

**Status:** Connected
**Location:** `C:\Devops\MCP\UEMCP`
**UE5 Source:** `C:\Devops\UESource\Engine\Source`

### Available MCP Tools
- `mcp__ue_mcp_local_wsl__code_search` - Regex/literal search across UE5 source
- `mcp__ue_mcp_local_wsl__zoekt_read` - Read engine files with line ranges
- `mcp__ue_mcp_local_wsl__symbols_find` - Symbol definitions/references
- `mcp__ue_mcp_local_wsl__xrefs_def` - Semantic definition lookup (clangd)
- `mcp__ue_mcp_local_wsl__xrefs_refs` - Semantic references (clangd)
- `mcp__ue_mcp_local_wsl__code_context` - Surrounding context for patterns

### Usage Strategy
1. **ONLY** `research_agent` subagent uses MCP tools directly
2. Cache results in `.claude/research_cache/[pattern-type]-[timestamp].md`
3. Other agents read from cache (no duplicate MCP queries)
4. 24-hour cache expiry

### When to Use MCP
- Implementing new UE5 subsystems, replication patterns, or engine integrations
- Verifying API signatures and usage patterns
- Finding Epic's implementation of similar features
- Performance optimization patterns from engine code

</mcp_integration>

---

<specialized_agents>

## Agent Workflows (Token-Efficient)

### research_agent
**Use when:** Need Epic source patterns, API verification, performance data
**Tools:** MCP tools, web search, filesystem
**Output:** `.claude/research_cache/[pattern]-[timestamp].md`
**Invoke:** User requests "research", need to verify Epic patterns before implementation

### system_architect
**Use when:** Designing new features, systems, or architectural changes
**Tools:** Filesystem, Google Drive search
**Output:** `.claude/handoffs/[feature]-architecture.md`
**Invoke:** User requests "design" or "architecture" for new systems

### code_implementor
**Use when:** Writing C++ implementation from validated designs
**Tools:** Filesystem (no MCP, reads from research cache)
**Output:** Source files + `.claude/handoffs/[feature]-implementation.md`
**Invoke:** Architecture complete, Epic patterns validated

### implementation_validator
**Use when:** Validating compilation, functionality, policy compliance
**Tools:** Bash, filesystem
**Output:** `.claude/handoffs/[feature]-validation.md`
**Invoke:** After compilation completes (manual trigger after build)

### performance_engineer
**Use when:** Deep analysis of frame time, bandwidth, memory
**Tools:** Bash, filesystem
**Output:** `.claude/handoffs/[feature]-performance-analysis.md`
**Invoke:** Performance targets missed, optimization needed

### Typical Workflows
```
Simple Edit:
  User → Edit code → Auto-compile → implementation_validator → Done

New Feature:
  User → system_architect → research_agent → code_implementor →
  Auto-compile → implementation_validator → Done

Performance Issue:
  User → performance_engineer → code_implementor →
  Auto-compile → implementation_validator → Done
```

</specialized_agents>

---

<constraints>

## Critical Constraints

1. **Server Authority:** ALL mutating methods check `HasAuthority()` first
2. **Naming:** PACS_ prefix required for all project classes
3. **Performance:** 90 FPS VR, <100KB/s network, <1MB per actor
4. **Epic Patterns:** Verify via MCP before implementing engine-adjacent code
5. **Multiplayer:** Design for dedicated server (no listen server, no standalone)
6. **File Size:** 750 LOC max per file
7. **No Emojis:** Never use emojis in code or documentation

## Common Pitfalls to Avoid
- ❌ Modifying gameplay state on clients without Server RPC
- ❌ Forgetting `HasAuthority()` checks in mutating methods
- ❌ Creating new files when editing existing ones would suffice
- ❌ Using Epic class names without verifying current API in source
- ❌ Assuming single-player or listen server architecture
- ❌ Creating documentation files proactively

</constraints>

---

<reference_paths>

## Quick Reference

### Documentation
- Performance optimization results: `docs/performance-optimizations-summary.md`
- Selection system guide: `docs/SelectionPlaneSystem_UserGuide.md`
- Agent handoffs: `.claude/handoffs/`
- Research cache: `.claude/research_cache/`

### Key Source Files
- Game mode: `Source/POLAIR_CS/Private/Core/PACSGameMode.cpp`
- Player controller: `Source/POLAIR_CS/Public/Core/PACS_PlayerController.h`
- Spawn orchestrator: `Source/POLAIR_CS/Public/Subsystems/PACS_SpawnOrchestrator.h`
- Replication graph: `Source/POLAIR_CS/Public/Core/PACS_ReplicationGraph.h`
- Network monitor: `Source/POLAIR_CS/Public/Components/PACS_NetworkMonitor.h`
- Lightweight NPC: `Source/POLAIR_CS/Public/Actors/NPC/PACS_NPC_Base_LW.h`

### Build Logs
- Compilation results: `/tmp/build_result.log`
- UE5 logs: `Saved/Logs/`

### Tools
- Unreal Insights: `C:\Devops\UESource\Engine\Binaries\Win64\UnrealInsights.exe`
- Build tool: `C:\Devops\UESource\Engine\Build\BatchFiles\Build.bat`

</reference_paths>

---

## Notes for Claude Code

- This codebase is **production-ready** with mature networking, pooling, and VR systems
- Focus on **editing existing files** rather than creating new ones
- **Always verify Epic patterns** via research_agent before implementing UE5 systems
- **Performance targets are non-negotiable** - validate before committing
- Use **specialized agents** to maintain token efficiency
- The project follows **Anthropic best practices** for prompt engineering and structure