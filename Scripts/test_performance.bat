@echo off
echo ========================================
echo POLAIR_CS Performance Test Script
echo ========================================
echo.

echo Starting Unreal Editor with performance testing...
echo.

REM Launch editor with our test map
"C:\Devops\UESource\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Devops\Projects\POLAIR_CS\POLAIR_CS.uproject" -log -game

echo.
echo ========================================
echo Testing Console Commands:
echo ========================================
echo.
echo Once in the editor, use these commands:
echo.
echo 1. pacs.ValidatePooling
echo    - Validates character pooling system
echo.
echo 2. pacs.MeasurePerformance
echo    - Measures NPC performance metrics
echo.
echo 3. stat fps
echo    - Shows frame rate statistics
echo.
echo 4. stat unit
echo    - Shows frame time breakdown
echo.
echo 5. stat game
echo    - Shows game thread timings
echo.
echo ========================================
echo Press any key to exit...
pause >nul