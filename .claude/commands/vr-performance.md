---
name: vr-performance
description: Check VR performance critical code
---

Analyze VR performance for Meta Quest 3 compatibility:

1. Search for VR-related code:
   - Find all VR-specific implementations
   - Check frame timing code
   - Review comfort settings

2. Use `performance-engineer` agent to:
   - Check if we're meeting 90 FPS target
   - Identify performance bottlenecks
   - Review motion-to-photon latency

3. Use MCP tool to find Epic VR patterns:
   - Search: `search_ue_patterns` for "VR performance optimization"
   - Check Epic's VR best practices

4. Specific checks:
   - Verify smooth interpolation for assessor-controlled movement
   - Check HMD tracking implementation
   - Review passenger perspective code

5. Memory analysis:
   - Check VR-specific memory allocations
   - Verify cleanup during candidate transitions

Report any violations of the 90 FPS target with specific recommendations.