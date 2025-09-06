---
description: Initialize Claude Code session with current project scope and state
argument-hint: 
---

# Project Session Initialization

Read and inject the current project context to initialize the Claude Code session with full project scope and state.

## Required Files to Read:
Please read the following files to understand the current project state:

@.claude/CLAUDE.md
@.claude/docs/project_state.json

## Session Context Summary:
After reading both files, provide a comprehensive session initialization summary including:

### Current Project Status:
- Current phase and completion percentage
- Critical issues requiring attention (disabled files, blockers)
- Recent progress and achievements

### Development Environment:
- Engine version (UE5.5) and platform targets
- MCP server connectivity status
- Available development tools and agents

### Available Agent Capabilities:
- List all available agents and their roles
- Highlight key agents for current phase work
- Show agent coordination hierarchy

### Available MCP Tools:
- Display connected MCP server tools
- Show Epic source research capabilities
- Confirm networking pattern analysis tools

### Immediate Actions:
- Extract recommended immediate actions from project_state.json
- Show current phase requirements and next steps
- Identify priority tasks for this session

### Critical Files Status:
- Verify all critical project files are present
- Check documentation consistency
- Flag any missing or corrupted essential files

## Expected Output Format:
```
=====================================
POLAIR PROJECT SESSION INITIALIZED
=====================================

CURRENT STATUS:
- Phase: [Current Phase] ([Completion %])
- Engine: Unreal Engine 5.5 (Source Build)
- Platform: PC + Meta Quest 3 VR
- MCP Server: [Status] ([Tool Count] tools available)

CRITICAL ISSUES:
- [List any critical blockers or disabled files]
- [Priority recovery items]

AVAILABLE AGENTS:
- documentation-manager: [Role description]
- PA_CodeImplementor: [Role description]
- PA_Architect: [Role description]
- [... other agents with their capabilities]

AVAILABLE MCP TOOLS:
- search_ue_patterns: Epic source pattern search
- get_epic_pattern: Specific implementation patterns
- [... other MCP tools]

IMMEDIATE ACTIONS:
- [Action 1 from project_state.json recommendations]
- [Action 2 from project_state.json recommendations]
- [Action 3 from project_state.json recommendations]

Ready for development session.
```

This command establishes full project context at the start of each Claude Code session, eliminating the need for manual context setup.