---
name: validate-epic
description: Validate code against Epic source patterns
---

Please validate the current implementation against Epic source patterns:

1. Use MCP tool `search_ue_patterns` to find relevant Epic patterns
2. Use `epic-source-reader` agent to check Actor.cpp for HasAuthority patterns
3. Use MCP tool `validate_pa_policies` to check policy compliance
4. Check for these specific patterns:
   - HasAuthority() usage in mutating methods
   - DOREPLIFETIME macros in replicated classes
   - UFUNCTION qualifiers for RPCs
   - Australian English in comments

Report any violations and suggest Epic-compliant fixes.