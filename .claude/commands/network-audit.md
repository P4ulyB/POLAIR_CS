---
name: network-audit
description: Audit networking code for authority and replication
---

Perform comprehensive network code audit:

1. Find all networking code:
   - Search for files with HasAuthority, DOREPLIFETIME, UFUNCTION
   - List all RPC implementations
   - Find replication setup code

2. Use MCP tools:
   - `analyze_networking_code` on each network file
   - `get_networking_patterns` for best practices
   - `validate_pa_policies` with networking category

3. Authority checks:
   - Verify every mutating method has HasAuthority()
   - Check for client-side state changes
   - Validate server RPC security

4. Replication audit:
   - Check all UPROPERTY have proper replication flags
   - Verify RepNotify functions are atomic
   - Validate COND_SkipOwner usage

5. Performance:
   - Calculate bandwidth usage
   - Check for unnecessary replication
   - Verify <100KB/s per client target

Report violations with Epic-compliant solutions.