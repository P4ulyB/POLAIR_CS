#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "PACS_NPCVisualConfig.generated.h"

USTRUCT()
struct POLAIR_CS_API FPACS_NPCVisualConfig
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 FieldsMask = 0; // bit0: Mesh, bit1: Anim

	UPROPERTY()
	FSoftObjectPath MeshPath;

	UPROPERTY()
	FSoftObjectPath AnimClassPath;

	bool operator==(const FPACS_NPCVisualConfig& Other) const
	{
		return FieldsMask == Other.FieldsMask && MeshPath == Other.MeshPath && AnimClassPath == Other.AnimClassPath;
	}

	bool operator!=(const FPACS_NPCVisualConfig& Other) const
	{
		return !(*this == Other);
	}

	// Initial-only, tiny; serialise just the bits/paths
	bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
	{
		Ar.SerializeBits(&FieldsMask, 2);
		if (FieldsMask & 0x1) Ar << MeshPath;
		if (FieldsMask & 0x2) Ar << AnimClassPath;
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