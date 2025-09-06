@echo off
setlocal enabledelayedexpansion

echo Calculating SHA256 for .zip files...
echo.

:: Check if any .zip files exist
if not exist "*.zip" (
    echo No .zip files found in current directory.
    pause
    exit /b
)

:: Process each .zip file
for %%F in (*.zip) do (
    echo File: %%F
    powershell -command "Get-FileHash -Algorithm SHA256 -Path '%%F' | Select-Object -ExpandProperty Hash"
    echo.
)

pause