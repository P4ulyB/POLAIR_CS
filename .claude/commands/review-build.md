---
name: review-build
description: Review latest build and validate compilation
---

Review the latest build and perform comprehensive validation:

1. Check build status:
   - Read `.claude/logs/build/latest_build.json`
   - List any compilation errors or warnings

2. Use `wsl-build-tester` agent to:
   - Compile the project: `cmd.exe /c "msbuild.exe C:\\Devops\\Projects\\Polair\\Polair.sln"`
   - Check for missing includes
   - Verify linker success

3. Use MCP tools to validate changed files:
   - Run `validate_pa_policies` on modified C++ files
   - Check Epic pattern compliance

4. Update documentation:
   - Use `documentation-manager` to update `project_progress.md` if needed
   - Log any new errors to `error_patterns.json`

Report: Build status, errors found, and recommendations.