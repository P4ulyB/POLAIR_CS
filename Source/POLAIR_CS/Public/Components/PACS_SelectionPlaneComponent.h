#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PACS_SelectionProfile.h"
#include "Interfaces/PACS_Poolable.h"
#include "PACS_SelectionPlaneComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UPACS_SelectionProfileAsset;

/**
 * Struct for storing state visuals (color + brightness)
 */
USTRUCT()
struct FSelectionStateVisuals
{
	GENERATED_BODY()
	FLinearColor Color = FLinearColor::White;
	float Brightness = 1.0f;
};

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
class POLAIR_CS_API UPACS_SelectionPlaneComponent : public UActorComponent, public IPACS_Poolable
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

	// Current profile reference
	UPROPERTY()
	TObjectPtr<UPACS_SelectionProfileAsset> CurrentProfileAsset;

	// Selection visual state (0=Hovered, 1=Selected, 2=Unavailable, 3=Available)
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

	// Apply cached color/brightness values directly (for replicated Character NPCs)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void ApplyCachedColorValues(
		const FLinearColor& InAvailableColor, float InAvailableBrightness,
		const FLinearColor& InHoveredColor, float InHoveredBrightness,
		const FLinearColor& InSelectedColor, float InSelectedBrightness,
		const FLinearColor& InUnavailableColor, float InUnavailableBrightness);

	// Set selection state (server authoritative)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetSelectionState(ESelectionVisualState NewState);

	// Set hover state (client-side only)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetHoverState(bool bHovered);

	// Get the selection plane mesh component
	UFUNCTION(BlueprintPure, Category = "Selection")
	UStaticMeshComponent* GetSelectionPlane() const { return SelectionPlane; }

	// Check if selection visuals should be shown
	UFUNCTION(BlueprintPure, Category = "Selection")
	bool ShouldShowSelectionVisuals() const;

	// IPACS_Poolable interface
	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReturnedToPool_Implementation() override;

	// Update CustomPrimitiveData values (public for forced updates after material changes)
	void UpdateSelectionPlaneCPD();

protected:
	// Create and setup the selection plane
	void SetupSelectionPlane();

	// Update visual state
	void UpdateVisuals();

	// Validate and apply mesh/material references (client-side)
	void ValidateAndApplyAssets();

	// Replication callback
	UFUNCTION()
	void OnRep_SelectionState();

public:
	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Asset validation state (client-side only)
protected:
	// Store all 4 states indexed by ESelectionVisualState
	// [0]=Hovered, [1]=Selected, [2]=Unavailable, [3]=Available
	FSelectionStateVisuals StateVisuals[4];
	float RenderDistance = 5000.0f;

	// Whether assets have been validated for this session
	bool bAssetsValidated = false;

	// Cached references to avoid repeated soft pointer resolution
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedSelectionMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedPlaneMesh;
};