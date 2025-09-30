#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PACS_NPCProfileData.generated.h"

/**
 * Cached NPC profile data for atomic replication
 * Contains all visual and configuration data from UPACS_SelectionProfileAsset
 * Replicated as a single struct to eliminate race conditions
 */
USTRUCT(BlueprintType)
struct POLAIR_CS_API FNPCProfileData
{
	GENERATED_BODY()

	// ========================================
	// Visual Assets
	// ========================================

	// Skeletal Mesh
	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	TSoftObjectPtr<USkeletalMesh> SkeletalMeshAsset;

	// CRITICAL: Use separate Location/Rotation/Scale for precise replication
	// FTransform uses compressed quaternions which lose precision over network
	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	FVector SkeletalMeshLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	FRotator SkeletalMeshRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	FVector SkeletalMeshScale = FVector(1.0f, 1.0f, 1.0f);

	// Static Mesh (for lightweight/vehicle NPCs)
	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;

	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	FVector StaticMeshLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	FRotator StaticMeshRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	FVector StaticMeshScale = FVector(1.0f, 1.0f, 1.0f);

	// Animations
	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	TSoftObjectPtr<class UAnimSequence> IdleAnimationSequence;

	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	TSoftObjectPtr<class UAnimSequence> RunAnimationSequence;

	// Particle Effects
	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	TSoftObjectPtr<class UParticleSystem> ParticleEffect;

	UPROPERTY(BlueprintReadWrite, Category = "Visual Assets")
	FTransform ParticleEffectTransform = FTransform::Identity;

	// ========================================
	// Selection Profile Data
	// ========================================

	// Selection Plane Mesh
	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	TSoftObjectPtr<UStaticMesh> SelectionStaticMesh;

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	FTransform SelectionStaticMeshTransform = FTransform(FRotator(-90.0f, 0, 0), FVector(0, 0, -88.0f), FVector(3.0f, 3.0f, 1.0f));

	// Selection Material
	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	TSoftObjectPtr<UMaterialInterface> SelectionMaterialInstance;

	// Selection Colors and Brightness
	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	FLinearColor AvailableColour = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	float AvailableBrightness = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	FLinearColor HoveredColour = FLinearColor(0.2f, 0.5f, 1.0f, 1.0f);

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	float HoveredBrightness = 1.2f;

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	FLinearColor SelectedColour = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	float SelectedBrightness = 1.5f;

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	FLinearColor UnavailableColour = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f);

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	float UnavailableBrightness = 0.5f;

	// Other Settings
	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	FName CollisionPreset = TEXT("Pawn");

	UPROPERTY(BlueprintReadWrite, Category = "Selection Profile")
	float RenderDistance = 5000.0f;

	// ========================================
	// Helper Methods
	// ========================================

	/**
	 * Populate this struct from a SelectionProfileAsset
	 * Called on server to cache profile data before replication
	 */
	void PopulateFromProfile(const class UPACS_SelectionProfileAsset* Profile);

	/**
	 * Validate that this struct contains valid data
	 * At minimum, must have either skeletal or static mesh
	 */
	bool IsValid() const
	{
		return !SkeletalMeshAsset.IsNull() || !StaticMeshAsset.IsNull();
	}

	/**
	 * Reset to default state (for pooling)
	 */
	void Reset()
	{
		SkeletalMeshAsset.Reset();
		StaticMeshAsset.Reset();
		SkeletalMeshLocation = FVector::ZeroVector;
		SkeletalMeshRotation = FRotator::ZeroRotator;
		SkeletalMeshScale = FVector(1.0f, 1.0f, 1.0f);
		StaticMeshLocation = FVector::ZeroVector;
		StaticMeshRotation = FRotator::ZeroRotator;
		StaticMeshScale = FVector(1.0f, 1.0f, 1.0f);
		IdleAnimationSequence.Reset();
		RunAnimationSequence.Reset();
		ParticleEffect.Reset();
	}
};