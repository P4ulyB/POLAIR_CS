---
name: fix-disabled
description: Recover and fix a disabled file
---

Fix the disabled file: $ARGUMENTS

Steps to recover the disabled file:

1. Read the disabled file to understand the implementation
2. Use MCP tool `search_ue_patterns` to find Epic patterns for the functionality
3. Use `epic-source-reader` to check how Epic implements similar features
4. Identify why the file was disabled (check compile errors)
5. Create a new working version that:
   - Follows Epic source patterns
   - Includes proper HasAuthority() checks
   - Uses correct UFUNCTION macros
   - Implements proper replication
6. Rename from .disabled to .cpp
7. Compile to verify it works
8. Update `project_intelligence.md` to mark file as recovered

Focus on Epic compliance and Australian English throughout.