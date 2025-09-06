@echo off
REM ================================================================================================
REM POLAIR_CS Client Deployment Package Creation Script for Azure
REM ================================================================================================

echo [INFO] Starting Client Deployment Package Creation...
echo [INFO] Project: POLAIR_CS
echo [INFO] Creating ZIP package for Azure Client Distribution
echo [INFO] Source: Target/Client
echo [INFO] Output: Target/POLAIR_CS_PlayFab_Client.zip
echo.

REM Set paths
set PROJECT_DIR=C:\Devops\Projects\POLAIR_CS
set CLIENT_BUILD_DIR=%PROJECT_DIR%\Target\Client
set ZIP_OUTPUT=%PROJECT_DIR%\Target\POLAIR_CS_PlayFab_Client.zip

REM Validate client build exists
if not exist "%CLIENT_BUILD_DIR%" (
    echo [ERROR] Client build directory not found: %CLIENT_BUILD_DIR%
    echo [INFO] Please run BuildShippingClient.bat first to create the client build.
    pause
    exit /b 1
)

REM Check for client executable (look for any .exe files in the build)
dir "%CLIENT_BUILD_DIR%\*.exe" /s /b >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] No client executable found in build directory.
    echo [INFO] Please ensure BuildShippingClient.bat completed successfully.
    pause
    exit /b 1
)

REM Remove existing zip file if it exists
if exist "%ZIP_OUTPUT%" (
    echo [INFO] Removing existing zip file...
    del "%ZIP_OUTPUT%"
)

REM Create Azure client deployment zip using PowerShell with proper structure
echo [INFO] Creating Azure client deployment zip package...
echo [INFO] Source files: %CLIENT_BUILD_DIR%
echo [INFO] Output zip: %ZIP_OUTPUT%
echo [INFO] Structure: Files at root level for client distribution
echo [INFO] Excluding: *.pdb debug files (optimization)

powershell -Command "& { Add-Type -AssemblyName System.IO.Compression.FileSystem; Add-Type -AssemblyName System.IO.Compression; $sourcePath = '%CLIENT_BUILD_DIR%'; $zipPath = '%ZIP_OUTPUT%'; if (Test-Path $zipPath) { Remove-Item $zipPath -Force }; $zipStream = [System.IO.File]::Create($zipPath); $archive = New-Object System.IO.Compression.ZipArchive($zipStream, [System.IO.Compression.ZipArchiveMode]::Create); try { $files = Get-ChildItem -Path $sourcePath -File -Recurse | Where-Object { $_.Extension -ne '.pdb' -and $_.Extension -ne '.zip' }; if ($files.Count -eq 0) { Write-Host '[ERROR] No files found to package'; exit 1 }; foreach ($file in $files) { $relativePath = $file.FullName.Substring($sourcePath.Length + 1); Write-Host ('Adding to zip root: ' + $relativePath); $entry = $archive.CreateEntry($relativePath, [System.IO.Compression.CompressionLevel]::Optimal); $entryStream = $entry.Open(); $fileStream = [System.IO.File]::OpenRead($file.FullName); $fileStream.CopyTo($entryStream); $fileStream.Close(); $entryStream.Close() }; Write-Host '[SUCCESS] Azure client deployment zip created - files at root level for distribution' } finally { $archive.Dispose(); $zipStream.Dispose() } }"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to create zip package
    pause
    exit /b %ERRORLEVEL%
)

REM Validate zip file was created
if not exist "%ZIP_OUTPUT%" (
    echo [ERROR] Zip file was not created successfully
    pause
    exit /b 1
)

REM Display zip file information
echo.
echo [SUCCESS] Client deployment package created successfully!
echo [INFO] Package Location: %ZIP_OUTPUT%
for %%A in ("%ZIP_OUTPUT%") do echo [INFO] Package Size: %%~zA bytes

REM Verify zip contents
echo [INFO] Zip package contents:
powershell -Command "& { Add-Type -AssemblyName System.IO.Compression.FileSystem; $zip = [System.IO.Compression.ZipFile]::OpenRead('%ZIP_OUTPUT%'); $zip.Entries | ForEach-Object { Write-Host ('  ' + $_.FullName) }; $zip.Dispose() }"

echo.
echo [INFO] Package is ready for Azure client distribution!
echo [IMPORTANT] This client can accept command line arguments: -server_ip, -server_port, -map_name
echo [INFO] Client will automatically connect to server after authentication when launched with arguments.
echo.
pause