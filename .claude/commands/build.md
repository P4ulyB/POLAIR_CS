---
description: Build UE5 project and run PA validation
argument-hint: 
---

# Build and Validate UE5 Project

Execute UE5 build process and run PA validation automatically.

## Build Process:
1. Trigger UE5 build using UnrealBuildTool
2. Monitor build output for errors
3. If build succeeds, run PA validation
4. Report results and suggest fixes if needed

## Build Command:
Execute the following build sequence:

```bash
# Navigate to project root
cd /mnt/c/Devops/Projects/Polair

# Trigger UE5 build via Windows command
cmd.exe /c "C:\Devops\UESource\Engine\Build\BatchFiles\Build.bat PolairEditor Win64 Development -project=C:\Devops\Projects\Polair\Polair.uproject"

# If build succeeds, run PA validation
if [ $? -eq 0 ]; then
    echo "Build successful, running PA validation..."
    ./Build/PA_UE5Integration.bat
else
    echo "Build failed, analyzing errors..."
    # Analyze build log for common issues
fi
```

## Post-Build Actions:
- Review build log for errors or warnings
- Check PA validation results
- Report Epic source compliance status
- Suggest fixes for any issues found
- Update project documentation if needed

## Expected Output:
Display build status, validation results, and any recommended actions for the developer.