#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "PACS_NPCVisualConfig.generated.h"

USTRUCT()
struct POLAIR_CS_API FPACS_NPCVisualConfig
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 FieldsMask = 0; // bit0: Mesh, bit1: Anim, bit2: CollisionScale, bit3: DecalSize, bit4: DecalMaterial

	UPROPERTY()
	FSoftObjectPath MeshPath;

	UPROPERTY()
	FSoftObjectPath AnimClassPath;

	UPROPERTY()
	uint8 CollisionScaleSteps = 0;

	UPROPERTY()
	uint8 DecalSizeMultiplier = 100;

	UPROPERTY()
	FSoftObjectPath DecalMaterialPath;

	bool operator==(const FPACS_NPCVisualConfig& Other) const
	{
		return FieldsMask == Other.FieldsMask && MeshPath == Other.MeshPath && AnimClassPath == Other.AnimClassPath && CollisionScaleSteps == Other.CollisionScaleSteps && DecalSizeMultiplier == Other.DecalSizeMultiplier && DecalMaterialPath == Other.DecalMaterialPath;
	}

	bool operator!=(const FPACS_NPCVisualConfig& Other) const
	{
		return !(*this == Other);
	}

	// Initial-only, tiny; serialise just the bits/paths
	bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
	{
		Ar.SerializeBits(&FieldsMask, 5); // Now 5 bits for Mesh, Anim, CollisionScale, DecalSize, DecalMaterial
		if (FieldsMask & 0x1) Ar << MeshPath;
		if (FieldsMask & 0x2) Ar << AnimClassPath;
		if (FieldsMask & 0x4) Ar << CollisionScaleSteps;
		if (FieldsMask & 0x8) Ar << DecalSizeMultiplier;
		if (FieldsMask & 0x10) Ar << DecalMaterialPath;
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