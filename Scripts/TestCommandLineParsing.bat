@echo off
REM ================================================================================================
REM PACS Command Line Parsing Test Script
REM Tests various command line argument scenarios for the authentication system
REM ================================================================================================

echo [TEST] PACS Command Line Parsing Test Suite
echo [TEST] Testing various command line argument scenarios
echo.

REM Set project paths
set PROJECT_DIR=C:\Devops\Projects\POLAIR_CS
set PROJECT_FILE=%PROJECT_DIR%\POLAIR_CS.uproject
set UE_SOURCE_DIR=C:\Devops\UESource
set EDITOR_PATH=%UE_SOURCE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe

REM Test scenario 1: Valid tokens with quotes
echo [TEST CASE 1] Valid tokens with quotes
echo Command: -playfab_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ0ZXN0In0.test" -playfab_entity_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJlbnRpdHkiOiJ0ZXN0In0.test" -session_id="test_session_123"
echo Expected: All tokens parsed successfully
echo.

REM Test scenario 2: Valid tokens without quotes
echo [TEST CASE 2] Valid tokens without quotes
echo Command: -playfab_token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ0ZXN0In0.test -playfab_entity_token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJlbnRpdHkiOiJ0ZXN0In0.test -session_id=test_session_123
echo Expected: All tokens parsed successfully
echo.

REM Test scenario 3: Missing PlayFab token
echo [TEST CASE 3] Missing PlayFab token
echo Command: -playfab_entity_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJlbnRpdHkiOiJ0ZXN0In0.test" -session_id="test_session_123"
echo Expected: Warning - Missing PlayFab token
echo.

REM Test scenario 4: Missing entity token
echo [TEST CASE 4] Missing entity token
echo Command: -playfab_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ0ZXN0In0.test" -session_id="test_session_123"
echo Expected: Warning - Missing entity token
echo.

REM Test scenario 5: Missing session ID
echo [TEST CASE 5] Missing session ID
echo Command: -playfab_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ0ZXN0In0.test" -playfab_entity_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJlbnRpdHkiOiJ0ZXN0In0.test"
echo Expected: Warning - Missing session ID
echo.

REM Test scenario 6: Invalid Base64 token
echo [TEST CASE 6] Invalid Base64 token format
echo Command: -playfab_token="not_base64_token!" -playfab_entity_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJlbnRpdHkiOiJ0ZXN0In0.test" -session_id="test_session_123"
echo Expected: Warning - Invalid token format
echo.

REM Test scenario 7: Short token (less than 10 chars)
echo [TEST CASE 7] Token too short
echo Command: -playfab_token="short" -playfab_entity_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJlbnRpdHkiOiJ0ZXN0In0.test" -session_id="test_session_123"
echo Expected: Warning - Token too short
echo.

REM Test scenario 8: Invalid session ID format
echo [TEST CASE 8] Invalid session ID characters
echo Command: -playfab_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ0ZXN0In0.test" -playfab_entity_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJlbnRpdHkiOiJ0ZXN0In0.test" -session_id="session@#$%"
echo Expected: Warning - Invalid session ID characters
echo.

REM Test scenario 9: Empty tokens
echo [TEST CASE 9] Empty token values
echo Command: -playfab_token="" -playfab_entity_token="" -session_id=""
echo Expected: Warning - All tokens empty
echo.

REM Test scenario 10: No command line arguments
echo [TEST CASE 10] No command line arguments provided
echo Command: (no arguments)
echo Expected: Warning - All tokens missing
echo.

echo [TEST] Test scenarios defined
echo [TEST] To run these tests, launch the editor with each command line scenario
echo [TEST] Monitor the LogPACSAuth output for validation results
echo.

REM Create a simple launcher for testing
echo [INFO] Creating test launcher script...
echo @echo off > TestLauncher.bat
echo REM Test launcher for PACS authentication >> TestLauncher.bat
echo set TEST_CASE=%%1 >> TestLauncher.bat
echo set CMD_ARGS=%%2 %%3 %%4 %%5 %%6 %%7 %%8 %%9 >> TestLauncher.bat
echo echo Running Test Case: %%TEST_CASE%% >> TestLauncher.bat
echo echo Command Args: %%CMD_ARGS%% >> TestLauncher.bat
echo "%EDITOR_PATH%" "%PROJECT_FILE%" -log -stdout -FullStdOutLogOutput %%CMD_ARGS%% >> TestLauncher.bat

echo [TEST] Test launcher created: TestLauncher.bat
echo [TEST] Usage: TestLauncher.bat [test_case_number] [command_line_args]
echo.
pause