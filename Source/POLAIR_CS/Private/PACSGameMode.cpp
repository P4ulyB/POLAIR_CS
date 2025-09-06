#include "PACSGameMode.h"
#include "PACSServerKeepaliveSubsystem.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetConnection.h"
#include "Misc/Parse.h"

void APACSGameMode::PreLogin(const FString& Options, const FString& Address, 
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    // Authority check as per policies
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS: PreLogin called without authority"));
        ErrorMessage = TEXT("Server authority error");
        return;
    }

    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

    // Extract and validate player name from travel URL
    FString PlayerName = UGameplayStatics::ParseOption(Options, TEXT("pfu"));
    if (PlayerName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS: No pfu parameter found in login options: %s"), *Options);
        // Don't fail login, just log warning - player name will be generated later
    }
    else
    {
        PlayerName = FGenericPlatformHttp::UrlDecode(PlayerName).Left(64).TrimStartAndEnd();
        UE_LOG(LogTemp, Log, TEXT("PACS: PreLogin player name: %s"), *PlayerName);
    }
}

void APACSGameMode::PostLogin(APlayerController* NewPlayer)
{
    // Authority check as per policies
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("PACS: PostLogin called without authority"));
        return;
    }

    Super::PostLogin(NewPlayer);

    if (!NewPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("PACS: PostLogin called with null PlayerController"));
        return;
    }

    // Extract and set player name from connection URL (Epic's pattern)
    FString DecodedName;
    if (NewPlayer->GetNetConnection())
    {
        const FURL& ConnectionURL = NewPlayer->GetNetConnection()->URL;
        DecodedName = ConnectionURL.GetOption(TEXT("pfu="), TEXT(""));
    }
    
    if (!DecodedName.IsEmpty())
    {
        DecodedName = FGenericPlatformHttp::UrlDecode(DecodedName).Left(64).TrimStartAndEnd();
        if (APlayerState* PS = NewPlayer->PlayerState)
        {
            PS->SetPlayerName(DecodedName);
            UE_LOG(LogTemp, Log, TEXT("PACS: Set player name to: %s"), *DecodedName);
        }
    }

    // Register with keepalive system
    if (UPACSServerKeepaliveSubsystem* KeepaliveSystem = 
        GetGameInstance()->GetSubsystem<UPACSServerKeepaliveSubsystem>())
    {
        FString PlayerId = DecodedName.IsEmpty() ? 
            FString::Printf(TEXT("Player_%d"), FMath::RandRange(1000, 9999)) : DecodedName;
        KeepaliveSystem->RegisterPlayer(PlayerId);
    }
}

void APACSGameMode::Logout(AController* Exiting)
{
    if (!HasAuthority())
    {
        return;
    }

    // Unregister from keepalive system before logout
    if (UPACSServerKeepaliveSubsystem* KeepaliveSystem = 
        GetGameInstance()->GetSubsystem<UPACSServerKeepaliveSubsystem>())
    {
        if (APlayerController* PC = Cast<APlayerController>(Exiting))
        {
            FString PlayerId = PC->PlayerState ? PC->PlayerState->GetPlayerName() : TEXT("Unknown");
            KeepaliveSystem->UnregisterPlayer(PlayerId);
        }
    }

    Super::Logout(Exiting);
}

