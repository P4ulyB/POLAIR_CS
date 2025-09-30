#include "Data/PACS_NPCProfileData.h"
#include "Data/PACS_SelectionProfile.h"

void FNPCProfileData::PopulateFromProfile(const UPACS_SelectionProfileAsset* Profile)
{
	if (!Profile)
	{
		UE_LOG(LogTemp, Error, TEXT("FNPCProfileData::PopulateFromProfile: NULL PROFILE PROVIDED"));
		Reset();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData::PopulateFromProfile: Starting population from profile %s"), *Profile->GetName());

	// Populate visual assets
	SkeletalMeshAsset = Profile->SkeletalMeshAsset;

	// Decompose transform to avoid quantization issues with FTransform replication
	// FTransform uses compressed quaternions which lose precision over network
	SkeletalMeshLocation = Profile->SkeletalMeshTransform.GetLocation();
	SkeletalMeshRotation = Profile->SkeletalMeshTransform.Rotator();
	SkeletalMeshScale = Profile->SkeletalMeshTransform.GetScale3D();

	// CRITICAL LOGGING: Verify transform values are being cached
	UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData::PopulateFromProfile: Cached SK Transform - Loc=%s, Rot=%s, Scale=%s"),
		*SkeletalMeshLocation.ToString(),
		*SkeletalMeshRotation.ToString(),
		*SkeletalMeshScale.ToString());

	// Validate asset references
	if (SkeletalMeshAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData::PopulateFromProfile: SkeletalMeshAsset is NULL in profile"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("FNPCProfileData::PopulateFromProfile: SkeletalMeshAsset cached: %s"),
			*SkeletalMeshAsset.ToString());
	}

	StaticMeshAsset = Profile->StaticMeshAsset;
	StaticMeshLocation = Profile->StaticMeshTransform.GetLocation();
	StaticMeshRotation = Profile->StaticMeshTransform.Rotator();
	StaticMeshScale = Profile->StaticMeshTransform.GetScale3D();

	IdleAnimationSequence = Profile->IdleAnimationSequence;
	RunAnimationSequence = Profile->RunAnimationSequence;

	ParticleEffect = Profile->ParticleEffect;
	ParticleEffectTransform = Profile->ParticleEffectTransform;

	// Populate selection profile data
	SelectionStaticMesh = Profile->SelectionStaticMesh;
	SelectionStaticMeshTransform = Profile->SelectionStaticMeshTransform;
	SelectionMaterialInstance = Profile->SelectionMaterialInstance;

	// Selection colors and brightness
	AvailableColour = Profile->AvailableColour;
	AvailableBrightness = Profile->AvailableBrightness;

	HoveredColour = Profile->HoveredColour;
	HoveredBrightness = Profile->HoveredBrightness;

	SelectedColour = Profile->SelectedColour;
	SelectedBrightness = Profile->SelectedBrightness;

	UnavailableColour = Profile->UnavailableColour;
	UnavailableBrightness = Profile->UnavailableBrightness;

	// Other settings
	CollisionPreset = Profile->CollisionPreset;
	RenderDistance = Profile->RenderDistance;

	UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData::PopulateFromProfile: COMPLETE - Cached all data from profile %s"),
		*Profile->GetName());
}