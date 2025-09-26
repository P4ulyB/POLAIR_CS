#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PACS_PlayerState.h"
#include "PACSGameMode.generated.h"


UCLASS()
class POLAIR_CS_API APACSGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APACSGameMode();

    virtual void BeginPlay() override;

    // Override for player connection handling
    virtual void PostLogin(APlayerController* NewPlayer) override;
        
    virtual void PreLogin(const FString& Options, const FString& Address, 
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
        
    virtual void Logout(AController* Exiting) override;

    // Timeout handler for zero-swap pattern - made public for testing
    UFUNCTION()
    void OnHMDTimeout(APlayerController* PlayerController);

protected:
    // Pawn classes for different user types
    UPROPERTY(EditDefaultsOnly, Category = "PACS|Spawning")
    TSubclassOf<APawn> CandidatePawnClass;

    UPROPERTY(EditDefaultsOnly, Category = "PACS|Spawning")
    TSubclassOf<APawn> AssessorPawnClass;

    // Override to provide custom pawn selection logic
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

    // Zero-swap spawn implementation - BlueprintNativeEvent override
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
