#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/PACS_Poolable.h"
#include "PACS_NPC_Base_Char.generated.h"

class UBoxComponent;
class UPACS_SelectionPlaneComponent;

/**
 * Base Character class for humanoid NPCs in POLAIR_CS
 * Extends ACharacter with pooling support and selection system
 * Selection visuals are client-side only, excluded from VR/HMD clients
 * Uses CustomPrimitiveData for per-actor visual customization
 */
UCLASS(Abstract)
class POLAIR_CS_API APACS_NPC_Base_Char : public ACharacter, public IPACS_Poolable
{
	GENERATED_BODY()

public:
	APACS_NPC_Base_Char();

protected:
	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IPACS_Poolable interface
	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReturnedToPool_Implementation() override;

public:
	// Selection plane component for visual indicators (manages state and client-side visuals)
	// This component handles creating visual elements ONLY on non-VR clients
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPACS_SelectionPlaneComponent> SelectionPlaneComponent;

protected:
	// Selection state
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TWeakObjectPtr<class APlayerState> CurrentSelector;

	// Movement state for pooling
	UPROPERTY()
	float DefaultMaxWalkSpeed = 600.0f;

public:
	// Selection interface
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelected(bool bNewSelected, class APlayerState* Selector = nullptr);

	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "Selection")
	class APlayerState* GetCurrentSelector() const { return CurrentSelector.Get(); }

	// Set the selection profile (can be called by spawn system)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelectionProfile(class UPACS_SelectionProfileAsset* InProfile);

	// Apply the selection profile to the component
	virtual void ApplySelectionProfile();

	// Movement control
	UFUNCTION(BlueprintCallable, Category = "NPC Movement")
	virtual void MoveToLocation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "NPC Movement")
	virtual void StopMovement();

protected:
	// Visual feedback
	virtual void UpdateSelectionVisuals();

	// Pool state management
	virtual void ResetForPool();
	virtual void PrepareForUse();

	// Character-specific reset
	virtual void ResetCharacterMovement();
	virtual void ResetCharacterAnimation();

	// Async loading support for selection profiles
	virtual void OnSelectionProfileLoaded();
	void ApplySkeletalMeshFromProfile(class UPACS_SelectionProfileAsset* ProfileAsset);

	// Handle for async loading
	TSharedPtr<struct FStreamableHandle> ProfileLoadHandle;

};