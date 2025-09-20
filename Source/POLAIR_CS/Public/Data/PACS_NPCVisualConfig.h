#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Engine/AssetManager.h"
#include "PACS_NPCVisualConfig.generated.h"

USTRUCT(BlueprintType)
struct POLAIR_CS_API FPACS_NPCVisualConfig
{
	GENERATED_BODY()

	UPROPERTY()
	FPrimaryAssetId MeshId;

	UPROPERTY()
	FPrimaryAssetId AnimBPId;

	UPROPERTY()
	uint8 FieldsMask = 0;

	FPACS_NPCVisualConfig()
		: FieldsMask(0)
	{
	}

	bool operator==(const FPACS_NPCVisualConfig& Other) const
	{
		return MeshId == Other.MeshId && AnimBPId == Other.AnimBPId && FieldsMask == Other.FieldsMask;
	}

	bool operator!=(const FPACS_NPCVisualConfig& Other) const
	{
		return !(*this == Other);
	}

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
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