#include "Pawns/NPC/PACS_NPCCharacter.h"
#include "Data/Configs/PACS_NPCConfig.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Blueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"

APACS_NPCCharacter::APACS_NPCCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetNetUpdateFrequency(10.0f);

	// Disable movement for now as specified in the design
	GetCharacterMovement()->SetComponentTickEnabled(false);

	// Create collision box component
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(GetMesh());
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	CollisionBox->SetRelativeLocation(FVector::ZeroVector);

	// Create decal component nested in collision box
	CollisionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("CollisionDecal"));
	CollisionDecal->SetupAttachment(CollisionBox);
	CollisionDecal->SetRelativeLocation(FVector::ZeroVector);
	CollisionDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // Point downward by default
	CollisionDecal->DecalSize = FVector(100.0f, 100.0f, 100.0f); // Initial size, will be updated

#if UE_SERVER
	// Server collision optional - disable for performance
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionDecal->SetVisibility(false);
#endif
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
	if (VisualConfig.FieldsMask & 0x8) ToLoad.Add(VisualConfig.DecalMaterialPath);
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

		// Apply loaded decal material if specified
		if (VisualConfig.FieldsMask & 0x8)
		{
			UObject* DecalMaterialObj = VisualConfig.DecalMaterialPath.TryLoad();
			if (UMaterialInterface* DecalMat = Cast<UMaterialInterface>(DecalMaterialObj))
			{
				if (CollisionDecal)
				{
					CollisionDecal->SetDecalMaterial(DecalMat);
				}
			}
		}

		// Apply collision after mesh is loaded
		ApplyCollisionFromMesh();

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

void APACS_NPCCharacter::ApplyCollisionFromMesh()
{
	if (!GetMesh() || !GetMesh()->GetSkeletalMeshAsset() || !CollisionBox)
	{
		return;
	}

	// Get bounds from the skeletal mesh
	const FBoxSphereBounds Bounds = GetMesh()->GetSkeletalMeshAsset()->GetBounds();

	// Find the highest dimension from the box extent
	const FVector& BoxExtent = Bounds.BoxExtent;
	const float MaxDimension = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);

	// Calculate scale factor from CollisionScaleSteps (0 = 1.0, 1 = 1.1, 10 = 2.0)
	const float ScaleFactor = 1.0f + (0.1f * static_cast<float>(VisualConfig.CollisionScaleSteps));

	// Apply uniform scaled extents using the highest dimension for all sides
	const float UniformExtent = MaxDimension * ScaleFactor;
	CollisionBox->SetBoxExtent(FVector(UniformExtent, UniformExtent, UniformExtent), true);

	// Align collision box center with mesh bounds center
	CollisionBox->SetRelativeLocation(Bounds.Origin);

	// Apply same dimensions to decal if it exists
	if (CollisionDecal)
	{
		// Decal size uses the same uniform extent for all dimensions to match collision box
		CollisionDecal->DecalSize = FVector(UniformExtent, UniformExtent, UniformExtent);
	}
}