@echo off
REM ================================================================================================
REM POLAIR_CS PlayFab Deployment Package Creation Script
REM ================================================================================================

echo [INFO] Starting PlayFab Deployment Package Creation...
echo [INFO] Project: POLAIR_CS
echo [INFO] Creating ZIP package for PlayFab Multiplayer Services
echo [INFO] Source: Target/Server
echo [INFO] Output: Target/POLAIR_CS_PlayFab_Server.zip
echo.

REM Set paths
set PROJECT_DIR=C:\Devops\Projects\POLAIR_CS
set SERVER_BUILD_DIR=%PROJECT_DIR%\Target\Server
set ZIP_OUTPUT=%PROJECT_DIR%\Target\POLAIR_CS_PlayFab_Server.zip

REM Validate server build exists
if not exist "%SERVER_BUILD_DIR%" (
    echo [ERROR] Server build directory not found: %SERVER_BUILD_DIR%
    echo [INFO] Please run BuildShippingServer.bat first to create the server build.
    pause
    exit /b 1
)

REM Check for server executable (look for any .exe files in the build)
dir "%SERVER_BUILD_DIR%\*.exe" /s /b >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] No server executable found in build directory.
    echo [INFO] Please ensure BuildShippingServer.bat completed successfully.
    pause
    exit /b 1
)

REM Remove existing zip file if it exists
if exist "%ZIP_OUTPUT%" (
    echo [INFO] Removing existing zip file...
    del "%ZIP_OUTPUT%"
)

REM Create PlayFab deployment zip using PowerShell with proper structure
echo [INFO] Creating PlayFab deployment zip package...
echo [INFO] Source files: %SERVER_BUILD_DIR%
echo [INFO] Output zip: %ZIP_OUTPUT%
echo [INFO] Structure: Files at root level (PlayFab requirement)
echo [INFO] Excluding: *.pdb debug files (optimization)

powershell -Command "& { Add-Type -AssemblyName System.IO.Compression.FileSystem; Add-Type -AssemblyName System.IO.Compression; $sourcePath = '%SERVER_BUILD_DIR%'; $zipPath = '%ZIP_OUTPUT%'; if (Test-Path $zipPath) { Remove-Item $zipPath -Force }; $zipStream = [System.IO.File]::Create($zipPath); $archive = New-Object System.IO.Compression.ZipArchive($zipStream, [System.IO.Compression.ZipArchiveMode]::Create); try { $files = Get-ChildItem -Path $sourcePath -File -Recurse | Where-Object { $_.Extension -ne '.pdb' -and $_.Extension -ne '.zip' }; if ($files.Count -eq 0) { Write-Host '[ERROR] No files found to package'; exit 1 }; foreach ($file in $files) { $relativePath = $file.FullName.Substring($sourcePath.Length + 1); Write-Host ('Adding to zip root: ' + $relativePath); $entry = $archive.CreateEntry($relativePath, [System.IO.Compression.CompressionLevel]::Optimal); $entryStream = $entry.Open(); $fileStream = [System.IO.File]::OpenRead($file.FullName); $fileStream.CopyTo($entryStream); $fileStream.Close(); $entryStream.Close() }; Write-Host '[SUCCESS] PlayFab deployment zip created - files at root level for C:\Assets extraction' } finally { $archive.Dispose(); $zipStream.Dispose() } }"

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
echo [SUCCESS] PlayFab deployment package created successfully!
echo [INFO] Package Location: %ZIP_OUTPUT%
for %%A in ("%ZIP_OUTPUT%") do echo [INFO] Package Size: %%~zA bytes

REM Verify zip contents
echo [INFO] Zip package contents:
powershell -Command "& { Add-Type -AssemblyName System.IO.Compression.FileSystem; $zip = [System.IO.Compression.ZipFile]::OpenRead('%ZIP_OUTPUT%'); $zip.Entries | ForEach-Object { Write-Host ('  ' + $_.FullName) }; $zip.Dispose() }"

echo.
echo [INFO] Package is ready for upload to PlayFab Multiplayer Services!
echo [IMPORTANT] Upload this file to PlayFab Game Manager as a server build asset.
echo.
pause