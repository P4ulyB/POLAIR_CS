#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PACS_SelectionProfile.generated.h"

/**
 * Visual states for NPC selection indicators
 */
UENUM(BlueprintType)
enum class ESelectionVisualState : uint8
{
    Hovered = 0,
    Selected = 1,
    Unavailable = 2,
    Available = 3,
    Hidden = 4
};

/**
 * NPC Visual Representation Configuration
 * These meshes are GAMEPLAY CRITICAL and visible to ALL clients including VR/HMD
 * Used for the actual NPC representation (human, vehicle, fire, smoke, etc.)
 */
USTRUCT(BlueprintType)
struct POLAIR_CS_API FNPCVisualRepresentation
{
    GENERATED_BODY()

    // Primary mesh for the NPC (skeletal for characters, static for props)
    // VISIBLE TO ALL INCLUDING VR
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual Representation",
        meta = (ToolTip = "Main visual mesh. Visible to ALL clients including VR/HMD."))
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;

    // Alternative static mesh (for vehicles, props, etc.)
    // VISIBLE TO ALL INCLUDING VR
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual Representation",
        meta = (ToolTip = "Static mesh for non-character NPCs. Visible to ALL clients including VR/HMD."))
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    // Transform for the visual representation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual Representation")
    FTransform MeshTransform = FTransform::Identity;

    // VFX for special NPCs (fire, smoke, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual Effects",
        meta = (ToolTip = "Particle system for fire/smoke NPCs. Visible to ALL clients including VR/HMD."))
    TSoftObjectPtr<class UParticleSystem> ParticleEffect;

    FNPCVisualRepresentation()
    {
        // Default constructor
    }
};

/**
 * Selection plane configuration for NON-VR client visual feedback only
 * NEVER visible to VR/HMD clients or used on dedicated servers
 * This is purely for flat-screen client selection indication
 */
USTRUCT(BlueprintType)
struct POLAIR_CS_API FSelectionPlaneConfiguration
{
    GENERATED_BODY()

    // Static mesh for the selection plane (NON-VR ONLY)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection Plane",
        meta = (ToolTip = "Selection plane mesh. NEVER shown to VR/HMD clients. Client asset only."))
    TSoftObjectPtr<UStaticMesh> SelectionPlaneMesh;

    // Material for selection plane (NON-VR ONLY)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection Plane",
        meta = (ToolTip = "Selection plane material. NEVER shown to VR/HMD clients."))
    TSoftObjectPtr<UMaterialInterface> SelectionMaterial;

    // Transform for selection plane relative to actor root
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection Plane")
    FTransform SelectionPlaneTransform = FTransform(FRotator(-90.0f, 0, 0), FVector(0, 0, -88.0f), FVector(3.0f, 3.0f, 1.0f));

    // Auto-calculate plane transform based on actor bounds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection Plane")
    bool bAutoCalculatePlaneTransform = true;

    // Maximum render distance for selection plane
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance",
        meta = (ClampMin = 100.0, ClampMax = 10000.0))
    float MaxRenderDistance = 5000.0f;

    FSelectionPlaneConfiguration()
    {
        // Default constructor
    }
};

/**
 * Configuration for NPC selection visual profiles
 * Used with CustomPrimitiveData for per-actor customization
 */
USTRUCT(BlueprintType)
struct POLAIR_CS_API FSelectionVisualProfile
{
    GENERATED_BODY()

    // Visual state colors (indexed by ESelectionVisualState)
    UPROPERTY(EditAnywhere, Category = "Visual States")
    FLinearColor HoveredColor = FLinearColor(0.2f, 0.5f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "Visual States")
    FLinearColor SelectedColor = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "Visual States")
    FLinearColor UnavailableColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f);

    UPROPERTY(EditAnywhere, Category = "Visual States")
    FLinearColor AvailableColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);

    // Brightness multipliers for each state
    UPROPERTY(EditAnywhere, Category = "Visual States", meta = (ClampMin = 0.1, ClampMax = 3.0))
    float HoveredBrightness = 1.2f;

    UPROPERTY(EditAnywhere, Category = "Visual States", meta = (ClampMin = 0.1, ClampMax = 3.0))
    float SelectedBrightness = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Visual States", meta = (ClampMin = 0.1, ClampMax = 3.0))
    float UnavailableBrightness = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Visual States", meta = (ClampMin = 0.1, ClampMax = 3.0))
    float AvailableBrightness = 1.0f;

    // Scale multiplier for selection plane relative to actor bounds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane Settings",
        meta = (ClampMin = 0.5, ClampMax = 3.0))
    float ProjectionScale = 1.5f;

    // Z-offset from ground to prevent z-fighting
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane Settings",
        meta = (ClampMin = 0.0, ClampMax = 10.0))
    float GroundOffset = 2.0f;

    // Optional fade radius for soft edges
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual Settings",
        meta = (ClampMin = 0.5, ClampMax = 2.0))
    float FadeRadius = 1.0f;

    FSelectionVisualProfile()
    {
        // Default constructor with sensible defaults
    }
};

/**
 * Data asset for storing selection visual profiles and mesh configuration
 * Can be referenced by spawn configs for different NPC types
 */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SelectionProfileAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPACS_SelectionProfileAsset();

    // The visual profile configuration
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile")
    FSelectionVisualProfile Profile;

    // NPC visual representation (VISIBLE TO ALL INCLUDING VR)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visual",
        meta = (ToolTip = "Main NPC visual representation. Visible to ALL clients including VR/HMD."))
    FNPCVisualRepresentation NPCVisual;

    // Selection plane configuration (NON-VR CLIENTS ONLY)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Visual",
        meta = (ToolTip = "Selection indicator for non-VR clients only. NEVER shown to VR/HMD users."))
    FSelectionPlaneConfiguration PlaneConfig;

    // Optional material override for this profile
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile",
        meta = (ToolTip = "Override material for selection plane. If not set, uses default M_PACS_Selection."))
    TSoftObjectPtr<UMaterialInterface> SelectionMaterialOverride;

    // DEPRECATED - Use MeshConfig.SelectionPlaneMesh instead
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile",
        meta = (DeprecatedProperty, DeprecationMessage = "Use MeshConfig.SelectionPlaneMesh instead"))
    TSoftObjectPtr<UStaticMesh> PlaneMeshOverride;

    // Get the effective plane mesh (handles deprecated property)
    UFUNCTION(BlueprintPure, Category = "Selection Profile")
    TSoftObjectPtr<UStaticMesh> GetEffectivePlaneMesh() const
    {
        // Prefer new config, fallback to deprecated property
        if (!PlaneConfig.SelectionPlaneMesh.IsNull())
            return PlaneConfig.SelectionPlaneMesh;
        return PlaneMeshOverride;
    }

    // Get the effective selection material
    UFUNCTION(BlueprintPure, Category = "Selection Profile")
    TSoftObjectPtr<UMaterialInterface> GetEffectiveSelectionMaterial() const
    {
        // Prefer plane config, fallback to override
        if (!PlaneConfig.SelectionMaterial.IsNull())
            return PlaneConfig.SelectionMaterial;
        return SelectionMaterialOverride;
    }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif
};