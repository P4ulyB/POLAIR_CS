#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "PACS_NPCVisualConfig.generated.h"

USTRUCT()
struct POLAIR_CS_API FPACS_NPCVisualConfig
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 FieldsMask = 0; // bit0: Mesh, bit1: Anim, bit2: CollisionScale, bit3: DecalMaterial, bit4: MeshTransform

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

	bool operator==(const FPACS_NPCVisualConfig& Other) const
	{
		return FieldsMask == Other.FieldsMask && MeshPath == Other.MeshPath && AnimClassPath == Other.AnimClassPath && CollisionScaleSteps == Other.CollisionScaleSteps && DecalMaterialPath == Other.DecalMaterialPath && MeshLocation.Equals(Other.MeshLocation) && MeshRotation.Equals(Other.MeshRotation) && MeshScale.Equals(Other.MeshScale);
	}

	bool operator!=(const FPACS_NPCVisualConfig& Other) const
	{
		return !(*this == Other);
	}

	// Initial-only, tiny; serialise just the bits/paths
	bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
	{
		Ar.SerializeBits(&FieldsMask, 5); // Now 5 bits for Mesh, Anim, CollisionScale, DecalMaterial, MeshTransform
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