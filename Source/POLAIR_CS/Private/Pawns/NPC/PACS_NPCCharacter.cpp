#include "Pawns/NPC/PACS_NPCCharacter.h"
#include "Data/Configs/PACS_NPCConfig.h"
#include "Settings/PACS_SelectionSystemSettings.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Blueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
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

	// Server: Apply global selection settings after base visual config is built
	if (HasAuthority())
	{
		ApplyGlobalSelectionSettings();
	}

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

		// Apply mesh transform if specified
		if (VisualConfig.FieldsMask & 0x10)
		{
			GetMesh()->SetRelativeLocation(VisualConfig.MeshLocation);
			GetMesh()->SetRelativeRotation(VisualConfig.MeshRotation);
			GetMesh()->SetRelativeScale3D(VisualConfig.MeshScale);
		}

		// Apply loaded decal material if specified
		if (VisualConfig.FieldsMask & 0x8)
		{
			UObject* DecalMaterialObj = VisualConfig.DecalMaterialPath.TryLoad();
			if (UMaterialInterface* DecalMat = Cast<UMaterialInterface>(DecalMaterialObj))
			{
				if (CollisionDecal)
				{
					// Check if selection parameters are specified
					if (VisualConfig.FieldsMask & 0x20)
					{
						// Create dynamic material instance to apply parameters
						// Epic pattern: UMaterialInstanceDynamic::Create with outer as this actor
						UMaterialInstanceDynamic* DynamicDecalMat = UMaterialInstanceDynamic::Create(DecalMat, this);
						if (DynamicDecalMat)
						{
							// Cache for direct hover access
							CachedDecalMaterial = DynamicDecalMat;

							// Apply brightness scalar parameter
							DynamicDecalMat->SetScalarParameterValue(FName(TEXT("Brightness")), VisualConfig.SelectionBrightness);

							// Apply color vector parameter
							DynamicDecalMat->SetVectorParameterValue(FName(TEXT("Colour")), VisualConfig.SelectionColour);

							// Set the dynamic material instance on the decal
							CollisionDecal->SetDecalMaterial(DynamicDecalMat);
						}
					}
					else
					{
						// No parameters specified, use material as-is
						CollisionDecal->SetDecalMaterial(DecalMat);
					}
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

void APACS_NPCCharacter::ApplyGlobalSelectionSettings()
{
	// Epic pattern: Server authority check at start of mutating method
	if (!HasAuthority())
	{
		return;
	}

	// Apply global selection configuration to this character
	// This extends the existing VisualConfig with selection parameters
	VisualConfig.ApplySelectionFromGlobalSettings(GetClass());

	// Note: No need to call OnRep_VisualConfig() here as the replication
	// will be triggered automatically when VisualConfig is modified
	// The clients will receive the updated VisualConfig and apply the selection materials
}

void APACS_NPCCharacter::SetLocalHover(bool bHovered)
{
	if (!CachedDecalMaterial)
	{
		return;
	}

	if (bHovered)
	{
		// Safety rail: only apply hover if material is "clean" (brightness <= 0)
		float CurrentBrightness = 0.0f;
		CachedDecalMaterial->GetScalarParameterValue(FName("Brightness"), CurrentBrightness);

		if (CurrentBrightness <= 0.0f)
		{
			bIsLocallyHovered = true;
			CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.HoveredBrightness);
			CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.HoveredColour);
		}
	}
	else
	{
		// Always restore to available state on unhover
		bIsLocallyHovered = false;
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.AvailableBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.AvailableColour);
	}
}