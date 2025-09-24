#include "Pawns/NPC/PACS_NPCCharacter.h"
#include "Data/Configs/PACS_NPCConfig.h"
#include "Settings/PACS_SelectionSystemSettings.h"
#include "PACS_PlayerState.h"
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
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

APACS_NPCCharacter::APACS_NPCCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetNetUpdateFrequency(10.0f);

	// Enable movement for right-click to move functionality
	GetCharacterMovement()->SetComponentTickEnabled(true);

	// Enable AI rotation toward movement direction for natural locomotion
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // Fast yaw rotation

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
	DOREPLIFETIME(APACS_NPCCharacter, CurrentSelector);
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

void APACS_NPCCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Apply visual settings earlier in initialization to prevent AI from overriding transforms
	// This runs after PreInitializeComponents (where VisualConfig is built) but before BeginPlay

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

void APACS_NPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Visual application moved to PostInitializeComponents for earlier timing
	// BeginPlay can now focus on other initialization that needs to happen after components are fully set up

	// Set up AI controller for movement
	AIControllerClass = AAIController::StaticClass();
}

void APACS_NPCCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up selection on server when NPC is destroyed
	if (HasAuthority() && CurrentSelector)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] NPC EndPlay - %s was selected by %s, cleaning up"),
			*GetName(), *CurrentSelector->GetPlayerName());

		// Clear the selector's reference to this NPC
		if (APACS_PlayerState* PS = Cast<APACS_PlayerState>(CurrentSelector))
		{
			if (PS->GetSelectedNPC() == this)
			{
				UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Clearing PlayerState selection reference"));
				PS->SetSelectedNPC(nullptr);
			}
		}

		// Clear our selector reference and push the update
		CurrentSelector = nullptr;
		ForceNetUpdate();
	}
	else if (HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] NPC EndPlay - %s had no selector"), *GetName());
	}

	Super::EndPlay(EndPlayReason);
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

			UE_LOG(LogTemp, Log, TEXT("[NPC MESH] Applied mesh transforms from data asset for %s"),
				*GetName());
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
	bIsLocallyHovered = bHovered;
	UpdateVisualState();
}

void APACS_NPCCharacter::OnRep_CurrentSelector()
{
	// Dedicated servers don't need visuals
	if (IsRunningDedicatedServer())
	{
		return;
	}

	// Update visual state based on new selection
	UpdateVisualState();
}

void APACS_NPCCharacter::UpdateVisualState()
{
	if (!CachedDecalMaterial)
	{
		return;
	}

	// Determine which visual state to apply based on priority
	EVisualPriority Priority = GetCurrentVisualPriority();

	// Apply material parameters based on priority
	switch (Priority)
	{
	case EVisualPriority::Selected:
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.SelectedBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.SelectedColour);
		break;

	case EVisualPriority::Unavailable:
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.UnavailableBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.UnavailableColour);
		break;

	case EVisualPriority::Hovered:
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.HoveredBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.HoveredColour);
		break;

	case EVisualPriority::Available:
	default:
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.AvailableBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.AvailableColour);
		break;
	}
}

APACS_NPCCharacter::EVisualPriority APACS_NPCCharacter::GetCurrentVisualPriority() const
{
	// Priority order: Selected > Unavailable > Hovered > Available

	// Get local player state to determine if we're the selector
	APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APlayerState* LocalPlayerState = LocalPC ? LocalPC->PlayerState : nullptr;

	// Check if we're selected by local player
	if (CurrentSelector && CurrentSelector == LocalPlayerState)
	{
		return EVisualPriority::Selected;
	}

	// Check if we're selected by someone else (unavailable)
	if (CurrentSelector && CurrentSelector != LocalPlayerState)
	{
		return EVisualPriority::Unavailable;
	}

	// Check if we're being hovered (local only)
	if (bIsLocallyHovered)
	{
		return EVisualPriority::Hovered;
	}

	// Default to available
	return EVisualPriority::Available;
}

void APACS_NPCCharacter::ServerMoveToLocation_Implementation(FVector TargetLocation)
{
	// Server authority check
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("[NPC MOVE] ServerMoveToLocation failed - No authority"));
		return;
	}

	// Basic validation - check if target location is reasonable
	FVector CurrentLocation = GetActorLocation();
	float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	// Reasonable distance check (prevent teleporting across map)
	if (Distance > 10000.0f) // 100 meters max
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] Target location too far: %f units"), Distance);
		return;
	}

	// Use UE5 AI movement system - this will handle pathfinding automatically
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(GetController(), TargetLocation);

	UE_LOG(LogTemp, Log, TEXT("[NPC MOVE] %s moving to location %s (Distance: %f)"),
		*GetName(), *TargetLocation.ToString(), Distance);
}