#include "Data/Configs/PACS_NPCConfig.h"
#include "Data/PACS_NPCVisualConfig.h"
#include "Settings/PACS_SelectionSystemSettings.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#endif

void UPACS_NPCConfig::ToVisualConfig(FPACS_NPCVisualConfig& Out) const
{
	Out = {};
	if (SkeletalMesh.IsValid() || SkeletalMesh.ToSoftObjectPath().IsValid())
	{
		Out.FieldsMask |= 0x1;
		Out.MeshPath = SkeletalMesh.ToSoftObjectPath();
	}
	if (AnimClass.IsValid() || AnimClass.ToSoftObjectPath().IsValid())
	{
		Out.FieldsMask |= 0x2;
		Out.AnimClassPath = AnimClass.ToSoftObjectPath();
	}
	// Always include collision scale even if 0
	Out.FieldsMask |= 0x4;
	Out.CollisionScaleSteps = CollisionScaleSteps;

	// Include decal material if set
	if (DecalMaterial.IsValid() || DecalMaterial.ToSoftObjectPath().IsValid())
	{
		Out.FieldsMask |= 0x8;
		Out.DecalMaterialPath = DecalMaterial.ToSoftObjectPath();
	}

	// Include mesh transform if different from defaults
	if (!MeshLocation.IsZero() || !MeshRotation.IsZero() || !MeshScale.Equals(FVector::OneVector))
	{
		Out.FieldsMask |= 0x10;
		Out.MeshLocation = MeshLocation;
		Out.MeshRotation = MeshRotation;
		Out.MeshScale = MeshScale;
	}
}

#if WITH_EDITOR
EDataValidationResult UPACS_NPCConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!SkeletalMesh.ToSoftObjectPath().IsValid())
	{
		Context.AddError(FText::FromString(TEXT("SkeletalMesh not set")));
		Result = EDataValidationResult::Invalid;
	}

	if (!AnimClass.ToSoftObjectPath().IsValid())
	{
		Context.AddError(FText::FromString(TEXT("AnimClass not set")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

// FPACS_NPCVisualConfig method implementations
void FPACS_NPCVisualConfig::ApplySelectionFromGlobalSettings(const UClass* CharacterClass)
{
	if (!CharacterClass)
	{
		return;
	}

	// Get global selection settings
	const UPACS_SelectionSystemSettings* Settings = UPACS_SelectionSystemSettings::Get();
	if (!Settings || !Settings->HasValidConfiguration())
	{
		return;
	}

	// Find configuration for this character class
	const FPACS_SelectionClassConfig* Config = Settings->GetConfigForClass(CharacterClass);
	if (Config && Config->IsValid())
	{
		// Apply selection material and parameters
		DecalMaterialPath = Config->SelectionMaterial.ToSoftObjectPath();
		FieldsMask |= 0x8; // Set decal material bit

		// Apply selection parameters
		SelectionBrightness = Config->Brightness;
		SelectionColour = Config->Colour;
		FieldsMask |= 0x20; // Set selection parameters bit
	}
}

bool FPACS_NPCVisualConfig::HasSelectionConfiguration() const
{
	return (FieldsMask & 0x20) != 0; // Check if selection parameters bit is set
}

FPACS_NPCVisualConfig FPACS_NPCVisualConfig::FromGlobalSettings(const UClass* CharacterClass)
{
	FPACS_NPCVisualConfig Config;
	Config.ApplySelectionFromGlobalSettings(CharacterClass);
	return Config;
}