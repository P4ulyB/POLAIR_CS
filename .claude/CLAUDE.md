# Polair Training Simulation - Claude Code Configuration

## Quick Navigation (Source of Truth)
- **Current Status:** `docs/project_progress.md`
- **Project Metadata:** `docs/project_state.json`
- **Epic Research:** `docs/project_intelligence.md`  
- **Phase Guides:** `docs/phases/`
- **Error Patterns:** `docs/error_logging/error_patterns.json`
- **MCP Status:** Connected (5 tools available)

## Configuration
- **max_messages**: 40  # Limit conversation history to reduce token usage

## Project Context
**Engine:** Unreal Engine 5.5 (Source Build)  
**Platform:** PC + Meta Quest 3 VR  
**Architecture:** Dedicated servers via PlayFab  
**Session:** 6-8 hours, 8-10 assessors + 1 VR candidate
**Environment:** WSL + Windows hybrid development (WSL for claude code, Windows for Unreal Engine project)

## Documentation Files (All Connected)

### Core Status Tracking:
- **project_progress.md** - Human-readable phase status and milestones
- **project_state.json** - Machine-readable metrics and completion data
- **project_intelligence.md** - Epic source research and disabled file tracking
- **error_patterns.json** - Error recording and solution database

### How Agents Use Documentation:
- **documentation-manager:** Updates all 4 core files
- **project-manager:** Reads project_progress.md for phase decisions
- **All agents:** Can query documentation-manager for any file data

## MCP Server Integration
**Status:** Connected and operational
**Location:** `C:\Devops\MCP\UE_Semantic_Analysis_MCP_Server`

### Available MCP Tools:
1. `search_ue_patterns` - Search UE codebase for patterns
2. `get_epic_pattern` - Get specific Epic implementation patterns
3. `validate_pa_policies` - Validate code against 22-point framework
4. `get_networking_patterns` - Get networking/replication patterns
5. `analyze_networking_code` - Analyze network code compliance

### MCP Usage Example:
```javascript
// Search for Epic patterns
await use_mcp_tool("ue-semantic", "search_ue_patterns", {
  query: "HasAuthority",
  context: "networking",
  requireEpicSource: true
});
```

## Core Development Policies

### Authority & Epic Source Verification
- Check UE5.5 source via MCP tools before implementing
- Direct source access: `C:\Devops\UESource\Engine\Source`
- Call `HasAuthority()` at start of every mutating method
- Server-authoritative for all state changes
- Epic source patterns override documentation when clear

### Performance Targets (Critical)
- **VR:** 90 FPS minimum (Meta Quest 3)
- **Network:** <100KB/s per client
- **Memory:** <1MB per actor
- **Session:** 6-8 hour stability

### Code Standards
- **Prefix:** PACS_ClassName convention
- **Structure:** Subsystems/, Core/, Components/, Actors/, UI/
- **Files:** 750 LOC max per file

## Agent Architecture (Token-Optimised Delegation)

### Design Phase Agents

#### PA_Architect
Designs UE5.5 system architecture and class structures. Creates implementation plans with Epic pattern specifications. Outputs handoff documents for validation and implementation.

#### epic-source-reader
Reads UE5 source files directly. Extracts exact Epic implementation patterns with line numbers. Validates patterns against engine source code. Works with MCP for semantic analysis.

### Implementation Phase Agents

#### PA_CodeImplementor
Implements C++ code from validated designs only. Uses pre-verified Epic patterns without modification. Ensures compilation and PA_ naming conventions. Focuses purely on code implementation.

#### code-reviewer
Reviews completed C++ implementations for compilation success, authority patterns, and Australian English compliance. Validates against UE5.5 standards and performance requirements.

### Testing & Validation Agents

#### qa-tester
Tests implementations for compilation, functionality, and runtime stability. Validates core features and network behaviour. Reports specific errors with locations and severity.

#### wsl-build-tester
Compiles UE5 projects from WSL using Windows build tools. Handles path translation and reports compilation results. Specialises in WSL environment integration.

#### performance-engineer
Monitors performance metrics against specific targets. Flags frame rate, memory, and network violations. Provides targeted analysis of performance bottlenecks.

### Management Agents

#### project-manager
Validates phase completion criteria and makes go/no-go decisions for phase transitions. Focuses on phase gates without daily coordination. Ensures targets are met before advancement.

#### documentation-manager
Maintains project documentation files. Updates progress tracking, error patterns, and research findings. Single source for all documentation updates.

#### research-agent
Research technical questions using ONLY official documentation via WebFetch. Never speculate or fill knowledge gaps with assumptions. Provide exact quotes and source URLs. Triggered by `-research` flag or technical deployment questions.

## Agent Delegation Workflow

### Simple Implementation Tasks
```
User Request → PA_CodeImplementor → code-reviewer → Complete
(15K tokens estimated)
```

### Design + Implementation Tasks
```
User Request → PA_Architect → epic-source-reader → PA_CodeImplementor → qa-tester → Complete
(35K tokens estimated)
```

### Epic Pattern Research
```
User Request → epic-source-reader → Cache Result → Complete
(8K tokens, reusable)
```

### Performance Analysis
```
User Request → performance-engineer → PA_CodeImplementor → qa-tester → Complete
(20K tokens estimated)
```

## Development Environment

### WSL Integration
- Claude Code runs in WSL
- Project files at: `/mnt/c/Devops/Projects/Polair`
- Build tools accessed via: `cmd.exe /c` or `powershell.exe`
- Path translation required for Windows tools

### Build Commands (from WSL)
```bash
# Compile via Windows tools from WSL
cmd.exe /c "msbuild.exe C:\\Devops\\Projects\\Polair\\Polair.sln"

# Generate project files
cmd.exe /c "C:\\Program Files\\Epic Games\\UE_5.5\\Engine\\Build\\BatchFiles\\GenerateProjectFiles.bat"
```
## Workflow Integration

### Token-Efficient Development Process:
1. **Task Analysis:** Determine complexity and required agents
2. **Agent Selection:** Use minimal agent set for task type
3. **Sequential Handoffs:** Agents work independently with file-based handoffs
4. **Quality Gates:** Each agent validates within their specialty
5. **Documentation:** Track completion in appropriate files

### Epic Pattern Verification Flow:
1. **MCP Search:** Use `search_ue_patterns` to find patterns
2. **Source Read:** Use `epic-source-reader` to get exact implementation
3. **Validation:** Use `validate_pa_policies` to check compliance
4. **Implementation:** Use `PA_CodeImplementor` for focused coding
5. **Testing:** Use `qa-tester` and `wsl-build-tester` for validation

### Handoff Document System:
- **Location:** `.claude/handoffs/[feature-name]-[stage].md`
- **Design Phase:** PA_Architect creates implementation specifications
- **Validation Phase:** epic-source-reader updates with verified patterns
- **Implementation Phase:** PA_CodeImplementor produces working code
- **Testing Phase:** qa-tester validates functionality

## File Operations Mode
When working with files:
- Use minimal context for basic operations
- Focus on filesystem tasks only
- Skip complex validation for simple file edits
- Use WSL paths: `/mnt/c/Devops/Projects/Polair`

## Quick Commands

### Check Project Status:
```
Use documentation-manager to read project_progress.md
```

### Design New Feature:
```
Use PA_Architect to create system design and handoff document
```

### Validate Epic Patterns:
```
Use epic-source-reader to verify patterns against UE5.5 source
```

### Implement Code:
```
Use PA_CodeImplementor to write C++ from validated design
```

### Test Implementation:
```
Use qa-tester for functionality and wsl-build-tester for compilation
```

### Update Documentation:
```
Use documentation-manager to update appropriate project files
```

## Known Issues & Solutions

### MCP Returns 0 Results:
- Ensure MCP server is running in Windows
- Check indexes exist in MCP/indexes directory
- Rebuild indexes: `npm run rebuild-indexes`

### Build Tools Not Found:
- Use `cmd.exe /c` prefix for Windows tools
- Ensure VS2022 and UE5.5 installed
- Check paths in build commands

### Path Issues in WSL:
- Windows: `C:\Devops\Projects\Polair`
- WSL: `/mnt/c/Devops/Projects/Polair`
- Use path translation functions when needed

### Agent Coordination:
- No central coordinator needed
- File-based handoffs eliminate coordination overhead
- Each agent reads previous output and produces focused results
- Documentation-manager tracks overall progress