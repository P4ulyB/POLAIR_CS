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

	AnimInstanceClass = Profile->AnimInstanceClass;

	ParticleEffect = Profile->ParticleEffect;
	ParticleEffectTransform = Profile->ParticleEffectTransform;

	// Populate selection profile data
	SelectionStaticMesh = Profile->SelectionStaticMesh;
	SelectionStaticMeshTransform = Profile->SelectionStaticMeshTransform;
	SelectionMaterialInstance = Profile->SelectionMaterialInstance;

	// Selection colors and brightness
	AvailableColour = Profile->AvailableColour;
	AvailableBrightness = Profile->AvailableBrightness;
	UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData: Loaded Available from profile: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		AvailableColour.R, AvailableColour.G, AvailableColour.B, AvailableColour.A, AvailableBrightness);

	HoveredColour = Profile->HoveredColour;
	HoveredBrightness = Profile->HoveredBrightness;
	UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData: Loaded Hovered from profile: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		HoveredColour.R, HoveredColour.G, HoveredColour.B, HoveredColour.A, HoveredBrightness);

	SelectedColour = Profile->SelectedColour;
	SelectedBrightness = Profile->SelectedBrightness;
	UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData: Loaded Selected from profile: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		SelectedColour.R, SelectedColour.G, SelectedColour.B, SelectedColour.A, SelectedBrightness);

	UnavailableColour = Profile->UnavailableColour;
	UnavailableBrightness = Profile->UnavailableBrightness;
	UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData: Loaded Unavailable from profile: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		UnavailableColour.R, UnavailableColour.G, UnavailableColour.B, UnavailableColour.A, UnavailableBrightness);

	// Other settings
	CollisionPreset = Profile->CollisionPreset;
	RenderDistance = Profile->RenderDistance;
	SelectionTraceChannel = Profile->SelectionTraceChannel;

	UE_LOG(LogTemp, Warning, TEXT("FNPCProfileData::PopulateFromProfile: COMPLETE - Cached all data from profile %s"),
		*Profile->GetName());
}