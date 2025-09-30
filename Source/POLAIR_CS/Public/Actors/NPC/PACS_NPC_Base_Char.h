#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/PACS_Poolable.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Data/PACS_NPCProfileData.h"
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
class POLAIR_CS_API APACS_NPC_Base_Char : public ACharacter, public IPACS_Poolable, public IPACS_SelectableCharacterInterface
{
	GENERATED_BODY()

public:
	APACS_NPC_Base_Char();

protected:
	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	// Cached NPC profile data for atomic replication
	// Contains all visual and configuration data - replicates as single struct to eliminate race conditions
	UPROPERTY(ReplicatedUsing = OnRep_CachedProfileData)
	struct FNPCProfileData CachedProfileData;

	// Movement state for pooling
	UPROPERTY()
	float DefaultMaxWalkSpeed = 600.0f;

	// Called when cached profile data replicates to clients
	UFUNCTION()
	void OnRep_CachedProfileData();

	// Apply cached profile data in controlled sequence (mesh -> transform -> materials -> animations)
	void ApplyCachedProfileData();

public:
	// Selection interface
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelected(bool bNewSelected, class APlayerState* Selector = nullptr);

	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsSelected() const { return bIsSelected; }

	// IPACS_SelectableCharacterInterface implementation
	UFUNCTION(BlueprintPure, Category = "Selection")
	virtual class APlayerState* GetCurrentSelector() const override { return CurrentSelector.Get(); }
	virtual void SetCurrentSelector(class APlayerState* Selector) override { CurrentSelector = Selector; }
	virtual bool IsSelectedBy(class APlayerState* InPlayerState) const override { return CurrentSelector.Get() == InPlayerState; }
	UFUNCTION(BlueprintCallable, Category = "NPC Movement")
	virtual void MoveToLocation(const FVector& TargetLocation) override;
	virtual bool IsMoving() const override;
	virtual void SetLocalHover(bool bHovered) override;
	virtual bool IsLocallyHovered() const override { return bIsLocallyHovered; }
	virtual class UMeshComponent* GetMeshComponent() const override { return GetMesh(); }
	virtual class UDecalComponent* GetSelectionDecal() const override { return nullptr; }

	// Set the selection profile (can be called by spawn system)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelectionProfile(class UPACS_SelectionProfileAsset* InProfile);

	// Movement control (MoveToLocation is defined in interface implementation above)
	UFUNCTION(BlueprintCallable, Category = "NPC Movement")
	virtual void StopMovement();

protected:
	// Hover state (client-side only)
	bool bIsLocallyHovered = false;

	// Visual feedback
	virtual void UpdateSelectionVisuals();

	// Pool state management
	virtual void ResetForPool();
	virtual void PrepareForUse();

	// Character-specific reset
	virtual void ResetCharacterMovement();
	virtual void ResetCharacterAnimation();
};