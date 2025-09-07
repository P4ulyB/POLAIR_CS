#include "Tests/PACS_HMDSpawning_TestHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "GameFramework/GameModeBase.h"

// Mock Candidate Pawn Implementation
AMockCandidatePawn::AMockCandidatePawn()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bIsCandidatePawn = true;
    
    // Set up basic properties for VR pawn
    SetCanBeDamaged(false);
}

// Mock Assessor Pawn Implementation  
AMockAssessorPawn::AMockAssessorPawn()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bIsAssessorPawn = true;
    
    // Set up basic properties for spectator pawn
    SetCanBeDamaged(false);
}

// Test GameMode Implementation
APACSTestGameMode::APACSTestGameMode()
{
    // Configure the test game mode with our mock classes
    CandidatePawnClass = AMockCandidatePawn::StaticClass();
    AssessorPawnClass = AMockAssessorPawn::StaticClass();
    PlayerControllerClass = AMockPACS_PlayerController::StaticClass();
    PlayerStateClass = APACS_PlayerState::StaticClass();
    
    // Initialize test statistics
    TotalPlayersSpawned = 0;
    CandidatesSpawned = 0;
    AssessorsSpawned = 0;
}

void APACSTestGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // Call parent implementation
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    
    // Track spawn statistics for testing
    if (NewPlayer && NewPlayer->GetPawn())
    {
        TotalPlayersSpawned++;
        
        if (NewPlayer->GetPawn()->IsA<AMockCandidatePawn>())
        {
            CandidatesSpawned++;
        }
        else if (NewPlayer->GetPawn()->IsA<AMockAssessorPawn>())
        {
            AssessorsSpawned++;
        }
    }
}

// Mock PlayerController Implementation
AMockPACS_PlayerController::AMockPACS_PlayerController()
{
    SimulatedHMDState = EHMDState::Unknown;
    bShouldSimulateTimeout = false;
    SimulatedResponseDelay = 0.0f;
    HMDRequestCount = 0;
}

void AMockPACS_PlayerController::ClientRequestHMDState_Implementation()
{
    HMDRequestCount++;
    
    if (bShouldSimulateTimeout)
    {
        // Don't respond to simulate timeout scenario
        UE_LOG(LogTemp, Log, TEXT("MockPACS_PlayerController: Simulating timeout - not responding to HMD request"));
        return;
    }
    
    if (SimulatedResponseDelay > 0.0f)
    {
        // Simulate delayed response
        GetWorld()->GetTimerManager().SetTimer(
            SimulatedResponseHandle,
            this,
            &AMockPACS_PlayerController::SimulateDelayedHMDResponse,
            SimulatedResponseDelay,
            false
        );
    }
    else
    {
        // Immediate response with simulated HMD state
        UE_LOG(LogTemp, Log, TEXT("MockPACS_PlayerController: Reporting simulated HMD state: %d"), 
            static_cast<int32>(SimulatedHMDState));
        ServerReportHMDState(SimulatedHMDState);
    }
}

void AMockPACS_PlayerController::SimulateDelayedHMDResponse()
{
    UE_LOG(LogTemp, Log, TEXT("MockPACS_PlayerController: Delayed response - reporting HMD state: %d"), 
        static_cast<int32>(SimulatedHMDState));
    ServerReportHMDState(SimulatedHMDState);
}

#if WITH_DEV_AUTOMATION_TESTS

// Test Utility Functions Implementation
namespace PACS_HMDSpawningTestUtils
{
    UWorld* CreateHMDTestWorld()
    {
        UWorld* TestWorld = FAutomationEditorCommonUtils::CreateNewMap();
        if (TestWorld)
        {
            // Set up the world with our test game mode
            TestWorld->GetWorldSettings()->DefaultGameMode = APACSTestGameMode::StaticClass();
            
            // Ensure the game mode is instantiated
            if (!TestWorld->GetAuthGameMode<APACSTestGameMode>())
            {
                APACSTestGameMode* GameMode = TestWorld->SpawnActor<APACSTestGameMode>();
                // Note: In a real scenario, the GameMode would be set up by the engine
                // This is a simplified approach for testing
            }
        }
        return TestWorld;
    }

    AMockPACS_PlayerController* CreateMockPlayerController(
        UWorld* World, 
        EHMDState HMDState, 
        bool bSimulateTimeout)
    {
        if (!World)
        {
            return nullptr;
        }

        AMockPACS_PlayerController* PC = World->SpawnActor<AMockPACS_PlayerController>();
        if (PC)
        {
            PC->SimulatedHMDState = HMDState;
            PC->bShouldSimulateTimeout = bSimulateTimeout;
            
            // Create and assign PlayerState
            APACS_PlayerState* PS = World->SpawnActor<APACS_PlayerState>();
            if (PS)
            {
                PC->PlayerState = PS;
            }
        }
        
        return PC;
    }

    bool SimulatePlayerLogin(APACSTestGameMode* GameMode, AMockPACS_PlayerController* PlayerController)
    {
        if (!GameMode || !PlayerController)
        {
            return false;
        }

        try
        {
            // Simulate the complete login flow
            GameMode->PostLogin(PlayerController);
            
            // Allow time for HMD detection RPC
            if (PlayerController->GetWorld())
            {
                PlayerController->GetWorld()->Tick(LEVELTICK_All, 0.016f);
            }
            
            // Trigger spawn
            GameMode->HandleStartingNewPlayer(PlayerController);
            
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void WaitForSpawnCompletion(UWorld* World, float MaxWaitTime)
    {
        if (!World)
        {
            return;
        }

        float ElapsedTime = 0.0f;
        const float TickInterval = 0.016f; // ~60 FPS
        
        while (ElapsedTime < MaxWaitTime)
        {
            World->Tick(LEVELTICK_All, TickInterval);
            ElapsedTime += TickInterval;
            
            // Could add additional completion checks here if needed
        }
    }

    bool ValidatePawnType(APlayerController* PC, EHMDState ExpectedHMDState)
    {
        if (!PC || !PC->GetPawn())
        {
            return false;
        }

        switch (ExpectedHMDState)
        {
            case EHMDState::HasHMD:
                return PC->GetPawn()->IsA<AMockCandidatePawn>();
                
            case EHMDState::NoHMD:
            case EHMDState::Unknown: // Unknown typically defaults to Assessor
                return PC->GetPawn()->IsA<AMockAssessorPawn>();
                
            default:
                return false;
        }
    }

    TArray<AMockPACS_PlayerController*> CreateMultipleTestClients(
        UWorld* World, 
        const TArray<EHMDState>& HMDStates)
    {
        TArray<AMockPACS_PlayerController*> Controllers;
        
        if (!World)
        {
            return Controllers;
        }

        for (EHMDState HMDState : HMDStates)
        {
            AMockPACS_PlayerController* PC = CreateMockPlayerController(World, HMDState, false);
            if (PC)
            {
                Controllers.Add(PC);
            }
        }
        
        return Controllers;
    }

    FHMDSpawnTestResult TestMultiClientSpawning(
        UWorld* World,
        const TArray<EHMDState>& ClientHMDStates)
    {
        FHMDSpawnTestResult Result;
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Unknown error");

        if (!World)
        {
            Result.ErrorMessage = TEXT("Invalid world");
            return Result;
        }

        APACSTestGameMode* GameMode = World->GetAuthGameMode<APACSTestGameMode>();
        if (!GameMode)
        {
            GameMode = World->SpawnActor<APACSTestGameMode>();
            if (!GameMode)
            {
                Result.ErrorMessage = TEXT("Failed to create game mode");
                return Result;
            }
        }

        // Calculate expected results
        for (EHMDState State : ClientHMDStates)
        {
            if (State == EHMDState::HasHMD)
            {
                Result.ExpectedCandidates++;
            }
            else
            {
                Result.ExpectedAssessors++;
            }
        }

        // Create and process all clients
        TArray<AMockPACS_PlayerController*> Controllers = CreateMultipleTestClients(World, ClientHMDStates);
        
        if (Controllers.Num() != ClientHMDStates.Num())
        {
            Result.ErrorMessage = FString::Printf(TEXT("Failed to create all controllers. Expected: %d, Created: %d"), 
                ClientHMDStates.Num(), Controllers.Num());
            return Result;
        }

        // Process each client through the login flow
        for (AMockPACS_PlayerController* PC : Controllers)
        {
            if (!SimulatePlayerLogin(GameMode, PC))
            {
                Result.ErrorMessage = TEXT("Failed to simulate player login");
                return Result;
            }
        }

        // Wait for all spawns to complete
        WaitForSpawnCompletion(World, 2.0f);

        // Collect actual results
        Result.ActualCandidates = GameMode->CandidatesSpawned;
        Result.ActualAssessors = GameMode->AssessorsSpawned;

        // Validate results
        if (Result.ExpectedCandidates == Result.ActualCandidates && 
            Result.ExpectedAssessors == Result.ActualAssessors)
        {
            Result.bSuccess = true;
            Result.ErrorMessage = TEXT("Success");
        }
        else
        {
            Result.ErrorMessage = FString::Printf(
                TEXT("Spawn count mismatch. Expected Candidates: %d, Actual: %d, Expected Assessors: %d, Actual: %d"),
                Result.ExpectedCandidates, Result.ActualCandidates,
                Result.ExpectedAssessors, Result.ActualAssessors);
        }

        return Result;
    }
}

#endif // WITH_DEV_AUTOMATION_TESTS