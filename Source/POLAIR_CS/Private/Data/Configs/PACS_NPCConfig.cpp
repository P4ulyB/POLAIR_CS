#include "Data/Configs/PACS_NPCConfig.h"
#include "Data/PACS_NPCVisualConfig.h"

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