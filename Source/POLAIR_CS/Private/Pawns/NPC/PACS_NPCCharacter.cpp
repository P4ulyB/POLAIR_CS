#include "Pawns/NPC/PACS_NPCCharacter.h"
#include "Data/Configs/PACS_NPCConfig.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/SkeletalMesh.h"
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

	// Use standard replication instead of COND_InitialOnly to avoid dormancy race condition
	DOREPLIFETIME(APACS_NPCCharacter, VisualConfig);
}

void APACS_NPCCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	if (HasAuthority())
	{
		checkf(NPCConfigAsset, TEXT("NPCConfigAsset must be set before startup"));
		BuildVisualConfigFromAsset_Server(); // fill VisualConfig here
	}
}

void APACS_NPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Client: if VisualConfig already valid, apply visuals
	if (!HasAuthority() && VisualConfig.FieldsMask != 0 && !bVisualsApplied)
	{
		ApplyVisuals_Client();
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
	if (IsRunningDedicatedServer() || !GetMesh())
	{
		return;
	}

	TArray<FSoftObjectPath> ToLoad;
	if (VisualConfig.FieldsMask & 0x1) ToLoad.Add(VisualConfig.MeshPath);
	if (VisualConfig.FieldsMask & 0x2) ToLoad.Add(VisualConfig.AnimClassPath);
	if (ToLoad.Num() == 0) return;

	auto& StreamableManager = UAssetManager::GetStreamableManager();
	AssetLoadHandle = StreamableManager.RequestAsyncLoad(ToLoad, FStreamableDelegate::CreateWeakLambda(this, [this]()
	{
		USkeletalMesh* Mesh = Cast<USkeletalMesh>(VisualConfig.MeshPath.TryLoad());
		UObject* AnimObj = VisualConfig.AnimClassPath.TryLoad();

		UClass* AnimClass = nullptr;
		if (UBlueprint* BP = Cast<UBlueprint>(AnimObj)) AnimClass = BP->GeneratedClass;
		else AnimClass = Cast<UClass>(AnimObj);

		if (Mesh) GetMesh()->SetSkeletalMesh(Mesh, /*bReinitPose*/true);
		if (AnimClass) GetMesh()->SetAnimInstanceClass(AnimClass);

		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		GetMesh()->bEnableUpdateRateOptimizations = true;

		bVisualsApplied = true;
	}));
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