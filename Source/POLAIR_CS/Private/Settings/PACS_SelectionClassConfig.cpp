#include "Settings/PACS_SelectionClassConfig.h"
#include "Pawns/NPC/PACS_NPCCharacter.h"
#include "Data/PACS_NPCVisualConfig.h"
#include "Materials/MaterialInterface.h"

FPACS_SelectionClassConfig::FPACS_SelectionClassConfig()
{
	// Initialize with sensible defaults
	Brightness = 1.0f;
	Texture = 0.0f;
	Colour = 0.0f;
}

bool FPACS_SelectionClassConfig::IsValid() const
{
	// Valid if we have both a target class and selection material
	return TargetClass.ToSoftObjectPath().IsValid() &&
		   SelectionMaterial.ToSoftObjectPath().IsValid();
}

bool FPACS_SelectionClassConfig::MatchesClass(const UClass* TestClass) const
{
	if (!TestClass || !TargetClass.ToSoftObjectPath().IsValid())
	{
		return false;
	}

	// Epic pattern: Load class and check inheritance
	if (UClass* ConfiguredClass = TargetClass.LoadSynchronous())
	{
		return TestClass == ConfiguredClass || TestClass->IsChildOf(ConfiguredClass);
	}

	return false;
}

void FPACS_SelectionClassConfig::ApplyToVisualConfig(FPACS_NPCVisualConfig& VisualConfig) const
{
	if (!IsValid())
	{
		return;
	}

	// Apply selection material to visual config
	// Set the DecalMaterial field and mark it in the FieldsMask
	VisualConfig.DecalMaterialPath = SelectionMaterial.ToSoftObjectPath();
	VisualConfig.FieldsMask |= 0x8; // Set decal material bit (from existing pattern)

	// Store selection parameters for later material parameter application
	// These will be applied when the material is actually loaded on the client
}

bool FPACS_SelectionClassConfig::operator==(const FPACS_SelectionClassConfig& Other) const
{
	return TargetClass == Other.TargetClass &&
		   SelectionMaterial == Other.SelectionMaterial &&
		   FMath::IsNearlyEqual(Brightness, Other.Brightness) &&
		   FMath::IsNearlyEqual(Texture, Other.Texture) &&
		   FMath::IsNearlyEqual(Colour, Other.Colour);
}

bool FPACS_SelectionClassConfig::operator!=(const FPACS_SelectionClassConfig& Other) const
{
	return !(*this == Other);
}