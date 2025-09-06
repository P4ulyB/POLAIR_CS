@echo off
REM ================================================================================================
REM POLAIR_CS Client Shipping Build Script for Distribution
REM ================================================================================================

echo [INFO] Starting Client Shipping Build...
echo [INFO] Project: POLAIR_CS
echo [INFO] Configuration: Shipping Client
echo [INFO] Target Platform: Windows 64-bit
echo [INFO] Output Directory: Target/Client
echo.

REM Set project paths
set PROJECT_DIR=C:\Devops\Projects\POLAIR_CS
set PROJECT_FILE=%PROJECT_DIR%\POLAIR_CS.uproject
set UE_SOURCE_DIR=C:\Devops\UESource
set RUNUAT_PATH=%UE_SOURCE_DIR%\Engine\Build\BatchFiles\RunUAT.bat
set OUTPUT_DIR=%PROJECT_DIR%\Target\Client

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

REM Clean only Target/Client directory (preserve other builds like server)
echo [INFO] Cleaning Target/Client directory (deleting client build contents only)...
if exist "%OUTPUT_DIR%" (
    echo [INFO] Removing all files and folders in %OUTPUT_DIR%
    rmdir /s /q "%OUTPUT_DIR%"
)
echo [INFO] Creating fresh Target/Client directory...
mkdir "%OUTPUT_DIR%"

REM Execute Client Shipping Build
echo [INFO] Executing RunUAT BuildCookRun for Client...
echo [COMMAND] "%RUNUAT_PATH%" BuildCookRun -project="%PROJECT_FILE%" -noP4 -platform=Win64 -configuration=Shipping -cook -build -stage -pak -archive -archivedirectory="%OUTPUT_DIR%" -target=POLAIR_CS

cd /d "%PROJECT_DIR%"

"%RUNUAT_PATH%" BuildCookRun -project="%PROJECT_FILE%" -noP4 -platform=Win64 -configuration=Shipping -cook -build -stage -pak -archive -archivedirectory="%OUTPUT_DIR%" -target=POLAIR_CS

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed with exit code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Client Shipping Build completed successfully!
echo [INFO] Build output location: %OUTPUT_DIR%
echo [INFO] Client can accept command line arguments: -server_ip, -server_port, -map_name
echo.
pause