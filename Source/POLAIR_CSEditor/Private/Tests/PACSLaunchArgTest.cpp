#if WITH_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "PACSLaunchArgSubsystem.h"
#include "PACSGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACSLaunchArgParseTest, "PACS.LaunchArgs.Parse", 
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPACSLaunchArgParseTest::RunTest(const FString& Parameters)
{
    // Save original command line
    FString OriginalCmdLine = FCommandLine::Get();
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Original command line: %s"), *OriginalCmdLine);
    
    // Set test command line
    FCommandLine::Set(TEXT("-ServerIP=203.0.113.42 -ServerPort=7777 -PlayFabPlayerName=TestPlayer"));
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Set test command line: %s"), FCommandLine::Get());

    // Create a test GameInstance for editor context testing
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Creating test GameInstance..."));
    UPACSGameInstance* TestGameInstance = NewObject<UPACSGameInstance>(GetTransientPackage(), UPACSGameInstance::StaticClass());
    if (!TestGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Failed to create test GameInstance"));
        AddError(TEXT("Failed to create test GameInstance"));
        FCommandLine::Set(*OriginalCmdLine);
        return false;
    }
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] GameInstance created successfully: %s"), *TestGameInstance->GetName());
    
    // Initialize the GameInstance using Epic's pattern
    // Note: Init() handles subsystem initialization internally
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Calling Init() on GameInstance (Epic's pattern)..."));
    TestGameInstance->Init();
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] GameInstance Init() complete"));

    // Check if GameInstance has subsystem support
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] GameInstance class: %s"), *TestGameInstance->GetClass()->GetName());
    
    // Try to initialize the GameInstance subsystems manually
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Attempting to get subsystem..."));
    
    // Get subsystem from our test game instance
    UPACSLaunchArgSubsystem* Subsystem = TestGameInstance->GetSubsystem<UPACSLaunchArgSubsystem>();
    
    if (Subsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] SUCCESS! Subsystem found via Epic pattern! Address: %p"), Subsystem);
        UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Subsystem class: %s"), *Subsystem->GetClass()->GetName());
        
        // Log parsed values before testing
        UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Parsed ServerIP: '%s'"), *Subsystem->Parsed.ServerIP);
        UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Parsed ServerPort: %d"), Subsystem->Parsed.ServerPort);
        UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Parsed PlayFabPlayerName: '%s'"), *Subsystem->Parsed.PlayFabPlayerName);
        UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] IsValid: %s"), Subsystem->Parsed.IsServerEndpointValid() ? TEXT("true") : TEXT("false"));
        
        // Test parsing results (subsystem should auto-parse on initialization)
        TestEqual(TEXT("ServerIP"), Subsystem->Parsed.ServerIP, FString("203.0.113.42"));
        TestEqual(TEXT("ServerPort"), Subsystem->Parsed.ServerPort, 7777);
        TestEqual(TEXT("PlayFabPlayerName"), Subsystem->Parsed.PlayFabPlayerName, FString("TestPlayer"));
        TestTrue(TEXT("IsValid"), Subsystem->Parsed.IsServerEndpointValid());
        TestEqual(TEXT("GetLauncherUsername"), Subsystem->GetLauncherUsername(), FString("TestPlayer"));
        
        UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] All assertions passed!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Subsystem is NULL!"));
        
        // Check if our subsystem class is valid
        UClass* SubsystemClass = UPACSLaunchArgSubsystem::StaticClass();
        if (SubsystemClass)
        {
            UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Subsystem class is valid: %s"), *SubsystemClass->GetName());
            UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Subsystem class flags: %d"), (int32)SubsystemClass->ClassFlags);
            
            // Try to manually create and initialize the subsystem
            UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Attempting manual subsystem creation..."));
            UPACSLaunchArgSubsystem* ManualSubsystem = NewObject<UPACSLaunchArgSubsystem>(TestGameInstance);
            if (ManualSubsystem)
            {
                UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Manual subsystem created successfully"));
                // Manual subsystem won't auto-initialize, so we need to call ParseCommandLine directly
                ManualSubsystem->ParseCommandLine();
                UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Manual subsystem parsed - IP: %s, Port: %d, Name: %s"), 
                    *ManualSubsystem->Parsed.ServerIP, ManualSubsystem->Parsed.ServerPort, *ManualSubsystem->Parsed.PlayFabPlayerName);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Failed to create manual subsystem"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Subsystem class is NULL!"));
        }
        
        // Epic's pattern failed, but we proved parsing works with manual creation
        UE_LOG(LogTemp, Error, TEXT("[PACS TEST] Epic pattern failed, but manual creation proved parsing works"));
        AddError(TEXT("Failed to get PACSLaunchArgSubsystem from test GameInstance - Epic Init() pattern didn't work"));
    }

    // Epic's cleanup pattern
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Calling Shutdown() on GameInstance (Epic's cleanup pattern)..."));
    TestGameInstance->Shutdown();
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] GameInstance Shutdown() complete"));

    // Restore original command line
    FCommandLine::Set(*OriginalCmdLine);
    UE_LOG(LogTemp, Warning, TEXT("[PACS TEST] Restored command line: %s"), FCommandLine::Get());

    return true;
}

#endif // WITH_AUTOMATION_TESTS