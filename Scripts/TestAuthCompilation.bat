@echo off
REM ================================================================================================
REM PACS Authentication System Compilation Test Script
REM ================================================================================================

echo [TEST] Starting PACS Authentication System Compilation Test...
echo [TEST] Testing compilation of authentication components
echo.

REM Set project paths
set PROJECT_DIR=C:\Devops\Projects\POLAIR_CS
set PROJECT_FILE=%PROJECT_DIR%\POLAIR_CS.uproject
set UE_SOURCE_DIR=C:\Devops\UESource
set BUILD_PATH=%UE_SOURCE_DIR%\Engine\Build\BatchFiles\Build.bat

REM Validate paths
if not exist "%PROJECT_FILE%" (
    echo [ERROR] Project file not found: %PROJECT_FILE%
    pause
    exit /b 1
)

if not exist "%BUILD_PATH%" (
    echo [ERROR] Build.bat not found: %BUILD_PATH%
    pause
    exit /b 1
)

REM Clean intermediate files for authentication module
echo [TEST] Cleaning intermediate files for authentication module...
if exist "%PROJECT_DIR%\Intermediate\Build\Win64\UnrealEditor\Development\POLAIR_CS" (
    echo [TEST] Removing intermediate build files...
    rmdir /s /q "%PROJECT_DIR%\Intermediate\Build\Win64\UnrealEditor\Development\POLAIR_CS"
)

REM Build Editor version to test compilation
echo [TEST] Building Editor version to test compilation...
echo [COMMAND] "%BUILD_PATH%" POLAIR_CSEditor Win64 Development "%PROJECT_FILE%" -WaitMutex -FromMsBuild

cd /d "%PROJECT_DIR%"

"%BUILD_PATH%" POLAIR_CSEditor Win64 Development "%PROJECT_FILE%" -WaitMutex -FromMsBuild

if %ERRORLEVEL% NEQ 0 (
    echo [TEST FAILED] Compilation failed with exit code %ERRORLEVEL%
    echo [TEST] Check the output above for specific compilation errors
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [TEST PASSED] PACS Authentication System compiled successfully!
echo [TEST] All authentication components compiled without errors
echo.
pause