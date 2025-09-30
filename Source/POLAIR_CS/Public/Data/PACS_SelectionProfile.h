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
	Available = 3
};

/**
 * Data asset for NPC visual configuration and selection profiles
 * Organized according to Data Asset Revision specifications
 */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SelectionProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPACS_SelectionProfileAsset();

	// ========================================
	// NPC Visuals
	// ========================================

	// --- Skeletal Mesh ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Skeletal Mesh",
		meta = (DisplayName = "SK Asset"))
	TSoftObjectPtr<USkeletalMesh> SkeletalMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Skeletal Mesh",
		meta = (DisplayName = "SK Transforms"))
	FTransform SkeletalMeshTransform = FTransform::Identity;

	// --- Animation Instance ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Animation",
		meta = (DisplayName = "Animation Blueprint"))
	TSoftClassPtr<class UAnimInstance> AnimInstanceClass;

	// --- Static Mesh ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Static Mesh",
		meta = (DisplayName = "SM Asset"))
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Static Mesh",
		meta = (DisplayName = "SM Transforms"))
	FTransform StaticMeshTransform = FTransform::Identity;

	// --- Niagara ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Niagara",
		meta = (DisplayName = "Particle Effect"))
	TSoftObjectPtr<class UParticleSystem> ParticleEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Niagara",
		meta = (DisplayName = "Particle Effect Transforms"))
	FTransform ParticleEffectTransform = FTransform::Identity;

	// --- Selection Plane ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Selection Plane",
		meta = (DisplayName = "Selection SM"))
	TSoftObjectPtr<UStaticMesh> SelectionStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Selection Plane",
		meta = (DisplayName = "Selection SM Transforms"))
	FTransform SelectionStaticMeshTransform = FTransform(FRotator(-90.0f, 0, 0), FVector(0, 0, -88.0f), FVector(3.0f, 3.0f, 1.0f));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Selection Plane",
		meta = (DisplayName = "Collision Preset"))
	FName CollisionPreset = TEXT("Pawn");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Selection Plane",
		meta = (DisplayName = "Selection Trace Channel"))
	TEnumAsByte<ECollisionChannel> SelectionTraceChannel = ECC_GameTraceChannel1;

	// --- Max Render Distance ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Visuals|Max Render Distance",
		meta = (DisplayName = "Render Distance", ClampMin = 100.0, ClampMax = 10000.0))
	float RenderDistance = 5000.0f;

	// ========================================
	// Selection Profile
	// ========================================

	// --- Selection Material ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Selection Material",
		meta = (DisplayName = "Selection Material Instance"))
	TSoftObjectPtr<UMaterialInterface> SelectionMaterialInstance;

	// --- Available ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Available",
		meta = (DisplayName = "Available Brightness", ClampMin = 0, ClampMax = 25.0))
	float AvailableBrightness = 0.0f;  // No defaults - must be set in data asset

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Available",
		meta = (DisplayName = "Available Colour"))
	FLinearColor AvailableColour = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Black/invisible - must be set in data asset

	// --- Hovered ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Hovered",
		meta = (DisplayName = "Hovered Brightness", ClampMin = 0, ClampMax = 25.0))
	float HoveredBrightness = 0.0f;  // No defaults - must be set in data asset

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Hovered",
		meta = (DisplayName = "Hovered Colour"))
	FLinearColor HoveredColour = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Black/invisible - must be set in data asset

	// --- Selected ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Selected",
		meta = (DisplayName = "Selected Brightness", ClampMin = 0, ClampMax = 25.0))
	float SelectedBrightness = 0.0f;  // No defaults - must be set in data asset

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Selected",
		meta = (DisplayName = "Selected Colour"))
	FLinearColor SelectedColour = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Black/invisible - must be set in data asset

	// --- Unavailable ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Unavailable",
		meta = (DisplayName = "Unavailable Brightness", ClampMin = 0, ClampMax = 25.0))
	float UnavailableBrightness = 0.0f;  // No defaults - must be set in data asset

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Profile|Unavailable",
		meta = (DisplayName = "Unavailable Colour"))
	FLinearColor UnavailableColour = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Black/invisible - must be set in data asset

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif
};