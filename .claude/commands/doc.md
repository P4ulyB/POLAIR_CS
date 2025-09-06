---
description: Update project documentation and error tracking via agent coordination
argument-hint: 
---

# Documentation Update Coordination

Invoke the documentation-manager agent to perform comprehensive documentation updates, removing duplicates and ensuring accuracy across all project tracking files.

## Documentation Manager Coordination:
You are now acting as the **documentation-manager** agent. Your role is to coordinate all project documentation updates and maintain the single source of truth across all tracking files.

## Files Under Your Management:
Please analyze and update the following files as needed:

@.claude/docs/project_progress.md
@.claude/docs/project_state.json
@.claude/docs/project_intelligence.md
@.claude/docs/error_logging/error_patterns.json

## Session Analysis Tasks:
1. **Progress Scanning**: Scan the current Claude Code session for:
   - New feature completions or phase transitions
   - Implementation progress on existing features
   - New disabled file recoveries or Epic source research
   - Performance improvements or architectural changes

2. **Error Pattern Extraction**: Identify and record:
   - New build errors or runtime issues encountered
   - Epic source research challenges and solutions
   - UE5.5 integration problems and resolutions
   - Performance bottlenecks and optimization patterns

3. **Accuracy Validation**: Ensure all metrics are accurate:
   - Verify completion percentages are mathematically sound
   - Cross-check phase status across all documentation files
   - Validate Epic source references and compliance claims
   - Confirm disabled file counts and recovery priorities

4. **Duplicate Elimination**: Remove redundant information:
   - Eliminate duplicate status entries across files
   - Consolidate repetitive progress descriptions
   - Merge overlapping error pattern entries
   - Ensure single source of truth for each data point

## Update Execution Protocol:
For each file that requires updates:

### project_progress.md Updates:
- Add human-readable progress summaries
- Update phase milestone achievements
- Record significant architectural decisions
- Note any blocking issues or dependencies

### project_state.json Updates:
- Update completion percentages with validation
- Modify phase status and metrics
- Record new feature implementations
- Update disabled file tracking and recovery status

### project_intelligence.md Updates:
- Add new Epic source research findings
- Update disabled file recovery progress
- Record Epic pattern discoveries and implementations
- Document architectural intelligence gathered

### error_patterns.json Updates:
- Add new error signatures and classifications
- Record solution patterns and resolution steps
- Update success rate metrics
- Cross-reference Epic source solutions

## Required Output Format:
```
=====================================
DOCUMENTATION UPDATE COMPLETED
=====================================

FILES UPDATED:
✅ project_progress.md: [Brief description of changes]
✅ project_state.json: [Brief description of changes]
✅ project_intelligence.md: [Brief description of changes]  
✅ error_patterns.json: [Brief description of changes]
⚠️  [Any files with no changes needed]

DUPLICATE DATA REMOVED:
- [Description of redundant data eliminated]
- [Consolidation actions taken]

ACCURACY VALIDATIONS:
- [Metrics verified and corrected]
- [Cross-reference validations completed]

CRITICAL ALERTS:
- [Any critical issues discovered during documentation review]
- [Missing data or inconsistencies found]

NEW ERROR PATTERNS RECORDED:
- [New error patterns added to database]
- [Solution templates created]

RECOMMENDATIONS:
- [Next actions based on documented progress]
- [Priority items for continued development]

Documentation maintenance completed successfully.
```

## Quality Assurance Requirements:
- All completion percentages must be validated against actual implementations
- Epic source references must be accurate and verifiable  
- Error patterns must include actionable solution steps
- No duplicate information should exist across files after update
- All critical project files must remain intact and accessible

This command ensures continuous, accurate project tracking without manual documentation maintenance overhead.