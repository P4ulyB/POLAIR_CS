#include "Data/Configs/PACS_NPCConfig.h"
#include "Data/PACS_NPCVisualConfig.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#endif

void UPACS_NPCConfig::ToVisualConfig(FPACS_NPCVisualConfig& Out) const
{
	Out.MeshId = SkeletalMeshId;
	Out.AnimBPId = AnimBPId;
	Out.FieldsMask = 0;

	if (SkeletalMeshId.IsValid())
	{
		Out.FieldsMask |= 0x01; // Bit 0 for Mesh
	}

	if (AnimBPId.IsValid())
	{
		Out.FieldsMask |= 0x02; // Bit 1 for Anim
	}
}

#if WITH_EDITOR
EDataValidationResult UPACS_NPCConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!SkeletalMeshId.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("SkeletalMeshId must be set")));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		// Try to resolve the skeletal mesh asset
		const FSoftObjectPath MeshPath = UAssetManager::Get().GetPrimaryAssetPath(SkeletalMeshId);
		if (!MeshPath.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("SkeletalMeshId '%s' cannot be resolved"), *SkeletalMeshId.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (!AnimBPId.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("AnimBPId must be set")));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		// Try to resolve the animation blueprint asset
		const FSoftObjectPath AnimBPPath = UAssetManager::Get().GetPrimaryAssetPath(AnimBPId);
		if (!AnimBPPath.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("AnimBPId '%s' cannot be resolved"), *AnimBPId.ToString())));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			// Check if it's a blueprint that generates an AnimInstance class
			if (UBlueprint* AnimBP = LoadObject<UBlueprint>(nullptr, *AnimBPPath.ToString()))
			{
				if (!AnimBP->GeneratedClass || !AnimBP->GeneratedClass->IsChildOf<UAnimInstance>())
				{
					Context.AddError(FText::FromString(FString::Printf(TEXT("AnimBPId '%s' does not generate a valid UAnimInstance class"), *AnimBPId.ToString())));
					Result = EDataValidationResult::Invalid;
				}
			}
		}
	}

	return Result;
}
#endif