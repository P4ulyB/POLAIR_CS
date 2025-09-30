#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PACS_Poolable.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "PACS_NPC_Base.generated.h"

class UNiagaraComponent;
class UPACS_SelectionPlaneComponent;

/**
 * Base class for all NPC actors in POLAIR_CS
 * Implements poolable interface for object pooling system
 * Selection visuals are client-side only, excluded from VR/HMD clients
 * Uses CustomPrimitiveData for per-actor visual customization
 */
UCLASS(Abstract)
class POLAIR_CS_API APACS_NPC_Base : public AActor, public IPACS_Poolable, public IPACS_SelectableCharacterInterface
{
	GENERATED_BODY()

public:
	APACS_NPC_Base();

protected:
	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IPACS_Poolable interface
	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReturnedToPool_Implementation() override;

public:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UNiagaraComponent> NiagaraComponent;

	// Selection plane component for visual indicators (manages state and CPD)
	// This component handles client-side visual creation and server-side state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPACS_SelectionPlaneComponent> SelectionPlaneComponent;

protected:
	// Selection state
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TWeakObjectPtr<class APlayerState> CurrentSelector;

	// Hover state (client-side only)
	bool bIsLocallyHovered = false;


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

	// Movement (base implementation - override in derived classes)
	virtual void MoveToLocation(const FVector& TargetLocation) override {}
	virtual bool IsMoving() const override { return false; }

	// Hover/Highlight
	virtual void SetLocalHover(bool bHovered) override;
	virtual bool IsLocallyHovered() const override { return bIsLocallyHovered; }

	// Visual Components (base returns nullptr - override in derived classes)
	virtual class UMeshComponent* GetMeshComponent() const override { return nullptr; }
	virtual class UDecalComponent* GetSelectionDecal() const override { return nullptr; }

	// Set the selection profile (can be called by spawn system)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelectionProfile(class UPACS_SelectionProfileAsset* InProfile);

	// Apply the selection profile to the component
	virtual void ApplySelectionProfile();

protected:
	// Virtual method for subclasses to apply their specific mesh type
	// Base class handles particle effects, subclasses handle mesh variants
	virtual void ApplyNPCMeshFromProfile(class UPACS_SelectionProfileAsset* Profile);

	// Visual feedback
	virtual void UpdateSelectionVisuals();

	// Pool state management
	virtual void ResetForPool();
	virtual void PrepareForUse();

};