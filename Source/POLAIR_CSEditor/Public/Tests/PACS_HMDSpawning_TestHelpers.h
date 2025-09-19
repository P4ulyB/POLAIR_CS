#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PACSGameMode.h"
#include "PACS_PlayerController.h"
#include "Players/PACS_PlayerState.h"
#include "PACS_HMDSpawning_TestHelpers.generated.h"

/**
 * Mock Candidate Pawn - represents VR users in the simulation
 * These represent the placeholder spectator pawns mentioned in requirements
 */
UCLASS(BlueprintType, NotBlueprintable)
class POLAIR_CSEDITOR_API AMockCandidatePawn : public APawn
{
    GENERATED_BODY()

public:
    AMockCandidatePawn();

    // Mark this pawn for easy identification in tests
    UPROPERTY(VisibleAnywhere, Category = "Testing")
    bool bIsCandidatePawn = true;
};

/**
 * Mock Assessor Pawn - represents non-VR users (spectators/desktop users)
 */
UCLASS(BlueprintType, NotBlueprintable)
class POLAIR_CSEDITOR_API AMockAssessorPawn : public APawn
{
    GENERATED_BODY()

public:
    AMockAssessorPawn();

    // Mark this pawn for easy identification in tests
    UPROPERTY(VisibleAnywhere, Category = "Testing")
    bool bIsAssessorPawn = true;
};

/**
 * Test GameMode with controlled pawn classes for automation testing
 */
UCLASS(BlueprintType, NotBlueprintable)
class POLAIR_CSEDITOR_API APACSTestGameMode : public APACSGameMode
{
    GENERATED_BODY()

public:
    APACSTestGameMode();

    // Test statistics for verification
    UPROPERTY(VisibleAnywhere, Category = "Testing")
    int32 TotalPlayersSpawned = 0;

    UPROPERTY(VisibleAnywhere, Category = "Testing") 
    int32 CandidatesSpawned = 0;

    UPROPERTY(VisibleAnywhere, Category = "Testing")
    int32 AssessorsSpawned = 0;

    // Override to track spawn statistics
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};

/**
 * Mock PlayerController that simulates different HMD states for testing
 */
UCLASS(BlueprintType, NotBlueprintable)
class POLAIR_CSEDITOR_API AMockPACS_PlayerController : public APACS_PlayerController
{
    GENERATED_BODY()

public:
    AMockPACS_PlayerController();

    // Simulated HMD state for testing (bypasses actual hardware detection)
    UPROPERTY(EditAnywhere, Category = "Testing")
    EHMDState SimulatedHMDState = EHMDState::Unknown;

    // Controls timeout simulation for testing timeout scenarios
    UPROPERTY(EditAnywhere, Category = "Testing")
    bool bShouldSimulateTimeout = false;

    // Controls RPC response delay for testing async scenarios
    UPROPERTY(EditAnywhere, Category = "Testing")
    float SimulatedResponseDelay = 0.0f;

    // Test tracking
    UPROPERTY(VisibleAnywhere, Category = "Testing")
    int32 HMDRequestCount = 0;

    // Override to use simulated HMD state instead of actual hardware
    virtual void ClientRequestHMDState_Implementation();

protected:
    // Timer handle for simulating delayed responses
    FTimerHandle SimulatedResponseHandle;

    // Internal method for delayed HMD response simulation
    void SimulateDelayedHMDResponse();
};

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test utility functions for HMD spawning tests
 */
namespace PACS_HMDSpawningTestUtils
{
    /**
     * Creates a test world with the proper game mode setup
     */
    POLAIR_CSEDITOR_API UWorld* CreateHMDTestWorld();

    /**
     * Creates a mock player controller with specified HMD state
     */
    POLAIR_CSEDITOR_API AMockPACS_PlayerController* CreateMockPlayerController(
        UWorld* World, 
        EHMDState HMDState, 
        bool bSimulateTimeout = false);

    /**
     * Simulates the complete login flow for a player
     */
    POLAIR_CSEDITOR_API bool SimulatePlayerLogin(
        APACSTestGameMode* GameMode, 
        AMockPACS_PlayerController* PlayerController);

    /**
     * Waits for async spawn operations to complete
     */
    POLAIR_CSEDITOR_API void WaitForSpawnCompletion(UWorld* World, float MaxWaitTime = 5.0f);

    /**
     * Validates that a player controller has the expected pawn type
     */
    POLAIR_CSEDITOR_API bool ValidatePawnType(
        APlayerController* PC, 
        EHMDState ExpectedHMDState);

    /**
     * Creates multiple test clients for multi-client scenarios
     */
    POLAIR_CSEDITOR_API TArray<AMockPACS_PlayerController*> CreateMultipleTestClients(
        UWorld* World, 
        const TArray<EHMDState>& HMDStates);

    /**
     * Test result structure for complex test scenarios
     */
    struct POLAIR_CSEDITOR_API FHMDSpawnTestResult
    {
        bool bSuccess = false;
        FString ErrorMessage;
        int32 ExpectedCandidates = 0;
        int32 ActualCandidates = 0;
        int32 ExpectedAssessors = 0;
        int32 ActualAssessors = 0;
        
        bool IsValid() const 
        { 
            return bSuccess && 
                   ExpectedCandidates == ActualCandidates && 
                   ExpectedAssessors == ActualAssessors; 
        }
    };

    /**
     * Comprehensive test for multiple clients with different HMD states
     */
    POLAIR_CSEDITOR_API FHMDSpawnTestResult TestMultiClientSpawning(
        UWorld* World,
        const TArray<EHMDState>& ClientHMDStates);
}

#endif // WITH_DEV_AUTOMATION_TESTS