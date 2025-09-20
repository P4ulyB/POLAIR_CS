#include "Pawns/NPC/PACS_NPCCharacter.h"
#include "Data/Configs/PACS_NPCConfig.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Engine/Blueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

APACS_NPCCharacter::APACS_NPCCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetNetUpdateFrequency(10.0f);

	// Disable movement for now as specified in the design
	GetCharacterMovement()->SetComponentTickEnabled(false);
}

void APACS_NPCCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(APACS_NPCCharacter, VisualConfig, COND_InitialOnly);
}

void APACS_NPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		checkf(NPCConfigAsset, TEXT("NPCConfigAsset must be set on server before BeginPlay"));
		BuildVisualConfigFromAsset_Server();
		SetNetDormancy(DORM_Initial);
	}
	else
	{
		// Client: if VisualConfig already valid (initial bunch delivered early), apply visuals
		if (VisualConfig.FieldsMask != 0 && !bVisualsApplied)
		{
			ApplyVisuals_Client();
		}
	}
}

void APACS_NPCCharacter::OnRep_VisualConfig()
{
	if (!HasAuthority() && !bVisualsApplied)
	{
		ApplyVisuals_Client();
	}
}

void APACS_NPCCharacter::ApplyVisuals_Client()
{
	if (bVisualsApplied || VisualConfig.FieldsMask == 0)
	{
		return;
	}

	// Guard against invalid mesh component
	if (!GetMesh())
	{
		ensureMsgf(false, TEXT("APACS_NPCCharacter: GetMesh() returned nullptr in ApplyVisuals_Client"));
		return;
	}

	// Prepare assets to load
	TArray<FSoftObjectPath> AssetsToLoad;

	if (VisualConfig.FieldsMask & 0x01) // Mesh
	{
		const FSoftObjectPath MeshPath = UAssetManager::Get().GetPrimaryAssetPath(VisualConfig.MeshId);
		if (MeshPath.IsValid())
		{
			AssetsToLoad.Add(MeshPath);
		}
	}

	if (VisualConfig.FieldsMask & 0x02) // AnimBP
	{
		const FSoftObjectPath AnimBPPath = UAssetManager::Get().GetPrimaryAssetPath(VisualConfig.AnimBPId);
		if (AnimBPPath.IsValid())
		{
			AssetsToLoad.Add(AnimBPPath);
		}
	}

	if (AssetsToLoad.Num() == 0)
	{
		ensureMsgf(false, TEXT("APACS_NPCCharacter: No valid assets to load for VisualConfig"));
		return;
	}

	// Start async load
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	AssetLoadHandle = StreamableManager.RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateUObject(this, &APACS_NPCCharacter::OnAssetsLoaded)
	);
}

void APACS_NPCCharacter::OnAssetsLoaded()
{
	if (bVisualsApplied || !IsValid(this))
	{
		return;
	}

	// Guard against invalid mesh component
	if (!GetMesh())
	{
		ensureMsgf(false, TEXT("APACS_NPCCharacter: GetMesh() returned nullptr"));
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	USkeletalMesh* LoadedSkeletalMesh = nullptr;
	TSubclassOf<UAnimInstance> LoadedAnimClass = nullptr;

	// Load skeletal mesh
	if (VisualConfig.FieldsMask & 0x01)
	{
		const FSoftObjectPath MeshPath = UAssetManager::Get().GetPrimaryAssetPath(VisualConfig.MeshId);
		if (MeshPath.IsValid())
		{
			LoadedSkeletalMesh = StreamableManager.LoadSynchronous<USkeletalMesh>(MeshPath);
			if (!LoadedSkeletalMesh)
			{
				ensureMsgf(false, TEXT("APACS_NPCCharacter: Failed to load SkeletalMesh for ID '%s'"), *VisualConfig.MeshId.ToString());
			}
		}
	}

	// Load animation blueprint class
	if (VisualConfig.FieldsMask & 0x02)
	{
		const FSoftObjectPath AnimBPPath = UAssetManager::Get().GetPrimaryAssetPath(VisualConfig.AnimBPId);
		if (AnimBPPath.IsValid())
		{
			// Handle both UBlueprint and direct UClass resolution
			if (UBlueprint* AnimBP = StreamableManager.LoadSynchronous<UBlueprint>(AnimBPPath))
			{
				if (AnimBP->GeneratedClass && AnimBP->GeneratedClass->IsChildOf<UAnimInstance>())
				{
					LoadedAnimClass = AnimBP->GeneratedClass;
				}
				else
				{
					ensureMsgf(false, TEXT("APACS_NPCCharacter: AnimBP '%s' does not generate a valid UAnimInstance class"), *VisualConfig.AnimBPId.ToString());
				}
			}
			else if (UClass* AnimClass = StreamableManager.LoadSynchronous<UClass>(AnimBPPath))
			{
				if (AnimClass->IsChildOf<UAnimInstance>())
				{
					LoadedAnimClass = AnimClass;
				}
				else
				{
					ensureMsgf(false, TEXT("APACS_NPCCharacter: Class '%s' is not a UAnimInstance subclass"), *VisualConfig.AnimBPId.ToString());
				}
			}
			else
			{
				ensureMsgf(false, TEXT("APACS_NPCCharacter: Failed to load AnimBP for ID '%s'"), *VisualConfig.AnimBPId.ToString());
			}
		}
	}

	// Apply visuals in the correct order: Mesh first, then AnimClass
	if (LoadedSkeletalMesh)
	{
		GetMesh()->SetSkeletalMesh(LoadedSkeletalMesh, true); // bReinitPose = true
	}

	if (LoadedAnimClass)
	{
		GetMesh()->SetAnimInstanceClass(LoadedAnimClass);
	}

	// Set client performance optimizations
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	GetMesh()->bEnableUpdateRateOptimizations = true;

	bVisualsApplied = true;
	AssetLoadHandle.Reset();
}

void APACS_NPCCharacter::BuildVisualConfigFromAsset_Server()
{
	if (!HasAuthority() || !NPCConfigAsset)
	{
		return;
	}

	// Do not load assets on dedicated server - only convert IDs
	NPCConfigAsset->ToVisualConfig(VisualConfig);

	// Optional: Apply visuals on listen server for testing
	if (!IsRunningDedicatedServer())
	{
		ApplyVisuals_Client();
	}
}