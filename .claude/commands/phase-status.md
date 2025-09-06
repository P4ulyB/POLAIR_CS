---
name: phase-status
description: Get comprehensive phase status and next steps
---

Provide a comprehensive phase status report:

1. Check current phase status:
   - Read `docs/project_progress.md`
   - Read `docs/project_state.json`
   - List completion percentages

2. Identify blocked/disabled files:
   - Read `docs/project_intelligence.md`
   - Count disabled files: `ls Source/**/*.disabled`
   - List which files are blocking progress

3. Use `project-manager` agent to:
   - Evaluate if we can proceed to next phase
   - Identify critical blockers
   - Make go/no-go decision

4. Next steps:
   - List immediate tasks to unblock progress
   - Prioritize disabled file recovery
   - Estimate time to next phase

5. Performance check:
   - Are we meeting 90 FPS VR target?
   - Network usage under 100KB/s?
   - Memory per actor under 1MB?

Provide clear status and actionable next steps.