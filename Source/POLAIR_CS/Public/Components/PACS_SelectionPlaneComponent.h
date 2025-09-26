#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PACS_SelectionProfile.h"
#include "PACS_SelectionPlaneComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UPACS_SelectionProfileAsset;

/**
 * Component that manages selection plane visuals for NPCs
 *
 * MULTIPLAYER ARCHITECTURE:
 * - Server: Only manages selection state (uint8), no visual components
 * - Clients: Create visual components locally based on replicated state
 * - VR/HMD: Automatically excluded from all visual operations
 *
 * Uses CustomPrimitiveData for efficient per-actor customization
 * Compatible with object pooling system
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POLAIR_CS_API UPACS_SelectionPlaneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPACS_SelectionPlaneComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The selection plane mesh component (client-side only, never on server)
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SelectionPlane;

	// Current visual profile
	UPROPERTY()
	FSelectionVisualProfile CurrentProfile;

	// Current plane configuration (client-side only)
	UPROPERTY()
	FSelectionPlaneConfiguration CurrentPlaneConfig;

	// Selection visual state (0=Hovered, 1=Selected, 2=Unavailable, 3=Available, 4=Hidden)
	UPROPERTY(ReplicatedUsing=OnRep_SelectionState)
	uint8 SelectionState = 3; // Default to Available

	// Client-only hover state (not replicated)
	uint8 LocalHoverState = 0;

	// Whether the component is initialized
	bool bIsInitialized = false;

public:
	// Initialize the selection plane (automatically called in BeginPlay for clients)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void InitializeSelectionPlane();

	// Apply configuration from profile asset (client-side only)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void ApplyProfileAsset(UPACS_SelectionProfileAsset* ProfileAsset);

	// Apply plane configuration directly (client-side only)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void ApplyPlaneConfiguration(const FSelectionPlaneConfiguration& PlaneConfig);

	// Apply NPC visual representation (for ALL clients including VR)
	UFUNCTION(BlueprintCallable, Category = "NPC Visual")
	void ApplyNPCVisualRepresentation(const FNPCVisualRepresentation& NPCVisual);

	// Apply a visual profile
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void ApplyProfile(const FSelectionVisualProfile& Profile);

	// Set selection state (server authoritative)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetSelectionState(ESelectionVisualState NewState);

	// Set hover state (client-side only)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetHoverState(bool bHovered);

	// Control visibility
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetSelectionPlaneVisible(bool bVisible);

	// Get the selection plane mesh component
	UFUNCTION(BlueprintPure, Category = "Selection")
	UStaticMeshComponent* GetSelectionPlane() const { return SelectionPlane; }

	// Check if selection visuals should be shown
	UFUNCTION(BlueprintPure, Category = "Selection")
	bool ShouldShowSelectionVisuals() const;

	// Update plane position based on owner bounds
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void UpdatePlanePosition();

	// Pool interface support
	void OnAcquiredFromPool();
	void OnReturnedToPool();

protected:
	// Create and setup the selection plane
	void SetupSelectionPlane();

	// Update CustomPrimitiveData values
	void UpdateSelectionPlaneCPD();

	// Update visual state
	void UpdateVisuals();

	// Calculate selection plane transform based on actor bounds (client-side)
	void CalculateSelectionPlaneTransform();

	// Validate and apply mesh/material references (client-side)
	void ValidateAndApplyAssets();

	// Replication callback
	UFUNCTION()
	void OnRep_SelectionState();

	// Get the root component to attach to
	USceneComponent* GetAttachmentRoot() const;

public:
	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Asset validation state (client-side only)
protected:
	// Whether assets have been validated for this session
	bool bAssetsValidated = false;

	// Cached references to avoid repeated soft pointer resolution
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedSelectionMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedPlaneMesh;
};