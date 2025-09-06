@echo off
REM ================================================================================================
REM POLAIR_CS Dedicated Server Shipping Build Script for PlayFab Deployment
REM ================================================================================================

echo [INFO] Starting PlayFab Dedicated Server Shipping Build...
echo [INFO] Project: POLAIR_CS
echo [INFO] Configuration: Shipping Dedicated Server
echo [INFO] Target Platform: Windows 64-bit
echo [INFO] Output Directory: Target/Server
echo.

REM Set project paths
set PROJECT_DIR=C:\Devops\Projects\POLAIR_CS
set PROJECT_FILE=%PROJECT_DIR%\POLAIR_CS.uproject
set UE_SOURCE_DIR=C:\Devops\UESource
set RUNUAT_PATH=%UE_SOURCE_DIR%\Engine\Build\BatchFiles\RunUAT.bat
set OUTPUT_DIR=%PROJECT_DIR%\Target\Server

REM Validate paths
if not exist "%PROJECT_FILE%" (
    echo [ERROR] Project file not found: %PROJECT_FILE%
    pause
    exit /b 1
)

if not exist "%RUNUAT_PATH%" (
    echo [ERROR] RunUAT.bat not found: %RUNUAT_PATH%
    pause
    exit /b 1
)

REM Clean only Target/Server directory (preserve other builds like client)
echo [INFO] Cleaning Target/Server directory (deleting server build contents only)...
if exist "%OUTPUT_DIR%" (
    echo [INFO] Removing all files and folders in %OUTPUT_DIR%
    rmdir /s /q "%OUTPUT_DIR%"
)
echo [INFO] Creating fresh Target/Server directory...
mkdir "%OUTPUT_DIR%"

REM Execute PlayFab Dedicated Server Shipping Build
echo [INFO] Executing RunUAT BuildCookRun for PlayFab Dedicated Server...
echo [COMMAND] "%RUNUAT_PATH%" BuildCookRun -project="%PROJECT_FILE%" -noP4 -platform=Win64 -serverconfig=Shipping -cook -server -serverplatform=Win64 -noclient -build -stage -pak -archive -archivedirectory="%OUTPUT_DIR%"

cd /d "%PROJECT_DIR%"

"%RUNUAT_PATH%" BuildCookRun -project="%PROJECT_FILE%" -noP4 -platform=Win64 -serverconfig=Shipping -cook -server -serverplatform=Win64 -noclient -build -stage -pak -archive -archivedirectory="%OUTPUT_DIR%"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed with exit code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] PlayFab Dedicated Server Shipping Build completed successfully!
echo [INFO] Build output location: %OUTPUT_DIR%
echo [INFO] Ready for PlayFab deployment packaging.
echo.
pause