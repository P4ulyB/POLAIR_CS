#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PACS_PlayerState.generated.h"

UENUM(BlueprintType)
enum class EHMDState : uint8
{
    Unknown = 0,    // HMD state not yet detected
    NoHMD = 1,      // No HMD connected/enabled
    HasHMD = 2      // HMD connected and enabled
};

UCLASS()
class POLAIR_CS_API APACS_PlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    APACS_PlayerState();

#pragma region Lifecycle
protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#pragma endregion

#pragma region VR/HMD Management
public:
    // HMD detection state - replicated to all clients for system-wide access
    UPROPERTY(ReplicatedUsing=OnRep_HMDState, BlueprintReadOnly, Category = "PACS")
    EHMDState HMDState = EHMDState::Unknown;

    // RepNotify for HMD state changes - made public for testing
    UFUNCTION()
    void OnRep_HMDState();
#pragma endregion

#pragma region Selection System
public:
    // Selection System - Server-only tracking (not replicated)

    // Single selection methods (backward compatibility)
    UFUNCTION(BlueprintCallable, Category = "Selection")
    AActor* GetSelectedActor() const;

    void SetSelectedActor(AActor* InActor);

    // Multi-selection methods
    UFUNCTION(BlueprintCallable, Category = "Selection")
    TArray<AActor*> GetSelectedActors() const;

    void AddSelectedActor(AActor* InActor);
    void RemoveSelectedActor(AActor* InActor);
    void ClearSelectedActors();
    void SetSelectedActors(const TArray<AActor*>& InActors);

    // Debug helper to log current selection state
    UFUNCTION(BlueprintCallable, Category = "Selection|Debug")
    void LogCurrentSelection() const;

private:
    // Server-only: currently selected actors for this player (weak ptrs for safety)
    TArray<TWeakObjectPtr<AActor>> SelectedActors_ServerOnly;
#pragma endregion
};