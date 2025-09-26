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

	// OPTIMIZATION: Only initialize visual configuration on clients
	// Dedicated servers don't need this data
	if (!IsRunningDedicatedServer())
	{
		// Set default configuration with engine assets
		CurrentPlaneConfig.SelectionPlaneMesh = TSoftObjectPtr<UStaticMesh>(
			FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
		CurrentPlaneConfig.SelectionMaterial = TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/Materials/M_PACS_Selection.M_PACS_Selection")));
	}
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

	// Apply transform configuration
	if (!CurrentPlaneConfig.bAutoCalculatePlaneTransform)
	{
		SelectionPlane->SetRelativeTransform(CurrentPlaneConfig.SelectionPlaneTransform);
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
	if (!SelectionPlane || bAssetsValidated)
	{
		return;
	}

	// Validate plane mesh (should already be loaded in memory for clients)
	if (!CurrentPlaneConfig.SelectionPlaneMesh.IsNull())
	{
		CachedPlaneMesh = CurrentPlaneConfig.SelectionPlaneMesh.Get();
		if (!CachedPlaneMesh)
		{
			// PERFORMANCE WARNING: Synchronous load on game thread
			// This can cause frame drops, especially problematic for VR (90 FPS requirement)
			// Assets should be pre-loaded during level load or use async loading
			TSoftObjectPtr<UStaticMesh> MeshPtr = CurrentPlaneConfig.SelectionPlaneMesh;
			CachedPlaneMesh = MeshPtr.LoadSynchronous();
			if (!CachedPlaneMesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent: Selection plane mesh not available for client"));
			}
		}

		if (CachedPlaneMesh)
		{
			SelectionPlane->SetStaticMesh(CachedPlaneMesh);
		}
	}

	// Validate selection material
	if (!CurrentPlaneConfig.SelectionMaterial.IsNull())
	{
		CachedSelectionMaterial = CurrentPlaneConfig.SelectionMaterial.Get();
		if (!CachedSelectionMaterial)
		{
			// PERFORMANCE WARNING: Synchronous load on game thread
			// This can cause frame drops, especially problematic for VR (90 FPS requirement)
			// Assets should be pre-loaded during level load or use async loading
			TSoftObjectPtr<UMaterialInterface> MatPtr = CurrentPlaneConfig.SelectionMaterial;
			CachedSelectionMaterial = MatPtr.LoadSynchronous();
			if (!CachedSelectionMaterial)
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent: Selection material not available for client"));
			}
		}

		if (CachedSelectionMaterial)
		{
			SelectionPlane->SetMaterial(0, CachedSelectionMaterial);
		}
	}

	bAssetsValidated = true;
}

void UPACS_SelectionPlaneComponent::ApplyProfile(const FSelectionVisualProfile& Profile)
{
	CurrentProfile = Profile;

	if (SelectionPlane && ShouldShowSelectionVisuals())
	{
		// Update transform based on profile settings
		CalculateSelectionPlaneTransform();

		// Update CPD with new profile values
		UpdateSelectionPlaneCPD();
	}
}

void UPACS_SelectionPlaneComponent::ApplyProfileAsset(UPACS_SelectionProfileAsset* ProfileAsset)
{
	if (!ProfileAsset)
	{
		return;
	}

	// Apply visual profile
	ApplyProfile(ProfileAsset->Profile);

	// Apply plane configuration (for non-VR clients only)
	if (ShouldShowSelectionVisuals())
	{
		ApplyPlaneConfiguration(ProfileAsset->PlaneConfig);
	}

	// Apply NPC visual representation (for ALL clients including VR)
	ApplyNPCVisualRepresentation(ProfileAsset->NPCVisual);
}

void UPACS_SelectionPlaneComponent::ApplyNPCVisualRepresentation(const FNPCVisualRepresentation& NPCVisual)
{
	// CRITICAL: Skip visual mesh application on dedicated servers
	// Servers need collision but don't need to load/apply visual representations
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// This applies the actual NPC mesh that EVERYONE sees (including VR)
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// For character NPCs
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (!NPCVisual.CharacterMesh.IsNull())
		{
			if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
			{
				// PERFORMANCE: Check if already loaded to avoid synchronous load
				USkeletalMesh* Mesh = NPCVisual.CharacterMesh.Get();
				if (!Mesh)
				{
					// WARNING: Synchronous load - should be pre-loaded or use async
					// This could cause frame drops especially in VR (90 FPS requirement)
					// TODO: Convert to async loading with FStreamableManager
					Mesh = NPCVisual.CharacterMesh.LoadSynchronous();
					UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent: Synchronous load of character mesh - consider pre-loading"));
				}

				if (Mesh)
				{
					MeshComp->SetSkeletalMesh(Mesh);
					MeshComp->SetRelativeTransform(NPCVisual.MeshTransform);
				}
			}
		}
	}
	// For static mesh NPCs (vehicles, props)
	else if (!NPCVisual.StaticMesh.IsNull())
	{
		// Find or create the main mesh component
		UStaticMeshComponent* MainMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
		if (MainMesh && MainMesh != SelectionPlane) // Make sure it's not the selection plane
		{
			// PERFORMANCE: Check if already loaded to avoid synchronous load
			UStaticMesh* Mesh = NPCVisual.StaticMesh.Get();
			if (!Mesh)
			{
				// WARNING: Synchronous load - should be pre-loaded or use async
				// This could cause frame drops especially in VR (90 FPS requirement)
				// TODO: Convert to async loading with FStreamableManager
				Mesh = NPCVisual.StaticMesh.LoadSynchronous();
				UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent: Synchronous load of static mesh - consider pre-loading"));
			}

			if (Mesh)
			{
				MainMesh->SetStaticMesh(Mesh);
				MainMesh->SetRelativeTransform(NPCVisual.MeshTransform);
			}
		}
	}

	// Apply particle effects if specified (fire, smoke, etc.)
	if (!NPCVisual.ParticleEffect.IsNull())
	{
		// This would create/update particle component
		// Implementation depends on your VFX setup
	}
}

void UPACS_SelectionPlaneComponent::ApplyPlaneConfiguration(const FSelectionPlaneConfiguration& PlaneConfig)
{
	if (!ShouldShowSelectionVisuals())
	{
		return; // Never apply plane config on VR or server
	}

	CurrentPlaneConfig = PlaneConfig;

	// Re-validate assets with new configuration
	bAssetsValidated = false;

	// If we haven't created the plane yet, it will use new config when created
	if (!SelectionPlane && !bIsInitialized)
	{
		return; // Will be applied when InitializeSelectionPlane is called
	}

	if (SelectionPlane)
	{
		ValidateAndApplyAssets();

		// Apply transform
		if (!PlaneConfig.bAutoCalculatePlaneTransform)
		{
			SelectionPlane->SetRelativeTransform(PlaneConfig.SelectionPlaneTransform);
		}
		else
		{
			CalculateSelectionPlaneTransform();
		}
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

				const float MaxDistSq = CurrentPlaneConfig.MaxRenderDistance * CurrentPlaneConfig.MaxRenderDistance;
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

	// Position plane at bottom of bounds with offset
	FVector PlaneLocation(0, 0, -BoxExtent.Z + CurrentProfile.GroundOffset);
	SelectionPlane->SetRelativeLocation(PlaneLocation);

	// Scale plane based on actor bounds and profile scale
	float ScaleMultiplier = CurrentProfile.ProjectionScale;
	float PlaneScale = FMath::Max(BoxExtent.X, BoxExtent.Y) * 2.0f * ScaleMultiplier / 100.0f;
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

	switch (DisplayState)
	{
	case 0: // Hovered
		StateColor = CurrentProfile.HoveredColor;
		StateBrightness = CurrentProfile.HoveredBrightness;
		break;
	case 1: // Selected
		StateColor = CurrentProfile.SelectedColor;
		StateBrightness = CurrentProfile.SelectedBrightness;
		break;
	case 2: // Unavailable
		StateColor = CurrentProfile.UnavailableColor;
		StateBrightness = CurrentProfile.UnavailableBrightness;
		break;
	case 3: // Available
	default:
		StateColor = CurrentProfile.AvailableColor;
		StateBrightness = CurrentProfile.AvailableBrightness;
		break;
	}

	// Set CustomPrimitiveData for the material to read
	SelectionPlane->SetCustomPrimitiveDataFloat(0, (float)DisplayState);
	SelectionPlane->SetCustomPrimitiveDataFloat(1, StateColor.R);
	SelectionPlane->SetCustomPrimitiveDataFloat(2, StateColor.G);
	SelectionPlane->SetCustomPrimitiveDataFloat(3, StateColor.B);
	SelectionPlane->SetCustomPrimitiveDataFloat(4, StateColor.A);
	SelectionPlane->SetCustomPrimitiveDataFloat(5, StateBrightness);
	SelectionPlane->SetCustomPrimitiveDataFloat(6, CurrentProfile.FadeRadius);
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