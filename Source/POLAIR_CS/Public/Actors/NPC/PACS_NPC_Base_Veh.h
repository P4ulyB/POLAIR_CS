#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Interfaces/PACS_Poolable.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "PACS_NPC_Base_Veh.generated.h"

class UBoxComponent;
class UPACS_SelectionPlaneComponent;

/**
 * Base Vehicle class for vehicle NPCs in POLAIR_CS
 * Extends AWheeledVehiclePawn with pooling support and selection system
 * Selection visuals are client-side only, excluded from VR/HMD clients
 * Uses CustomPrimitiveData for per-actor visual customization
 */
UCLASS(Abstract)
class POLAIR_CS_API APACS_NPC_Base_Veh : public AWheeledVehiclePawn, public IPACS_Poolable, public IPACS_SelectableCharacterInterface
{
	GENERATED_BODY()

public:
	APACS_NPC_Base_Veh();

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

	UPROPERTY(ReplicatedUsing=OnRep_CurrentSelector, BlueprintReadOnly, Category = "Selection")
	TWeakObjectPtr<class APlayerState> CurrentSelector;

	// Replication callback for CurrentSelector
	UFUNCTION()
	void OnRep_CurrentSelector();

	// Hover state (client-side only)
	bool bIsLocallyHovered = false;

	// Vehicle state for pooling
	UPROPERTY()
	bool bEngineStartedByDefault = true;

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
	virtual void MoveToLocation(const FVector& TargetLocation) override { DriveToLocation(TargetLocation); }
	virtual bool IsMoving() const override;
	virtual void SetLocalHover(bool bHovered) override;
	virtual bool IsLocallyHovered() const override { return bIsLocallyHovered; }
	virtual class UMeshComponent* GetMeshComponent() const override { return GetMesh(); }
	virtual class UDecalComponent* GetSelectionDecal() const override { return nullptr; }

	// Set the selection profile (can be called by spawn system)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelectionProfile(class UPACS_SelectionProfileAsset* InProfile);

	// Apply the selection profile to the component
	virtual void ApplySelectionProfile();

	// Vehicle control
	UFUNCTION(BlueprintCallable, Category = "NPC Vehicle")
	virtual void DriveToLocation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "NPC Vehicle")
	virtual void StopVehicle();

	UFUNCTION(BlueprintCallable, Category = "NPC Vehicle")
	virtual void SetHandbrake(bool bEngaged);

protected:
	// Method for applying vehicle mesh from profile
	virtual void ApplyNPCMeshFromProfile(class UPACS_SelectionProfileAsset* Profile);

	// Visual feedback
	virtual void UpdateSelectionVisuals();

	// Pool state management
	virtual void ResetForPool();
	virtual void PrepareForUse();

	// Vehicle-specific reset
	virtual void ResetVehicleState();
	virtual void ResetVehiclePhysics();

public:
	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

};