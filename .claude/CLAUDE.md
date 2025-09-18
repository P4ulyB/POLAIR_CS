# POLAIR_CS Training Simulation - Claude Code Configuration

## Quick Navigation (Source of Truth)
- **Current Status:** `docs/project_progress.md`
- **Project Metadata:** `docs/project_state.json`
- **Epic Research:** `docs/project_intelligence.md`
- **Phase Guides:** `docs/phases/`
- **Error Patterns:** `docs/error_logging/error_patterns.json`
- **MCP Status:** Connected - UE5 Semantic Analysis Server

## Configuration
- **max_messages**: 40  # Limit conversation history to reduce token usage

## Project Context
**Engine:** Unreal Engine 5.5 (Source Build)  
**Platform:** PC + Meta Quest 3 VR  
**Architecture:** Dedicated servers via PlayFab  
**Session:** 6-8 hours, 8-10 assessors + 1 VR candidate
**Environment:** WSL + Windows hybrid development (WSL for Claude Code, Windows for UE)

## MCP Server Integration
**Status:** Connected and operational
**Location:** `C:\Devops\MCP\UEMCP`

### Available MCP Tools:
- `ue-mcp-local:code_search` - Search UE5 codebase for patterns
- `ue-mcp-local:zoekt_read` - Read engine source files with context
- `ue-mcp-local:symbols_find` - Find symbol definitions and references
- `ue-mcp-local:xrefs_def` - Semantic definition lookup via clangd LSP
- `ue-mcp-local:xrefs_refs` - Semantic references via clangd
- `ue-mcp-local:code_context` - Get surrounding context lines for patterns

### MCP Usage Strategy:
**SINGLE INTERFACE:** Only `research_agent` uses MCP tools directly
**TOKEN EFFICIENCY:** All other agents request research from `research_agent` cache
**CACHE REUSE:** Research results stored in `.claude/research_cache/` for multiple agent access

## Core Development Policies

### Authority & Epic Source Verification
- Check UE5.5 source via research_agent before implementing
- Direct source access: `C:\Devops\UESource\Engine\Source`
- Call `HasAuthority()` at start of every mutating method
- Server-authoritative for all state changes
- Epic source patterns override documentation when conflicts exist

### Performance Targets (Critical)
- **VR:** 90 FPS minimum (Meta Quest 3)
- **Network:** <100KB/s per client
- **Memory:** <1MB per actor
- **Session:** 6-8 hour stability

### Code Standards
- **Prefix:** PACS_ClassName convention
- **Structure:** Subsystems/, Core/, Components/, Actors/, UI/
- **Files:** 750 LOC max per file
- **Emojis:** Never use emojis under any circumstances

## Streamlined Agent Architecture (7 Agents)

### 1. system_architect
**Role:** Designs UE5.5 C++ system architecture and class structures  
**Tools:** Filesystem operations, Google Drive search  
**Triggers:** User requests new features, systems, or architectural planning  
**Output:** `.claude/handoffs/[feature-name]-architecture.md`  
**Next:** research_agent for Epic pattern validation

### 2. research_agent  
**Role:** Single MCP interface for all UE5 Epic pattern research  
**Tools:** All MCP tools + web search  
**Triggers:** Any agent needs Epic source patterns, performance data, API verification  
**Output:** `.claude/research_cache/[pattern-type]-[timestamp].md`  
**Next:** code_implementor or requesting agent

### 3. code_implementor
**Role:** Implements C++ code from validated designs and Epic patterns  
**Tools:** Filesystem operations (no MCP - reads from research cache)  
**Triggers:** Architecture complete and research patterns available  
**Output:** Source files + `.claude/handoffs/[feature-name]-implementation.md`  
**Next:** implementation_validator (triggered by compilation hook)

### 4. implementation_validator
**Role:** Validates compilation, functionality, and policy compliance  
**Tools:** Bash, filesystem operations  
**Triggers:** Code implementation complete (manual after compilation)  
**Output:** `.claude/handoffs/[feature-name]-validation.md`  
**Next:** project_manager or code_implementor for fixes

### 5. performance_engineer
**Role:** Analyzes performance against VR 90 FPS, 100KB/s network, 1MB memory targets  
**Tools:** Bash, filesystem operations  
**Triggers:** Deep performance analysis needed  
**Output:** `.claude/handoffs/[feature-name]-performance-analysis.md`  
**Next:** code_implementor for fixes or system_architect for redesign

### 6. project_manager
**Role:** Phase completion decisions and milestone validation  
**Tools:** Filesystem operations, documentation access  
**Triggers:** Validation complete, phase gate decisions needed  
**Output:** Phase completion decisions and next milestone planning  
**Next:** documentation_manager or next phase agents

### 7. documentation_manager
**Role:** Maintains project documentation files  
**Tools:** Filesystem operations  
**Triggers:** Project state changes, phase completions  
**Output:** Updated project_progress.md, project_state.json, error_patterns.json  
**Next:** Session complete

## Automated Hook System

### PostToolUse Hooks:
```json
{
  "matcher": "Edit|Write",
  "hooks": [{
    "command": "Auto-compile + suggest implementation_validator"
  }]
}
```
- **Triggers:** Any file edit/write operation
- **Action:** Automatic UE5.5 compilation with error capture to `/tmp/build_result.log`
- **Output:** Success/failure message + validation suggestion

### MCP Tracking Hook:
```json
{
  "matcher": "mcp__ue-mcp-local__.*", 
  "hooks": [{
    "command": "Confirm Epic pattern research completed"
  }]
}
```

### Session Completion Hook:
- **Triggers:** Session stop
- **Reminds:** Use implementation_validator, performance_engineer, commit changes

## Agent Workflow Patterns

### Simple Implementation (15K tokens):
```
User Request → code_implementor → Auto-compile → implementation_validator → Complete
```

### Design + Implementation (35K tokens):
```
User Request → system_architect → research_agent → code_implementor → Auto-compile → implementation_validator → Complete
```

### Performance Analysis (20K tokens):
```
User Request → performance_engineer → code_implementor → Auto-compile → implementation_validator → Complete
```

### Epic Pattern Research (8K tokens, reusable):
```
User Request → research_agent → Cache Result → Multiple agents reuse
```

## Token Efficiency Features

### Research Cache System:
- **Location:** `.claude/research_cache/`
- **Format:** `[pattern-type]-[timestamp].md`
- **Reuse:** Multiple agents read same research without duplicate MCP calls
- **Expiry:** 24-hour cache invalidation

### Handoff Documents:
- **Location:** `.claude/handoffs/`
- **Format:** `[feature-name]-[stage].md`
- **Flow:** system_architect → research_agent → code_implementor → implementation_validator

### Hook Automation:
- **Auto-compilation:** Eliminates manual build commands
- **Next-step suggestions:** Reduces coordination overhead
- **Error capture:** Automatic build log generation for validation

## Quick Commands & Aliases

### Build Commands:
- **`build`** - Compile client (auto-triggered by hooks)
- **`buildserver`** - Compile dedicated server manually

### Agent Commands:
- **`validate`** - Invoke implementation_validator
- **`perf`** - Invoke performance_engineer  
- **`research`** - Invoke research_agent

### Analysis Commands:
- **`check-vr`** - Scan for VR code, suggest performance analysis
- **`check-authority`** - Scan for HasAuthority patterns, suggest validation

## Development Environment

### WSL Integration:
- Claude Code runs in WSL
- Project files: `/mnt/c/Devops/Projects/POLAIR_CS`
- Build tools: Windows via `cmd.exe /c`
- Path translation handled automatically

### MCP Server:
- **Windows Location:** `C:\Devops\MCP\UEMCP`
- **WSL Access:** Via MCP protocol
- **Research Cache:** Shared between WSL and Windows

## Error Handling

### Compilation Failures:
1. **Auto-detected** by compilation hook
2. **Log captured** to `/tmp/build_result.log`
3. **Suggestion provided** to use implementation_validator
4. **Manual analysis** required for fixes

### MCP Connection Issues:
- **Check MCP server** running in Windows
- **Verify indexes** in MCP/indexes directory  
- **Use `test-mcp` alias** to verify connection

### Agent Coordination:
- **File-based handoffs** prevent coordination failures
- **Research cache** eliminates duplicate MCP queries
- **Hook suggestions** provide workflow guidance
- **Manual invocation** maintains control and token efficiency

## Performance Monitoring

### Automatic Checks:
- **VR Code Detection:** `check-vr` command
- **Authority Patterns:** `check-authority` command
- **Memory Footprint:** performance_engineer analysis
- **Network Bandwidth:** replication pattern validation

### Targets Enforcement:
- **90 FPS:** Meta Quest 3 VR performance
- **100KB/s:** Network bandwidth per client
- **1MB:** Memory footprint per actor
- **6-8 hours:** Session stability requirement

## Known Limitations

### Hook System:
- **No file path filtering** - hooks trigger on all edits
- **No SubagentStop events** - manual agent coordination required
- **No conditional logic** - same hook for success/failure

### Workarounds:
- **Manual agent invocation** maintains token efficiency
- **Research cache** prevents duplicate MCP calls
- **Enhanced aliases** provide specific functionality
- **Session hooks** remind of workflow steps

This architecture provides **maximum token efficiency** while maintaining **full Epic pattern compliance** and **automated compilation feedback** for the POLAIR_CS VR training simulation.
