#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PACS_Poolable.h"
#include "PACS_NPC_Base_LW.generated.h"

class UBoxComponent;
class UFloatingPawnMovement;
class USkeletalMeshComponent;
class UPACS_SelectionPlaneComponent;
class UPACS_SelectionProfileAsset;
class UAnimSequence;
class USignificanceManager;

// Movement state enum for lightweight NPCs
UENUM(BlueprintType)
enum class EPACSLightweightNPCMovementState : uint8
{
	Idle = 0,
	Moving = 1
};

/**
 * Lightweight NPC base class for simple movement without Character Movement Component
 * Designed for flat terrain with basic Idle/Run animation states
 * Optimized for VR performance (90 FPS) and network bandwidth (<100KB/s)
 *
 * Features:
 * - Box collision for efficient flat terrain navigation
 * - FloatingPawnMovement for simple velocity-based movement
 * - Direct animation sequence playback (no AnimBlueprint overhead)
 * - Significance Manager integration for LOD optimization
 * - Replication Graph optimized with custom net priorities
 * - Full pooling support via IPACS_Poolable interface
 */
UCLASS()
class POLAIR_CS_API APACS_NPC_Base_LW : public APawn, public IPACS_Poolable
{
	GENERATED_BODY()

public:
	APACS_NPC_Base_LW(const FObjectInitializer& ObjectInitializer);

protected:
	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IPACS_Poolable interface
	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReturnedToPool_Implementation() override;

public:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFloatingPawnMovement> FloatingMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPACS_SelectionPlaneComponent> SelectionPlaneComponent;

protected:

	// Replicated movement state (1 byte)
	UPROPERTY(ReplicatedUsing = OnRep_MovementState)
	uint8 MovementStateRep;

	// Target location for movement (compressed)
	UPROPERTY(ReplicatedUsing = OnRep_TargetLocation, meta = (AllowPrivateAccess = "true"))
	FVector_NetQuantize TargetLocation;

	// Movement configuration
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float MovementSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float StoppingDistance = 50.0f;

	// Animation assets (loaded from SelectionProfile)
	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY()
	TObjectPtr<UAnimSequence> RunAnimation;

	// Selection state
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TWeakObjectPtr<class APlayerState> CurrentSelector;

	// Significance tracking
	UPROPERTY()
	float CurrentSignificance = 0.0f;

	// Current selection profile asset
	UPROPERTY()
	TObjectPtr<UPACS_SelectionProfileAsset> CurrentProfileAsset;

	// Async loading handle for selection profile
	TSharedPtr<struct FStreamableHandle> ProfileLoadHandle;

public:
	// Movement interface
	UFUNCTION(BlueprintCallable, Category = "NPC Movement", BlueprintAuthorityOnly)
	virtual void MoveToLocation(const FVector& InTargetLocation);

	UFUNCTION(BlueprintCallable, Category = "NPC Movement", BlueprintAuthorityOnly)
	virtual void StopMovement();

	// Selection interface
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelected(bool bNewSelected, class APlayerState* Selector = nullptr);

	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "Selection")
	class APlayerState* GetCurrentSelector() const { return CurrentSelector.Get(); }

	// Selection profile management
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelectionProfile(UPACS_SelectionProfileAsset* InProfile);

	// Apply the selection profile to components
	virtual void ApplySelectionProfile();

	// Significance management
	UFUNCTION()
	void UpdateSignificance(float NewSignificance);

	// Network optimization overrides
	virtual float GetNetPriority(const FVector& ViewPos, const FVector& ViewDir,
	                            AActor* Viewer, AActor* ViewTarget,
	                            UActorChannel* InChannel, float Time,
	                            bool bLowBandwidth) override;

protected:
	// Replication callbacks
	UFUNCTION()
	void OnRep_MovementState();

	UFUNCTION()
	void OnRep_TargetLocation();

	// Animation management
	void PlayIdleAnimation();
	void PlayRunAnimation();
	void UpdateAnimationState();

	// Movement update
	virtual void UpdateMovement(float DeltaSeconds);

	// Visual feedback
	virtual void UpdateSelectionVisuals();

	// Pool state management
	virtual void ResetForPool();
	virtual void PrepareForUse();

	// Profile loading callbacks
	virtual void OnSelectionProfileLoaded();
	void ApplyAnimationsFromProfile(UPACS_SelectionProfileAsset* ProfileAsset);
	void ApplySkeletalMeshFromProfile(UPACS_SelectionProfileAsset* ProfileAsset);

	// Helper to get current movement state
	EPACSLightweightNPCMovementState GetMovementState() const { return static_cast<EPACSLightweightNPCMovementState>(MovementStateRep); }
	void SetMovementState(EPACSLightweightNPCMovementState NewState);

	// Check if we've reached target
	bool HasReachedTarget() const;

	// Register with significance manager
	void RegisterWithSignificanceManager();
	void UnregisterFromSignificanceManager();
};