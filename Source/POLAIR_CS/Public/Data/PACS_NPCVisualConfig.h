#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "PACS_NPCVisualConfig.generated.h"

USTRUCT()
struct POLAIR_CS_API FPACS_NPCVisualConfig
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 FieldsMask = 0; // bit0: Mesh, bit1: Anim, bit2: CollisionScale, bit3: DecalMaterial, bit4: MeshTransform, bit5: SelectionParameters

	UPROPERTY()
	FSoftObjectPath MeshPath;

	UPROPERTY()
	FSoftObjectPath AnimClassPath;

	UPROPERTY()
	uint8 CollisionScaleSteps = 0;

	UPROPERTY()
	FSoftObjectPath DecalMaterialPath;

	UPROPERTY()
	FVector MeshLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator MeshRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector MeshScale = FVector::OneVector;

	// Selection system properties - ADDED AT END for safe extension (Epic pattern)
	// bit5: SelectionMaterialParameters
	// Current active parameters (applied to material instance)
	UPROPERTY()
	float SelectionBrightness = 1.0f;

	UPROPERTY()
	FLinearColor SelectionColour = FLinearColor::White;

	// State storage for runtime switching
	UPROPERTY()
	float AvailableBrightness = 1.0f;

	UPROPERTY()
	FLinearColor AvailableColour = FLinearColor::Green;

	UPROPERTY()
	float SelectedBrightness = 1.5f;

	UPROPERTY()
	FLinearColor SelectedColour = FLinearColor::Yellow;

	UPROPERTY()
	float HoveredBrightness = 2.0f;

	UPROPERTY()
	FLinearColor HoveredColour = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY()
	float UnavailableBrightness = 0.5f;

	UPROPERTY()
	FLinearColor UnavailableColour = FLinearColor::Red;

	// Current state tracking
	UPROPERTY()
	uint8 CurrentSelectionState = 0; // 0=Available, 1=Selected, 2=Hovered, 3=Unavailable

	// Selection state management
	enum class ESelectionState : uint8
	{
		Available = 0,
		Selected = 1,
		Hovered = 2,
		Unavailable = 3
	};

	void SetSelectionState(ESelectionState NewState);
	ESelectionState GetCurrentSelectionState() const;

	// Global selection system integration methods
	void ApplySelectionFromGlobalSettings(const UClass* CharacterClass);
	bool HasSelectionConfiguration() const;
	static FPACS_NPCVisualConfig FromGlobalSettings(const UClass* CharacterClass);

	bool operator==(const FPACS_NPCVisualConfig& Other) const
	{
		return FieldsMask == Other.FieldsMask && MeshPath == Other.MeshPath && AnimClassPath == Other.AnimClassPath && CollisionScaleSteps == Other.CollisionScaleSteps && DecalMaterialPath == Other.DecalMaterialPath && MeshLocation.Equals(Other.MeshLocation) && MeshRotation.Equals(Other.MeshRotation) && MeshScale.Equals(Other.MeshScale) && FMath::IsNearlyEqual(SelectionBrightness, Other.SelectionBrightness) && SelectionColour.Equals(Other.SelectionColour) && FMath::IsNearlyEqual(AvailableBrightness, Other.AvailableBrightness) && AvailableColour.Equals(Other.AvailableColour) && FMath::IsNearlyEqual(SelectedBrightness, Other.SelectedBrightness) && SelectedColour.Equals(Other.SelectedColour) && FMath::IsNearlyEqual(HoveredBrightness, Other.HoveredBrightness) && HoveredColour.Equals(Other.HoveredColour) && FMath::IsNearlyEqual(UnavailableBrightness, Other.UnavailableBrightness) && UnavailableColour.Equals(Other.UnavailableColour) && CurrentSelectionState == Other.CurrentSelectionState;
	}

	bool operator!=(const FPACS_NPCVisualConfig& Other) const
	{
		return !(*this == Other);
	}

	// Initial-only, tiny; serialise just the bits/paths
	bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
	{
		Ar.SerializeBits(&FieldsMask, 6); // Now 6 bits for Mesh, Anim, CollisionScale, DecalMaterial, MeshTransform, SelectionParameters
		if (FieldsMask & 0x1) Ar << MeshPath;
		if (FieldsMask & 0x2) Ar << AnimClassPath;
		if (FieldsMask & 0x4) Ar << CollisionScaleSteps;
		if (FieldsMask & 0x8) Ar << DecalMaterialPath;
		if (FieldsMask & 0x10)
		{
			Ar << MeshLocation;
			Ar << MeshRotation;
			Ar << MeshScale;
		}
		if (FieldsMask & 0x20)
		{
			Ar << SelectionBrightness;
			Ar << SelectionColour;
			Ar << AvailableBrightness;
			Ar << AvailableColour;
			Ar << SelectedBrightness;
			Ar << SelectedColour;
			Ar << HoveredBrightness;
			Ar << HoveredColour;
			Ar << UnavailableBrightness;
			Ar << UnavailableColour;
			Ar << CurrentSelectionState;
		}
		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FPACS_NPCVisualConfig> : public TStructOpsTypeTraitsBase2<FPACS_NPCVisualConfig>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true
	};
};