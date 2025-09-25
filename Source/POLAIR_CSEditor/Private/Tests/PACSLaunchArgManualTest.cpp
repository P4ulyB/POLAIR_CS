// Manual test for PACSLaunchArgSubsystem
// This test can be run directly in the editor console: PACS.TestLaunchArgs

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Subsystems/PACSLaunchArgSubsystem.h"
#include "Core/PACSGameInstance.h"
#include "Misc/CommandLine.h"

#if WITH_EDITOR

static void TestPACSLaunchArgs()
{
    UE_LOG(LogTemp, Warning, TEXT("=== PACS Manual Launch Args Test ==="));
    
    // Save original command line
    FString OriginalCmdLine = FCommandLine::Get();
    UE_LOG(LogTemp, Warning, TEXT("Original command line: %s"), *OriginalCmdLine);
    
    // Set test command line
    FString TestCmdLine = TEXT("-ServerIP=203.0.113.42 -ServerPort=7777 -PlayFabPlayerName=TestPlayer");
    FCommandLine::Set(*TestCmdLine);
    UE_LOG(LogTemp, Warning, TEXT("Set test command line: %s"), FCommandLine::Get());
    
    // Create test GameInstance with Epic's pattern
    UE_LOG(LogTemp, Warning, TEXT("Creating test GameInstance..."));
    UPACSGameInstance* TestGameInstance = NewObject<UPACSGameInstance>(GetTransientPackage(), UPACSGameInstance::StaticClass());
    
    if (!TestGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create test GameInstance"));
        FCommandLine::Set(*OriginalCmdLine);
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("GameInstance created: %s"), *TestGameInstance->GetName());
    
    // Initialize GameInstance (Epic's pattern - required for subsystems)
    UE_LOG(LogTemp, Warning, TEXT("Calling Init() on GameInstance..."));
    TestGameInstance->Init();
    UE_LOG(LogTemp, Warning, TEXT("GameInstance Init() complete"));
    
    // Get subsystem
    UE_LOG(LogTemp, Warning, TEXT("Getting subsystem..."));
    UPACSLaunchArgSubsystem* Subsystem = TestGameInstance->GetSubsystem<UPACSLaunchArgSubsystem>();
    
    if (Subsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("SUCCESS! Subsystem found via Epic Init() pattern!"));
        UE_LOG(LogTemp, Warning, TEXT("Subsystem address: %p"), Subsystem);
        
        // Test results
        UE_LOG(LogTemp, Warning, TEXT("=== Test Results ==="));
        UE_LOG(LogTemp, Warning, TEXT("ServerIP: %s (Expected: 203.0.113.42)"), *Subsystem->Parsed.ServerIP);
        UE_LOG(LogTemp, Warning, TEXT("ServerPort: %d (Expected: 7777)"), Subsystem->Parsed.ServerPort);
        UE_LOG(LogTemp, Warning, TEXT("PlayFabPlayerName: %s (Expected: TestPlayer)"), *Subsystem->Parsed.PlayFabPlayerName);
        UE_LOG(LogTemp, Warning, TEXT("IsValid: %s (Expected: true)"), Subsystem->Parsed.IsServerEndpointValid() ? TEXT("true") : TEXT("false"));
        UE_LOG(LogTemp, Warning, TEXT("GetLauncherUsername: %s (Expected: TestPlayer)"), *Subsystem->GetLauncherUsername());
        
        // Verify all values match
        bool bAllTestsPassed = true;
        if (Subsystem->Parsed.ServerIP != TEXT("203.0.113.42"))
        {
            UE_LOG(LogTemp, Error, TEXT("FAIL: ServerIP mismatch"));
            bAllTestsPassed = false;
        }
        if (Subsystem->Parsed.ServerPort != 7777)
        {
            UE_LOG(LogTemp, Error, TEXT("FAIL: ServerPort mismatch"));
            bAllTestsPassed = false;
        }
        if (Subsystem->Parsed.PlayFabPlayerName != TEXT("TestPlayer"))
        {
            UE_LOG(LogTemp, Error, TEXT("FAIL: PlayFabPlayerName mismatch"));
            bAllTestsPassed = false;
        }
        if (!Subsystem->Parsed.IsServerEndpointValid())
        {
            UE_LOG(LogTemp, Error, TEXT("FAIL: IsServerEndpointValid returned false"));
            bAllTestsPassed = false;
        }
        
        if (bAllTestsPassed)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== ALL TESTS PASSED! ==="));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("=== SOME TESTS FAILED ==="));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FAIL: GetSubsystem returned NULL even after Init()"));
        
        // Try manual creation for comparison
        UE_LOG(LogTemp, Warning, TEXT("Attempting manual subsystem creation for comparison..."));
        UPACSLaunchArgSubsystem* ManualSubsystem = NewObject<UPACSLaunchArgSubsystem>(TestGameInstance);
        if (ManualSubsystem)
        {
            ManualSubsystem->ParseCommandLine();
            UE_LOG(LogTemp, Warning, TEXT("Manual creation worked - IP: %s, Port: %d, Name: %s"),
                *ManualSubsystem->Parsed.ServerIP, ManualSubsystem->Parsed.ServerPort, *ManualSubsystem->Parsed.PlayFabPlayerName);
            UE_LOG(LogTemp, Warning, TEXT("This proves parsing works, but GetSubsystem() still fails"));
        }
    }
    
    // Cleanup with Epic's pattern
    UE_LOG(LogTemp, Warning, TEXT("Calling Shutdown() on GameInstance..."));
    TestGameInstance->Shutdown();
    UE_LOG(LogTemp, Warning, TEXT("GameInstance Shutdown() complete"));
    
    // Restore original command line
    FCommandLine::Set(*OriginalCmdLine);
    UE_LOG(LogTemp, Warning, TEXT("Restored command line: %s"), FCommandLine::Get());
    
    UE_LOG(LogTemp, Warning, TEXT("=== Test Complete ==="));
}

// Register console command
static FAutoConsoleCommand TestLaunchArgsCommand(
    TEXT("PACS.TestLaunchArgs"),
    TEXT("Run manual test of PACS LaunchArg subsystem"),
    FConsoleCommandDelegate::CreateStatic(&TestPACSLaunchArgs)
);

#endif // WITH_EDITOR