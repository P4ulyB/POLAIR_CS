#include "Components/PACS_SelectionPlaneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Data/PACS_SelectionProfile.h"

UPACS_SelectionPlaneComponent::UPACS_SelectionPlaneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// This component replicates selection state only
	SetIsReplicatedByDefault(true);
}

void UPACS_SelectionPlaneComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only initialize visuals on non-VR clients (not server, not VR)
	if (ShouldShowSelectionVisuals())
	{
		InitializeSelectionPlane();
	}
}

void UPACS_SelectionPlaneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up dynamically created selection plane
	if (SelectionPlane)
	{
		SelectionPlane->DestroyComponent();
		SelectionPlane = nullptr;
	}

	// Clear cached references
	CachedSelectionMaterial = nullptr;
	CachedPlaneMesh = nullptr;
	bAssetsValidated = false;

	Super::EndPlay(EndPlayReason);
}

void UPACS_SelectionPlaneComponent::InitializeSelectionPlane()
{
	if (bIsInitialized || !ShouldShowSelectionVisuals())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// CREATE selection plane component dynamically (client-only)
	// This ensures it NEVER exists on servers
	SelectionPlane = NewObject<UStaticMeshComponent>(Owner, TEXT("SelectionPlane"), RF_Transient);

	if (!SelectionPlane)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_SelectionPlaneComponent: Failed to create selection plane mesh component"));
		return;
	}

	// Setup attachment
	if (USceneComponent* RootComp = Owner->GetRootComponent())
	{
		SelectionPlane->SetupAttachment(RootComp);
		SelectionPlane->RegisterComponent();
	}

	// Setup the selection plane with validated assets
	SetupSelectionPlane();
	bIsInitialized = true;
}

void UPACS_SelectionPlaneComponent::SetupSelectionPlane()
{
	if (!SelectionPlane)
	{
		return;
	}

	// Validate and apply assets (client-side only)
	ValidateAndApplyAssets();

	// Configure collision for selection traces - BUT ONLY FOR NON-VR
	SelectionPlane->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectionPlane->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionPlane->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

	// Visual settings for performance
	SelectionPlane->SetCastShadow(false);
	SelectionPlane->SetReceivesDecals(false);
	SelectionPlane->bUseAsOccluder = false;
	SelectionPlane->SetGenerateOverlapEvents(false);

	// IMPORTANT: Mark as not replicated (client-only visual)
	SelectionPlane->SetIsReplicated(false);

	// Apply transform from profile if available
	if (CurrentProfileAsset)
	{
		SelectionPlane->SetRelativeTransform(CurrentProfileAsset->SelectionStaticMeshTransform);
	}
	else
	{
		CalculateSelectionPlaneTransform();
	}

	// Start hidden until selection state changes
	SelectionPlane->SetVisibility(false);

	// Set initial CPD values
	UpdateSelectionPlaneCPD();
}

void UPACS_SelectionPlaneComponent::ValidateAndApplyAssets()
{
	if (!SelectionPlane || bAssetsValidated || !CurrentProfileAsset)
	{
		return;
	}

	// Validate and apply plane mesh
	if (!CurrentProfileAsset->SelectionStaticMesh.IsNull())
	{
		CachedPlaneMesh = CurrentProfileAsset->SelectionStaticMesh.Get();
		if (!CachedPlaneMesh)
		{
			// PERFORMANCE WARNING: Synchronous load on game thread
			CachedPlaneMesh = CurrentProfileAsset->SelectionStaticMesh.LoadSynchronous();
			if (!CachedPlaneMesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent: Selection plane mesh not available"));
			}
		}

		if (CachedPlaneMesh)
		{
			SelectionPlane->SetStaticMesh(CachedPlaneMesh);
		}
	}

	// Validate and apply selection material
	if (!CurrentProfileAsset->SelectionMaterialInstance.IsNull())
	{
		CachedSelectionMaterial = CurrentProfileAsset->SelectionMaterialInstance.Get();
		if (!CachedSelectionMaterial)
		{
			// PERFORMANCE WARNING: Synchronous load on game thread
			CachedSelectionMaterial = CurrentProfileAsset->SelectionMaterialInstance.LoadSynchronous();
			if (!CachedSelectionMaterial)
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent: Selection material not available"));
			}
		}

		if (CachedSelectionMaterial)
		{
			SelectionPlane->SetMaterial(0, CachedSelectionMaterial);
		}
	}

	bAssetsValidated = true;
}


void UPACS_SelectionPlaneComponent::ApplyProfileAsset(UPACS_SelectionProfileAsset* ProfileAsset)
{
	if (!ProfileAsset)
	{
		return;
	}

	// Store the current profile
	CurrentProfileAsset = ProfileAsset;

	// Skip visual application on dedicated servers
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Apply NPC visual meshes (for ALL clients including VR)
	AActor* Owner = GetOwner();
	if (Owner)
	{
		// For character NPCs - SK asset is handled by the character class itself
		// SelectionPlaneComponent only handles selection visuals, not NPC mesh assignment

		// For static mesh NPCs (vehicles, props)
		if (!ProfileAsset->StaticMeshAsset.IsNull())
		{
			UStaticMeshComponent* MainMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
			if (MainMesh && MainMesh != SelectionPlane)
			{
				// Check if already loaded (from pre-loading or pooling)
				UStaticMesh* Mesh = ProfileAsset->StaticMeshAsset.Get();
				if (!Mesh)
				{
					// If not loaded, try synchronous load only during initialization/pooling
					// Runtime loads should use async
					Mesh = ProfileAsset->StaticMeshAsset.LoadSynchronous();
				}

				if (Mesh)
				{
					MainMesh->SetStaticMesh(Mesh);
					MainMesh->SetRelativeTransform(ProfileAsset->StaticMeshTransform);
				}
			}
		}

		// Apply particle effects if specified (fire, smoke, etc.)
		if (!ProfileAsset->ParticleEffect.IsNull())
		{
			// TODO: Implement particle component creation/update
		}
	}

	// Apply selection plane configuration (for non-VR clients only)
	if (ShouldShowSelectionVisuals() && SelectionPlane)
	{
		// Apply selection plane mesh
		if (!ProfileAsset->SelectionStaticMesh.IsNull())
		{
			UStaticMesh* PlaneMesh = ProfileAsset->SelectionStaticMesh.LoadSynchronous();
			if (PlaneMesh)
			{
				SelectionPlane->SetStaticMesh(PlaneMesh);
				SelectionPlane->SetRelativeTransform(ProfileAsset->SelectionStaticMeshTransform);
			}
		}

		// Apply selection material
		if (!ProfileAsset->SelectionMaterialInstance.IsNull())
		{
			UMaterialInterface* Material = ProfileAsset->SelectionMaterialInstance.LoadSynchronous();
			if (Material)
			{
				SelectionPlane->SetMaterial(0, Material);
			}
		}

		// Update visual state with new colors and brightness
		UpdateSelectionPlaneCPD();
	}
}


void UPACS_SelectionPlaneComponent::SetSelectionState(ESelectionVisualState NewState)
{
	// Only server can set selection state
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	SelectionState = (uint8)NewState;

	// Apply locally on server (for listen server)
	OnRep_SelectionState();
}

void UPACS_SelectionPlaneComponent::SetHoverState(bool bHovered)
{
	// Client-side only hover state
	if (!ShouldShowSelectionVisuals())
	{
		return;
	}

	LocalHoverState = bHovered ? 1 : 0;
	UpdateVisuals();
}

void UPACS_SelectionPlaneComponent::SetSelectionPlaneVisible(bool bVisible)
{
	// CRITICAL: Only show selection plane on non-VR clients
	if (!SelectionPlane || !ShouldShowSelectionVisuals())
	{
		return;
	}

	// Check render distance
	if (bVisible && GetOwner())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				const float DistanceSq = FVector::DistSquared(
					GetOwner()->GetActorLocation(),
					Pawn->GetActorLocation());

				const float MaxDist = CurrentProfileAsset ? CurrentProfileAsset->RenderDistance : 5000.0f;
				const float MaxDistSq = MaxDist * MaxDist;
				bVisible = DistanceSq <= MaxDistSq;
			}
		}
	}

	SelectionPlane->SetVisibility(bVisible);
}

bool UPACS_SelectionPlaneComponent::ShouldShowSelectionVisuals() const
{
	// Never show on dedicated server
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// CRITICAL: Never show selection plane on VR/HMD clients
	// But they WILL see the actual NPC meshes
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		return false;
	}

	// Only show on regular (flat screen) clients and listen servers
	return true;
}

void UPACS_SelectionPlaneComponent::UpdatePlanePosition()
{
	if (!SelectionPlane || !GetOwner())
	{
		return;
	}

	CalculateSelectionPlaneTransform();
}

void UPACS_SelectionPlaneComponent::CalculateSelectionPlaneTransform()
{
	if (!SelectionPlane || !GetOwner())
	{
		return;
	}

	// Calculate bounds of the actor
	FVector Origin, BoxExtent;
	GetOwner()->GetActorBounds(false, Origin, BoxExtent);

	// Position plane at bottom of bounds
	FVector PlaneLocation(0, 0, -BoxExtent.Z + 2.0f); // Default 2.0f offset
	SelectionPlane->SetRelativeLocation(PlaneLocation);

	// Scale plane based on actor bounds
	float PlaneScale = FMath::Max(BoxExtent.X, BoxExtent.Y) * 2.0f * 1.5f / 100.0f; // Default 1.5x multiplier
	SelectionPlane->SetRelativeScale3D(FVector(PlaneScale, PlaneScale, 1.0f));

	// Keep rotation facing up
	SelectionPlane->SetRelativeRotation(FRotator(-90.0f, 0, 0));
}

void UPACS_SelectionPlaneComponent::UpdateSelectionPlaneCPD()
{
	if (!SelectionPlane || !ShouldShowSelectionVisuals())
	{
		return;
	}

	// Determine effective display state (hover overrides selection)
	uint8 DisplayState = (LocalHoverState == 1) ? 0 : SelectionState;

	// Get the appropriate color and brightness based on state
	FLinearColor StateColor;
	float StateBrightness;

	// Use default values if no profile is set
	if (!CurrentProfileAsset)
	{
		// Default values
		StateColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);
		StateBrightness = 1.0f;
	}
	else
	{
		switch (DisplayState)
		{
		case 0: // Hovered
			StateColor = CurrentProfileAsset->HoveredColour;
			StateBrightness = CurrentProfileAsset->HoveredBrightness;
			break;
		case 1: // Selected
			StateColor = CurrentProfileAsset->SelectedColour;
			StateBrightness = CurrentProfileAsset->SelectedBrightness;
			break;
		case 2: // Unavailable
			StateColor = CurrentProfileAsset->UnavailableColour;
			StateBrightness = CurrentProfileAsset->UnavailableBrightness;
			break;
		case 3: // Available
		default:
			StateColor = CurrentProfileAsset->AvailableColour;
			StateBrightness = CurrentProfileAsset->AvailableBrightness;
			break;
		}
	}

	// Set CustomPrimitiveData for the material to read
	SelectionPlane->SetCustomPrimitiveDataFloat(0, (float)DisplayState);
	SelectionPlane->SetCustomPrimitiveDataFloat(1, StateColor.R);
	SelectionPlane->SetCustomPrimitiveDataFloat(2, StateColor.G);
	SelectionPlane->SetCustomPrimitiveDataFloat(3, StateColor.B);
	SelectionPlane->SetCustomPrimitiveDataFloat(4, StateColor.A);
	SelectionPlane->SetCustomPrimitiveDataFloat(5, StateBrightness);
	SelectionPlane->SetCustomPrimitiveDataFloat(6, 1.0f); // Default fade radius
	SelectionPlane->SetCustomPrimitiveDataFloat(7, 1.0f); // Fade alpha for future animation
}

void UPACS_SelectionPlaneComponent::UpdateVisuals()
{
	if (SelectionPlane && ShouldShowSelectionVisuals())
	{
		// Update visibility based on state
		uint8 DisplayState = (LocalHoverState == 1) ? 0 : SelectionState;
		SelectionPlane->SetVisibility(DisplayState != 4 && DisplayState != 3);

		// Update CPD values
		UpdateSelectionPlaneCPD();
	}
}

void UPACS_SelectionPlaneComponent::OnRep_SelectionState()
{
	// Client-side visual update based on replicated state
	UpdateVisuals();
}

USceneComponent* UPACS_SelectionPlaneComponent::GetAttachmentRoot() const
{
	if (AActor* Owner = GetOwner())
	{
		return Owner->GetRootComponent();
	}
	return nullptr;
}

void UPACS_SelectionPlaneComponent::OnAcquiredFromPool()
{
	// Reset visual state for pool acquisition
	if (ShouldShowSelectionVisuals())
	{
		// Create selection plane if it doesn't exist yet
		if (!SelectionPlane && !bIsInitialized)
		{
			InitializeSelectionPlane();
		}

		// Re-validate assets in case they were cleared
		bAssetsValidated = false;
		if (SelectionPlane)
		{
			ValidateAndApplyAssets();
		}

		// Reset to available state
		LocalHoverState = 0;
		UpdateVisuals();
	}
}

void UPACS_SelectionPlaneComponent::OnReturnedToPool()
{
	// Clear visual state for pool return
	LocalHoverState = 0;

	if (SelectionPlane)
	{
		SelectionPlane->SetVisibility(false);
		// Don't destroy the component - keep it for reuse
	}
}

void UPACS_SelectionPlaneComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPACS_SelectionPlaneComponent, SelectionState);
}