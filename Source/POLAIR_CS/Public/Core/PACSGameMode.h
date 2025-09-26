#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PACS_PlayerState.h"
#include "PACSGameMode.generated.h"

class UPACS_SpawnConfiguration;

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

    /**
     * Get the effective spawn configuration (with level override support)
     */
    UFUNCTION(BlueprintCallable, Category = "PACS|NPC Spawning")
    UPACS_SpawnConfiguration* GetEffectiveSpawnConfiguration() const;

protected:
    // Pawn classes for different user types
    UPROPERTY(EditDefaultsOnly, Category = "PACS|Spawning")
    TSubclassOf<APawn> CandidatePawnClass;

    UPROPERTY(EditDefaultsOnly, Category = "PACS|Spawning")
    TSubclassOf<APawn> AssessorPawnClass;

    // NPC Spawn Configuration
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PACS|NPC Spawning", meta = (DisplayName = "NPC Spawn Configuration"))
    TSoftObjectPtr<UPACS_SpawnConfiguration> NPCSpawnConfiguration;

    // Optional per-level override
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PACS|NPC Spawning", meta = (DisplayName = "Level Spawn Config Override"))
    TSoftObjectPtr<UPACS_SpawnConfiguration> LevelSpawnConfigOverride;

    // Override to provide custom pawn selection logic
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

    // Zero-swap spawn implementation - BlueprintNativeEvent override
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
