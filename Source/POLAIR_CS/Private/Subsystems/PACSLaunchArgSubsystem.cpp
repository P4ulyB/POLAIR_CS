#include "Subsystems/PACSLaunchArgSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UPACSLaunchArgSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] Initialize() called"));
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] About to parse command line"));
    ParseCommandLine();
    UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] Command line parsing complete"));
    
    // Auto-connect if valid arguments provided
    if (Parsed.IsServerEndpointValid() && !Parsed.PlayFabPlayerName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] Valid arguments found, setting up auto-connect"));
        // Delay connection attempt to allow world/player controller to be ready
        FTimerHandle DelayHandle;
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(DelayHandle, this, 
                &UPACSLaunchArgSubsystem::AttemptAutoConnect, 0.5f, false);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] No world available, skipping auto-connect timer"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] Invalid arguments or missing data, skipping auto-connect"));
    }
}

void UPACSLaunchArgSubsystem::ParseCommandLine()
{
    const TCHAR* Cmd = FCommandLine::Get();
    UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] ParseCommandLine - Full command line: %s"), Cmd);
    
    FParse::Value(Cmd, TEXT("ServerIP="), Parsed.ServerIP);
    UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] Parsed ServerIP: '%s'"), *Parsed.ServerIP);

    int32 Port = 0;
    if (FParse::Value(Cmd, TEXT("ServerPort="), Port))
        Parsed.ServerPort = Port;
    UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] Parsed ServerPort: %d"), Parsed.ServerPort);

    FParse::Value(Cmd, TEXT("PlayFabPlayerName="), Parsed.PlayFabPlayerName);
    UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] Parsed PlayFabPlayerName: '%s'"), *Parsed.PlayFabPlayerName);
    
    UE_LOG(LogTemp, Warning, TEXT("[PACS SUBSYSTEM] PACS LaunchArgs Final: IP=%s, Port=%d, Player=%s"), 
        *Parsed.ServerIP, Parsed.ServerPort, *Parsed.PlayFabPlayerName);
}

void UPACSLaunchArgSubsystem::AttemptAutoConnect()
{
    if (!bHasAttemptedConnect)
    {
        bHasAttemptedConnect = true;
        BeginConnectFlow();
    }
}

void UPACSLaunchArgSubsystem::BeginConnectFlow()
{
    if (!Parsed.IsServerEndpointValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS: Missing or invalid ServerIP/Port"));
        return;
    }

    if (Parsed.PlayFabPlayerName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS: Missing PlayFabPlayerName"));
        return;
    }

    TravelToServer();
}

bool UPACSLaunchArgSubsystem::TravelToServer()
{
    if (!Parsed.IsServerEndpointValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS: Cannot travel - invalid server endpoint"));
        return false;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS: Cannot travel - no PlayerController found"));
        return false;
    }

    FString EncodedName = FGenericPlatformHttp::UrlEncode(Parsed.PlayFabPlayerName);
    FString Url = FString::Printf(TEXT("%s:%d?pfu=%s"),
        *Parsed.ServerIP, Parsed.ServerPort, *EncodedName);

    UE_LOG(LogTemp, Log, TEXT("PACS: Travelling to: %s"), *Url);
    PC->ClientTravel(Url, ETravelType::TRAVEL_Absolute);
    return true;
}