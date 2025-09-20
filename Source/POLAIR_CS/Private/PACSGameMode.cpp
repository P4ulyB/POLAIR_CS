#include "PACSGameMode.h"
#include "PACS_PlayerController.h"
#include "PACS_PlayerState.h"
#include "PACSServerKeepaliveSubsystem.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetConnection.h"
#include "Misc/Parse.h"
#include "PACS/Heli/PACS_CandidateHelicopterCharacter.h"
#include "Pawns/Assessor/PACS_AssessorPawn.h"
#include "TimerManager.h"
//#include "PACS/Heli/PACS_OrbitMessages.h" // only if saved offsets are passed

APACSGameMode::APACSGameMode()
{
    // Set default PlayerState class
    PlayerStateClass = APACS_PlayerState::StaticClass();

    // Set pawn classes - configure in Blueprint or here
    // CandidatePawnClass = APACS_CandidatePawn::StaticClass();
    AssessorPawnClass = APACS_AssessorPawn::StaticClass();
}

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

    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: PostLogin called for player"));
    
    // Zero-swap handshake: request HMD state immediately
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(NewPlayer))
    {
        // GameMode only exists on server, safe to call Client RPC
        UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Requesting HMD state from client"));
        PACSPC->ClientRequestHMDState();
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

UClass* APACSGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    // GameMode only exists on server - no HasAuthority() check needed
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(InController))
    {
        // Guard PlayerState access with null check
        if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PACSPC->PlayerState))
        {
            // Return appropriate pawn class based on HMD detection
            switch (PACSPS->HMDState)
            {
                case EHMDState::HasHMD:
                    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Selecting CandidatePawn for HMD user"));
                    return CandidatePawnClass.Get();
                case EHMDState::NoHMD:
                    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Selecting AssessorPawn for non-HMD user"));
                    return AssessorPawnClass.Get();
                case EHMDState::Unknown:
                default:
                    UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: HMD state unknown - falling back to AssessorPawn"));
                    break;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: PlayerState null - falling back to AssessorPawn"));
        }
    }
    
    // Fail-safe: Default to AssessorPawn if PlayerState unavailable or HMD state unknown
    return AssessorPawnClass ? AssessorPawnClass.Get() : Super::GetDefaultPawnClassForController_Implementation(InController);
}

void APACSGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // Idempotence guard: prevent double spawn
    if (NewPlayer && NewPlayer->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: Player already has pawn - clearing timer and returning"));
        if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(NewPlayer))
        {
            GetWorld()->GetTimerManager().ClearTimer(PACSPC->HMDWaitHandle);
        }
        return;
    }

    // Zero-swap pattern: Only spawn once HMD state is known
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(NewPlayer))
    {
        // Guard PlayerState access with null check
        if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PACSPC->PlayerState))
        {
            if (PACSPS->HMDState != EHMDState::Unknown)
            {
                // HMD state known, clear any pending timer and proceed with spawn
                UE_LOG(LogTemp, Log, TEXT("PACS GameMode: HMD state known (%d) - proceeding with spawn"), static_cast<int32>(PACSPS->HMDState));
                GetWorld()->GetTimerManager().ClearTimer(PACSPC->HMDWaitHandle);
                Super::HandleStartingNewPlayer_Implementation(NewPlayer);

                // Seed orbit immediately after spawn (server authority)
                if (APACS_CandidateHelicopterCharacter* Candidate =
                    Cast<APACS_CandidateHelicopterCharacter>(NewPlayer->GetPawn()))
                {
                    // If you have save offsets, pass a pointer; else pass nullptr.
                    // const FPACS_OrbitOffsets* Offsets = SaveSubsystem ? &SaveSubsystem->GetOffsets() : nullptr;
                    Candidate->ApplyOffsetsThenSeed(/*Offsets*/ nullptr);
                }
                return;
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("PACS GameMode: HMD state unknown - setting timeout"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: PlayerState null - setting timeout"));
        }
        
        // Set per-player timeout to prevent infinite wait
        GetWorld()->GetTimerManager().SetTimer(
            PACSPC->HMDWaitHandle,
            FTimerDelegate::CreateLambda([this, WeakPC = TWeakObjectPtr<APACS_PlayerController>(PACSPC)]()
                {
                    if (WeakPC.IsValid()) { OnHMDTimeout(WeakPC.Get()); }
                }),
            3.0f, false);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Non-PACS PlayerController - using default spawn"));
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void APACSGameMode::OnHMDTimeout(APlayerController* PlayerController)
{
    UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: HMD detection timeout reached"));
    
    // Clear timer first to prevent race conditions
    if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(PlayerController))
    {
        GetWorld()->GetTimerManager().ClearTimer(PACSPC->HMDWaitHandle);
    }
    
    // Idempotence guard: prevent double spawn
    if (PlayerController && PlayerController->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("PACS GameMode: Player already has pawn during timeout - returning"));
        return;
    }

    // Timeout reached - force spawn with NoHMD default
    if (IsValid(PlayerController))
    {
        if (APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(PlayerController))
        {
            // Guard PlayerState access with null check
            if (APACS_PlayerState* PACSPS = Cast<APACS_PlayerState>(PACSPC->PlayerState))
            {
                if (PACSPS->HMDState == EHMDState::Unknown)
                {
                    UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Setting HMD state to NoHMD due to timeout"));
                    PACSPS->HMDState = EHMDState::NoHMD;
                }
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("PACS GameMode: Forcing spawn after timeout"));
        Super::HandleStartingNewPlayer_Implementation(PlayerController);

        // Seed after forced spawn
        if (APACS_CandidateHelicopterCharacter* Candidate =
            Cast<APACS_CandidateHelicopterCharacter>(PlayerController ? PlayerController->GetPawn() : nullptr))
        {
            Candidate->ApplyOffsetsThenSeed(/*Offsets*/ nullptr);
        }
    }
}

