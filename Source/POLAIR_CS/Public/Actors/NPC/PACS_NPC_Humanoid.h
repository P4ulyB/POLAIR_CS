// Copyright Notice: Property of Particular Audience Limited

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "PACS_NPC_Humanoid.generated.h"

class USkeletalMeshComponent;
class UBoxComponent;
class UFloatingPawnMovement;
class UDecalComponent;
class UAnimSequence;
class UPACS_NPC_v2_Config;

/**
 * Lightweight NPC implementation using APawn instead of ACharacter
 * Uses FloatingPawnMovement instead of CharacterMovementComponent for massive performance gains
 * Dead simple implementation - no unnecessary features
 */
UCLASS()
class POLAIR_CS_API APACS_NPC_Humanoid : public APawn, public IPACS_SelectableCharacterInterface
{
    GENERATED_BODY()

public:
    APACS_NPC_Humanoid();

    // Components
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* SkeletalMeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UFloatingPawnMovement* FloatingMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UDecalComponent* SelectionDecal;

    // Configuration
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
    UPACS_NPC_v2_Config* NPCConfig;

    // Allow pool to set cached material
    UMaterialInstanceDynamic* CachedDecalMaterial;

    // Replicated Properties
protected:
    UPROPERTY(ReplicatedUsing = OnRep_CurrentSelector, BlueprintReadOnly, Category = "Selection")
    APlayerState* CurrentSelector;

    UPROPERTY(Replicated)
    FVector TargetLocation;

    UPROPERTY(Replicated)
    bool bIsMoving;

    // Local State
private:
    bool bIsLocallyHovered;
    UAnimSequence* LoadedIdleAnimation;
    UAnimSequence* LoadedMoveAnimation;
    float ConfiguredWalkSpeed;

    // Overrides
public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Interface Implementation
public:
    virtual APlayerState* GetCurrentSelector() const override { return CurrentSelector; }
    virtual void SetCurrentSelector(APlayerState* Selector) override;
    virtual bool IsSelectedBy(APlayerState* InPlayerState) const override { return CurrentSelector == InPlayerState; }

    virtual void MoveToLocation(const FVector& Location) override;
    virtual bool IsMoving() const override { return bIsMoving; }

    virtual void SetLocalHover(bool bHovered) override;
    virtual bool IsLocallyHovered() const override { return bIsLocallyHovered; }

    virtual UMeshComponent* GetMeshComponent() const override { return SkeletalMeshComponent; }
    virtual UDecalComponent* GetSelectionDecal() const override { return SelectionDecal; }

    // Movement
    UFUNCTION(Server, Reliable)
    void ServerMoveToLocation(FVector Location);

protected:
    // Replication Callbacks
    UFUNCTION()
    void OnRep_CurrentSelector();

    // Movement Callbacks
    void OnMoveCompleted(struct FAIRequestID RequestID, const struct FPathFollowingResult& Result);

private:
    // Internal Methods
    void LoadAssetsFromConfig();
    void UpdateAnimation();
    void UpdateVisualState();
    void ApplyDecalState(float Brightness, const FLinearColor& Color);
};