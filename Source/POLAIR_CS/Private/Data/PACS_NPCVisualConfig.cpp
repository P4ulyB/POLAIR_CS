#include "Data/PACS_NPCVisualConfig.h"

bool FPACS_NPCVisualConfig::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;

	// Serialize the FieldsMask first to know what to serialize (only 2 bits needed)
	Ar.SerializeBits(&FieldsMask, 2);

	// Serialize MeshId if bit 0 is set
	if (FieldsMask & 0x01)
	{
		Ar << MeshId;
	}
	else if (Ar.IsLoading())
	{
		MeshId = FPrimaryAssetId();
	}

	// Serialize AnimBPId if bit 1 is set
	if (FieldsMask & 0x02)
	{
		Ar << AnimBPId;
	}
	else if (Ar.IsLoading())
	{
		AnimBPId = FPrimaryAssetId();
	}

	return true;
}