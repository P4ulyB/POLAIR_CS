#include "Subsystems/PACSServerKeepaliveSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

// Only include GSDK on server builds
#if UE_SERVER
#include "PlayFabGSDK.h"
#endif

void UPACSServerKeepaliveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Only run on dedicated servers with authority
    UWorld* World = GetWorld();
    if (!World || !World->IsNetMode(NM_DedicatedServer))
    {
        return;
    }

#if UE_SERVER
    // Initialize GSDK
    if (UPlayFabGSDK* GSDK = UPlayFabGSDK::Get())
    {
        GSDK->ReadyForPlayers();
        UE_LOG(LogTemp, Log, TEXT("PACS: GSDK ReadyForPlayers called"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS: GSDK not available"));
    }
#endif

    // Start GSDK update timer (every 30 seconds)
    World->GetTimerManager().SetTimer(GSDKUpdateTimer, this, 
        &UPACSServerKeepaliveSubsystem::TickGSDKUpdate, 30.0f, true);

    // Start idle check timer (every 30 seconds)
    World->GetTimerManager().SetTimer(IdleCheckTimer, this,
        &UPACSServerKeepaliveSubsystem::CheckIdleShutdown, 30.0f, true);

    UE_LOG(LogTemp, Log, TEXT("PACS: ServerKeepaliveSubsystem initialized"));
}

void UPACSServerKeepaliveSubsystem::Deinitialize()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(GSDKUpdateTimer);
        World->GetTimerManager().ClearTimer(IdleCheckTimer);
    }
    
    Super::Deinitialize();
}

void UPACSServerKeepaliveSubsystem::RegisterPlayer(const FString& PlayerId)
{
    if (!GetWorld() || !GetWorld()->IsNetMode(NM_DedicatedServer))
    {
        return;
    }

    if (!PlayerId.IsEmpty())
    {
        ConnectedPlayers.Add(PlayerId);
        UE_LOG(LogTemp, Log, TEXT("PACS: Registered player: %s (Total: %d)"), 
            *PlayerId, ConnectedPlayers.Num());
    }
}

void UPACSServerKeepaliveSubsystem::UnregisterPlayer(const FString& PlayerId)
{
    if (!GetWorld() || !GetWorld()->IsNetMode(NM_DedicatedServer))
    {
        return;
    }

    if (ConnectedPlayers.Remove(PlayerId) > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("PACS: Unregistered player: %s (Total: %d)"), 
            *PlayerId, ConnectedPlayers.Num());
        
        // Mark disconnect time for idle shutdown tracking
        if (ConnectedPlayers.Num() == 0)
        {
            LastPlayerDisconnectTime = GetWorld()->GetTimeSeconds();
            UE_LOG(LogTemp, Log, TEXT("PACS: Server now empty - starting idle timer"));
        }
    }
}

void UPACSServerKeepaliveSubsystem::TickGSDKUpdate()
{
    if (!GetWorld() || !GetWorld()->IsNetMode(NM_DedicatedServer))
    {
        return;
    }

#if UE_SERVER
    if (UPlayFabGSDK* GSDK = UPlayFabGSDK::Get())
    {
        GSDK->UpdateConnectedPlayers(ConnectedPlayers.Array());
    }
#endif
}

void UPACSServerKeepaliveSubsystem::CheckIdleShutdown()
{
    if (!GetWorld() || !GetWorld()->IsNetMode(NM_DedicatedServer))
    {
        return;
    }

    // Only check shutdown if server is empty
    if (ConnectedPlayers.Num() == 0 && LastPlayerDisconnectTime > 0.0f)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        float IdleTime = CurrentTime - LastPlayerDisconnectTime;
        
        if (IdleTime >= IDLE_SHUTDOWN_DELAY)
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS: Server idle for %.1f seconds - shutting down"), IdleTime);
            ShutdownServer();
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("PACS: Server idle for %.1f/%.1f seconds"), IdleTime, IDLE_SHUTDOWN_DELAY);
        }
    }
}

void UPACSServerKeepaliveSubsystem::ShutdownServer()
{
#if UE_SERVER
    if (UPlayFabGSDK* GSDK = UPlayFabGSDK::Get())
    {
        GSDK->Shutdown();
    }
    else
    {
        // Fallback shutdown if GSDK unavailable
        UE_LOG(LogTemp, Error, TEXT("PACS: GSDK unavailable - requesting engine shutdown"));
        FPlatformMisc::RequestExit(false);
    }
#endif
}